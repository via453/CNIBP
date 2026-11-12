"""
Signal Processing Module for ABPImputation
ECG/PPG packet parsing, resampling, R-wave detection, PPG peak detection, PAT and HR calculation.
"""
from __future__ import annotations

import struct
import numpy as np
from scipy import signal
from scipy.signal import find_peaks


# ---------------------------------------------------------------------------
# A) ECG packet parsing (0xAA, 12 bytes)
# ---------------------------------------------------------------------------

def parse_ecg_packet(data: bytes) -> tuple | None:
    """Parse a 12-byte ECG data packet from ADS1292R.

    Packet format:
        [0xAA, 0xAA, 0xF3, 0x07, ecg_uV(4B), hr, status, cmrr, checksum]

    Returns:
        (ecg_uV: int, hr: int) on success, None on failure.
    """
    if len(data) < 12:
        return None
    if data[0] != 0xAA or data[1] != 0xAA:
        return None
    # checksum: sum of first 11 bytes & 0xFF must equal byte 11
    checksum = sum(data[:11]) & 0xFF
    if checksum != data[11]:
        return None
    # ECG microvolt value — 32-bit signed big-endian
    ecg_uV = struct.unpack('>i', bytes(data[4:8]))[0]
    hr = data[8]
    return ecg_uV, hr


# ---------------------------------------------------------------------------
# B) PPG packet parsing (0xBB, 17 bytes)
# ---------------------------------------------------------------------------

def parse_ppg_packet(data: bytes) -> tuple | None:
    """Parse a 17-byte PPG data packet from MAX30102.

    Packet format:
        [0xBB, 0xBB, 0xF2, 0x0C, IR(4B), RED(4B), timestamp(4B), checksum]

    Returns:
        (ir_value: int, red_value: int, timestamp: int) on success, None on failure.
    """
    if len(data) < 17:
        return None
    if data[0] != 0xBB or data[1] != 0xBB:
        return None
    # checksum: sum of first 16 bytes & 0xFF must equal byte 16
    checksum = sum(data[:16]) & 0xFF
    if checksum != data[16]:
        return None
    ir_value = struct.unpack('>I', bytes(data[4:8]))[0]
    red_value = struct.unpack('>I', bytes(data[8:12]))[0]
    timestamp = struct.unpack('>I', bytes(data[12:16]))[0]
    return ir_value, red_value, timestamp


# ---------------------------------------------------------------------------
# C) ECG resampling 250 Hz → 100 Hz
# ---------------------------------------------------------------------------

def resample_ecg(ecg_signal: np.ndarray, orig_fs: int = 250, target_fs: int = 100) -> np.ndarray:
    """Resample ECG signal from *orig_fs* to *target_fs* using scipy.signal.resample."""
    num_samples = int(len(ecg_signal) * target_fs / orig_fs)
    return signal.resample(ecg_signal, num_samples)


# ---------------------------------------------------------------------------
# D) R-wave detection (Pan-Tompkins-inspired)
# ---------------------------------------------------------------------------

def detect_r_peaks(ecg_signal: np.ndarray, fs: int = 100) -> np.ndarray:
    """Detect R-wave peaks in an ECG signal.

    Algorithm:
        1. Bandpass filter 5–15 Hz (QRS energy band)
        2. Derivative + squaring
        3. Moving-window integration
        4. Peak detection with adaptive threshold

    Returns:
        Array of sample indices where R-peaks were detected.
    """
    nyq = fs / 2.0

    # 1. Bandpass filter 5–15 Hz
    low = 5.0 / nyq
    high = 15.0 / nyq
    # Clamp to valid range
    low = max(low, 0.001)
    high = min(high, 0.999)
    b, a = signal.butter(4, [low, high], btype='band')
    filtered = signal.filtfilt(b, a, ecg_signal.astype(np.float64))

    # 2. Derivative
    diff = np.diff(filtered)

    # 3. Squaring
    squared = diff ** 2

    # 4. Moving-window integration (150 ms window)
    win_size = max(int(0.150 * fs), 1)
    kernel = np.ones(win_size) / win_size
    integrated = np.convolve(squared, kernel, mode='same')

    # 5. Peak detection
    min_distance = int(0.3 * fs)  # minimum 300 ms between R-peaks
    threshold = 0.4 * np.max(integrated) if np.max(integrated) > 0 else 0
    peaks, _ = find_peaks(integrated, distance=min_distance, height=threshold)

    return peaks


# ---------------------------------------------------------------------------
# E) PPG peak detection
# ---------------------------------------------------------------------------

def detect_ppg_peaks(ppg_signal: np.ndarray, fs: int = 100) -> np.ndarray:
    """Detect pulse peaks in a PPG signal.

    Algorithm:
        1. Bandpass filter 0.5–5 Hz
        2. scipy.signal.find_peaks with minimum distance 0.5 s

    Returns:
        Array of sample indices where PPG peaks were detected.
    """
    nyq = fs / 2.0

    # Bandpass filter 0.5–5 Hz
    low = 0.5 / nyq
    high = 5.0 / nyq
    low = max(low, 0.001)
    high = min(high, 0.999)
    b, a = signal.butter(3, [low, high], btype='band')
    filtered = signal.filtfilt(b, a, ppg_signal.astype(np.float64))

    # Peak detection
    min_distance = int(0.5 * fs)  # minimum 0.5 s between peaks
    prominence = 0.1 * np.std(filtered) if np.std(filtered) > 0 else 0
    peaks, _ = find_peaks(filtered, distance=min_distance, prominence=prominence)

    return peaks


# ---------------------------------------------------------------------------
# F) PAT calculation
# ---------------------------------------------------------------------------

def calculate_pat(ecg_r_peaks: np.ndarray, ppg_peaks: np.ndarray, fs: int = 100) -> tuple:
    """Calculate Pulse Arrival Time (PAT) from ECG R-peaks to PPG peaks.

    For each R-peak, the nearest PPG peak within a 100–500 ms window is used.

    Returns:
        (pat_median_ms, pat_std_ms) — median and standard deviation in milliseconds.
    """
    pat_values = []
    for r_peak in ecg_r_peaks:
        window_start = r_peak + int(0.100 * fs)  # 100 ms
        window_end = r_peak + int(0.500 * fs)    # 500 ms
        candidates = ppg_peaks[(ppg_peaks >= window_start) & (ppg_peaks <= window_end)]
        if len(candidates) > 0:
            nearest = candidates[np.argmin(np.abs(candidates - r_peak))]
            pat_ms = (nearest - r_peak) * 1000.0 / fs
            pat_values.append(pat_ms)

    if len(pat_values) > 0:
        return float(np.median(pat_values)), float(np.std(pat_values))
    return 0.0, 0.0


# ---------------------------------------------------------------------------
# G) Heart-rate calculation from ECG R-R intervals
# ---------------------------------------------------------------------------

def calculate_hr_from_ecg(ecg_r_peaks: np.ndarray, fs: int = 100) -> float:
    """Calculate heart rate (BPM) from R-R intervals.

    Returns:
        Heart rate in beats per minute, or 0.0 if fewer than 2 peaks.
    """
    if len(ecg_r_peaks) < 2:
        return 0.0
    rr_intervals = np.diff(ecg_r_peaks) / fs  # seconds
    mean_rr = np.mean(rr_intervals)
    if mean_rr <= 0:
        return 0.0
    hr = 60.0 / mean_rr
    return float(hr)
