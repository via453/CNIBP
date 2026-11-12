#!/usr/bin/env python
"""
ABPImputation Sensor Bridge
============================
Reads ECG + PPG from an STM32 microcontroller over serial, processes the
signals, synthesises the multi-channel model input, runs the ABPImputation
V-Net model, and prints estimated systolic / diastolic / mean arterial
pressure.

Usage examples
--------------
Live capture from sensor::

    python abp_sensor_bridge.py --port COM37 --nibp-sys 120 --nibp-dias 80

Offline test with simulated data::

    python -c "from abp_sensor_bridge import process_sensor_data, generate_sim_data; \\
               ecg, ppg = generate_sim_data(); \\
               sbp, dbp, m, _ = process_sensor_data(ecg, ppg, 120, 80); \\
               print(f'SBP={sbp:.1f} DBP={dbp:.1f} MAP={m:.1f}')"
"""
from __future__ import annotations

import argparse
import os
import sys
import time
from typing import Optional

import numpy as np

# ---------------------------------------------------------------------------
# Ensure project root is on sys.path so local imports resolve
# ---------------------------------------------------------------------------
_PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

# ---------------------------------------------------------------------------
# Signal processing helpers (Task 6)
# ---------------------------------------------------------------------------
from signal_processing import (          # noqa: E402
    parse_ecg_packet,
    parse_ppg_packet,
    resample_ecg,
    detect_r_peaks,
    detect_ppg_peaks,
    calculate_pat,
    calculate_hr_from_ecg,
)

# ---------------------------------------------------------------------------
# ABPImputation project configs (for sample_freq, column indices, etc.)
# ---------------------------------------------------------------------------
import abpimputation.project_configs as project_configs  # noqa: E402

# ---------------------------------------------------------------------------
# Sampling constants
# ---------------------------------------------------------------------------
ECG_FS = 250          # Hz — native ECG sample rate from ADS1292R
PPG_FS = 100          # Hz — native PPG sample rate from MAX30102
TARGET_FS = 100       # Hz — model operating sample rate
WINDOW_SEC = 32       # seconds — model input window length
WINDOW_SAMPLES = int(WINDOW_SEC * TARGET_FS)  # 3200


# ═══════════════════════════════════════════════════════════════════════════
# A) Serial data capture
# ═══════════════════════════════════════════════════════════════════════════

def capture_sensor_data(
    port: str = "COM37",
    duration: int = WINDOW_SEC,
    baudrate: int = 115200,
) -> tuple[np.ndarray, np.ndarray]:
    """Capture *duration* seconds of ECG + PPG data from STM32 serial.

    Returns:
        (ecg_raw, ppg_raw) — raw sample arrays at native rates
        ecg_raw: ~ECG_FS * duration samples (µV)
        ppg_raw: ~PPG_FS * duration samples (IR counts)
    """
    import serial  # lazy — only needed for live capture

    ecg_target = int(ECG_FS * duration)
    ppg_target = int(PPG_FS * duration)

    ecg_buf: list[int] = []
    ppg_buf: list[int] = []

    print(f"[bridge] Opening {port} @ {baudrate} baud …")
    ser = serial.Serial(port, baudrate, timeout=1)
    time.sleep(2)  # let STM32 reset

    print(f"[bridge] Capturing {duration}s of data …")
    t0 = time.time()
    timeout = duration + 10  # generous timeout

    while len(ecg_buf) < ecg_target or len(ppg_buf) < ppg_target:
        if time.time() - t0 > timeout:
            print("[bridge] WARNING: capture timeout — proceeding with partial data")
            break

        hdr = ser.read(1)
        if not hdr:
            continue

        if hdr[0] == 0xAA:
            rest = ser.read(11)
            if len(rest) == 11:
                pkt = hdr + rest
                result = parse_ecg_packet(pkt)
                if result is not None:
                    ecg_buf.append(result[0])

        elif hdr[0] == 0xBB:
            rest = ser.read(16)
            if len(rest) == 16:
                pkt = hdr + rest
                result = parse_ppg_packet(pkt)
                if result is not None:
                    ppg_buf.append(result[0])

    ser.close()

    ecg_raw = np.array(ecg_buf[:ecg_target], dtype=np.float64)
    ppg_raw = np.array(ppg_buf[:ppg_target], dtype=np.float64)
    print(f"[bridge] Captured {len(ecg_raw)} ECG + {len(ppg_raw)} PPG samples")
    return ecg_raw, ppg_raw


