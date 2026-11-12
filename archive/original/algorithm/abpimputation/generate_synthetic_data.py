"""
Generate synthetic test data mimicking MIMIC preprocessed format.
Used to verify the inference pipeline without real Box data.
"""
import numpy as np
import pandas as pd

# Generate 32 seconds at 100Hz = 3200 samples
n_samples = 3200
fs = 100
t = np.arange(n_samples) / fs

# ECG: synthetic QRS complexes at 72 BPM
heart_rate = 72
beat_period = 60 / heart_rate
ecg = np.zeros(n_samples)
for beat_start in np.arange(0, t[-1], beat_period):
    idx = int(beat_start * fs)
    if idx + 10 < n_samples:
        ecg[idx:idx+10] = np.sin(np.linspace(0, np.pi, 10))

# PPG: delayed pulse wave (shifted by ~200ms = 20 samples at 100Hz)
ppg = np.roll(ecg, 20) * 0.5 + 0.5

# ABP: realistic arterial pressure waveform
# Systolic ~120, Diastolic ~80, mean ~93
abp = 80 + 40 * ppg + np.random.normal(0, 2, n_samples)

# NIBP: constant cuff measurements (simulates one measurement repeated)
nibp_sys = np.full(n_samples, 120.0)
nibp_dias = np.full(n_samples, 80.0)
nibp_mean = np.full(n_samples, 93.0)

# Create DataFrame with timestamp index
df = pd.DataFrame({
    'ekg': ecg,
    'ppg': ppg,
    'art': abp,
    'pseudo_NIBP_sys_5min': nibp_sys,
    'pseudo_NIBP_dias_5min': nibp_dias,
    'pseudo_NIBP_mean_5min': nibp_mean
})

# Save as gzipped CSV
output_path = 'G:/ABPImputation/data/synthetic_test.csv.gz'
df.to_csv(output_path, compression='gzip')
print(f"Generated {n_samples} samples, shape: {df.shape}")
print(f"Columns: {list(df.columns)}")
print(f"Saved to: {output_path}")
print(f"\nValue ranges:")
print(f"  ECG:  [{ecg.min():.3f}, {ecg.max():.3f}]")
print(f"  PPG:  [{ppg.min():.3f}, {ppg.max():.3f}]")
print(f"  ABP:  [{abp.min():.1f}, {abp.max():.1f}] mmHg")
print(f"  NIBP sys: {nibp_sys[0]:.0f}, dias: {nibp_dias[0]:.0f}, mean: {nibp_mean[0]:.0f}")