# ═══════════════════════════════════════════════════════════════════════════
# B) Signal processing wrapper
# ═══════════════════════════════════════════════════════════════════════════

def process_signals(
    ecg_250hz: np.ndarray,
    ppg_100hz: np.ndarray,
) -> tuple[np.ndarray, float, float, float]:
    """Resample ECG 250 → 100 Hz, detect peaks, compute HR and PAT.

    Returns:
        (ecg_100hz, hr_bpm, pat_median_ms, pat_std_ms)
    """
    # Resample ECG to match PPG rate
    ecg_100hz = resample_ecg(ecg_250hz, orig_fs=ECG_FS, target_fs=TARGET_FS)

    # Truncate both to the same length
    n = min(len(ecg_100hz), len(ppg_100hz))
    ecg_100hz = ecg_100hz[:n]
    ppg_100hz = ppg_100hz[:n]

    # Detect R-peaks and PPG peaks
    r_peaks = detect_r_peaks(ecg_100hz, fs=TARGET_FS)
    ppg_peaks = detect_ppg_peaks(ppg_100hz, fs=TARGET_FS)

    # Calculate PAT (pulse arrival time)
    pat_median, pat_std = calculate_pat(r_peaks, ppg_peaks, fs=TARGET_FS)

    # Heart rate from R-R intervals
    hr = calculate_hr_from_ecg(r_peaks, fs=TARGET_FS)

    return ecg_100hz, hr, pat_median, pat_std


# ═══════════════════════════════════════════════════════════════════════════
# C) 9-channel model input synthesis
# ═══════════════════════════════════════════════════════════════════════════

def synthesize_channels(
    ecg_100hz: np.ndarray,
    ppg_100hz: np.ndarray,
    hr: float,
    pat_median: float,
    pat_std: float,
    nibp_sys: float,
    nibp_dias: float,
    nibp_mean: float,
) -> np.ndarray:
    """Create the input array for ABPImputer.

    The underlying V-Net model expects shape ``(1, 3208, 9)`` after
    standardisation and padding.  ``ABPImputer.predict`` handles the
    feature-engineering (standardisation → waveform-feature extraction →
    padding) internally, so this function produces the **base 6-channel**
    array of shape ``(n_samples, 1, 6)`` that ``ABPImputer`` accepts:

    ======  =============================  ===========================
    Index   Channel                        Source
    ======  =============================  ===========================
    0       ECG waveform (100 Hz)          sensor / resampled
    1       PPG waveform (100 Hz)          sensor
    2       Proximity (zeros)              no intra-NIBP timing info
    3       NIBP systolic  (mmHg)          manual cuff
    4       NIBP diastolic (mmHg)          manual cuff
    5       NIBP mean      (mmHg)          manual cuff / computed
    ======  =============================  ===========================

    After ``ABPImputer._prepare_matrix`` the model also adds:

    ======  =============================  ===========================
    6       Heart rate (BPM, standardised) computed from ECG R-peaks
    7       PAT median  (log, standardised) ECG→PPG peak delay
    8       PAT std     (standardised)      PAT variability
    ======  =============================  ===========================

    Args:
        ecg_100hz: ECG signal at 100 Hz (``WINDOW_SAMPLES`` points).
        ppg_100hz: PPG signal at 100 Hz (``WINDOW_SAMPLES`` points).
        hr: Heart rate in BPM (logged, used internally by ABPImputer).
        pat_median: Median PAT in ms (logged, used internally by ABPImputer).
        pat_std: PAT std in ms (logged, used internally by ABPImputer).
        nibp_sys: Cuff systolic pressure (mmHg).
        nibp_dias: Cuff diastolic pressure (mmHg).
        nibp_mean: Cuff mean pressure (mmHg).

    Returns:
        np.ndarray of shape ``(WINDOW_SAMPLES, 1, 6)`` — ready for
        ``ABPImputer.predict()``.
    """
    n = len(ecg_100hz)
    assert len(ppg_100hz) == n, "ECG and PPG must have equal length"

    # Build the 6 base channels
    X = np.zeros((n, 6), dtype=np.float64)
    X[:, 0] = ecg_100hz
    X[:, 1] = ppg_100hz
    # channel 2 (proximity) stays zero — no NIBP timing from sensor
    X[:, 3] = nibp_sys
    X[:, 4] = nibp_dias
    X[:, 5] = nibp_mean

    # Add pseudo-time axis: (n, 1, 6)
    X = np.expand_dims(X, axis=1)
    return X


# ═══════════════════════════════════════════════════════════════════════════
# D) Blood-pressure extraction
# ═══════════════════════════════════════════════════════════════════════════

def extract_bp_values(abp_waveform: np.ndarray) -> tuple[float, float, float]:
    """Derive SBP / DBP / MAP from a predicted ABP waveform.

    Args:
        abp_waveform: 1-D array of ABP values in mmHg.

    Returns:
        (systolic, diastolic, mean) in mmHg.
    """
    sbp = float(np.max(abp_waveform))
    dbp = float(np.min(abp_waveform))
    map_val = float(np.mean(abp_waveform))
    return sbp, dbp, map_val


# ═══════════════════════════════════════════════════════════════════════════
# E) End-to-end processing (testable without serial)
# ═══════════════════════════════════════════════════════════════════════════

def process_sensor_data(
    ecg_250hz: np.ndarray,
    ppg_100hz: np.ndarray,
    nibp_sys: float,
    nibp_dias: float,
    nibp_mean: Optional[float] = None,
) -> tuple[float, float, float, np.ndarray]:
    """Full pipeline: ECG + PPG → BP values (no serial required).

    Args:
        ecg_250hz: Raw ECG at 250 Hz.
        ppg_100hz: Raw PPG at 100 Hz.
        nibp_sys: Cuff systolic (mmHg).
        nibp_dias: Cuff diastolic (mmHg).
        nibp_mean: Cuff mean (mmHg). Auto-calculated if ``None``.

    Returns:
        (sbp, dbp, map_val, abp_waveform)
    """
    if nibp_mean is None:
        nibp_mean = (nibp_sys + 2 * nibp_dias) / 3.0

    # --- signal processing ---
    ecg_100hz, hr, pat_median, pat_std = process_signals(ecg_250hz, ppg_100hz)
    print(f"[bridge] HR = {hr:.1f} bpm | PAT = {pat_median:.1f} ± {pat_std:.1f} ms")

    # --- synthesise model input ---
    X = synthesize_channels(
        ecg_100hz, ppg_100hz,
        hr, pat_median, pat_std,
        nibp_sys, nibp_dias, nibp_mean,
    )

    # --- model inference ---
    print("[bridge] Loading ABPImputer model …")
    from abpimputation.ABPImputer import ABPImputer
    abp = ABPImputer()
    print("[bridge] Running inference …")
    y_pred = abp.predict(X)            # (1, 3200) or (n_win, 3200)
    abp_waveform = y_pred.flatten()    # 1-D

    # --- extract BP values ---
    sbp, dbp, map_val = extract_bp_values(abp_waveform)
    return sbp, dbp, map_val, abp_waveform


# ═══════════════════════════════════════════════════════════════════════════
# F) Simulated data generator (for offline testing)
# ═══════════════════════════════════════════════════════════════════════════

def generate_sim_data(
    duration: int = WINDOW_SEC,
    ecg_fs: int = ECG_FS,
    ppg_fs: int = PPG_FS,
    hr_bpm: float = 72.0,
) -> tuple[np.ndarray, np.ndarray]:
    """Generate synthetic ECG (250 Hz) and PPG (100 Hz) waveforms.

    The ECG is a simple QRS-like triangle pulse train.  The PPG is a
    smoothed pulse wave derived from the same timing.

    Returns:
        (ecg_250hz, ppg_100hz)
    """
    # ECG: triangle-pulse QRS at given heart rate
    ecg_n = int(ecg_fs * duration)
    ecg = np.zeros(ecg_n, dtype=np.float64)
    beat_period = 60.0 / hr_bpm  # seconds
    beat_samples = int(beat_period * ecg_fs)
    # QRS-like triangle: 10 ms up, 10 ms down
    qrs_hw = int(0.010 * ecg_fs)  # half-width in samples
    qrs_template = np.concatenate([
        np.linspace(0, 1.0, qrs_hw + 1),
        np.linspace(1.0, 0, qrs_hw + 1)[1:],
    ])
    for beat_start in range(0, ecg_n - len(qrs_template), beat_samples):
        end = min(beat_start + len(qrs_template), ecg_n)
        ecg[beat_start:end] = qrs_template[: end - beat_start]
    # add small baseline wander + noise
    ecg += 0.05 * np.sin(2 * np.pi * 0.15 * np.arange(ecg_n) / ecg_fs)
    ecg += 0.02 * np.random.randn(ecg_n)

    # PPG: damped sinusoid pulse
    ppg_n = int(ppg_fs * duration)
    ppg = np.zeros(ppg_n, dtype=np.float64)
    beat_period_ppg = 60.0 / hr_bpm
    beat_samples_ppg = int(beat_period_ppg * ppg_fs)
    t_pulse = np.linspace(0, beat_period_ppg, beat_samples_ppg, endpoint=False)
    # single-cycle pulse shape
    pulse = np.exp(-3 * t_pulse / beat_period_ppg) * np.sin(
        np.pi * t_pulse / beat_period_ppg
    )
    for beat_start in range(0, ppg_n - len(pulse), beat_samples_ppg):
        end = min(beat_start + len(pulse), ppg_n)
        ppg[beat_start:end] = pulse[: end - beat_start]
    ppg += 0.01 * np.random.randn(ppg_n)

    return ecg, ppg


# ═══════════════════════════════════════════════════════════════════════════
# G) CLI entry-point
# ═══════════════════════════════════════════════════════════════════════════

def main() -> None:
    parser = argparse.ArgumentParser(
        description="ABPImputation Sensor Bridge — STM32 → BP estimation",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--port", default="COM37",
        help="Serial port connected to STM32",
    )
    parser.add_argument(
        "--nibp-sys", type=float, required=True,
        help="NIBP systolic pressure (mmHg)",
    )
    parser.add_argument(
        "--nibp-dias", type=float, required=True,
        help="NIBP diastolic pressure (mmHg)",
    )
    parser.add_argument(
        "--nibp-mean", type=float, default=None,
        help="NIBP mean pressure (mmHg). Auto-calculated as (sys+2*dias)/3 if omitted.",
    )
    parser.add_argument(
        "--duration", type=int, default=WINDOW_SEC,
        help="Capture duration in seconds",
    )
    parser.add_argument(
        "--output", default=None,
        help="Save predicted ABP waveform to .npy file",
    )
    args = parser.parse_args()

    # Auto-calculate MAP
    if args.nibp_mean is None:
        args.nibp_mean = (args.nibp_sys + 2 * args.nibp_dias) / 3.0

    # Capture from sensor
    ecg_250hz, ppg_100hz = capture_sensor_data(
        port=args.port, duration=args.duration,
    )

    # Run pipeline
    sbp, dbp, map_val, abp_waveform = process_sensor_data(
        ecg_250hz, ppg_100hz,
        args.nibp_sys, args.nibp_dias, args.nibp_mean,
    )

    # Display results
    print()
    print("=" * 44)
    print("  Blood Pressure Estimation Results")
    print("=" * 44)
    print(f"  SBP : {sbp:7.1f} mmHg")
    print(f"  DBP : {dbp:7.1f} mmHg")
    print(f"  MAP : {map_val:7.1f} mmHg")
    print("=" * 44)

    # Optionally save waveform
    if args.output:
        np.save(args.output, abp_waveform)
        print(f"[bridge] ABP waveform saved → {args.output}")


if __name__ == "__main__":
    main()
