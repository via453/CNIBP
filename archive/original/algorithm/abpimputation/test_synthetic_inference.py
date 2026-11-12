"""
Run inference test on synthetic data to verify the full pipeline.
"""
import sys
import os
import pandas as pd
import numpy as np

# Add project root to path
sys.path.insert(0, 'G:/ABPImputation')

from abpimputation.ABPImputer import ABPImputer
from abpimputation.preprocessing.preprocess import preprocess

print("=" * 60)
print("SYNTHETIC DATA INFERENCE TEST")
print("=" * 60)

# Load synthetic data
data = pd.read_csv('G:/ABPImputation/data/synthetic_test.csv.gz', index_col=0)
print(f"\n1. Loaded data shape: {data.shape}")
print(f"   Columns: {list(data.columns)}")
print(f"   Value ranges:")
for col in data.columns:
    print(f"     {col}: [{data[col].min():.3f}, {data[col].max():.3f}]")

# Preprocess
print("\n2. Running preprocess()...")
preprocessed = preprocess(data)
print(f"   Preprocessed shape: {preprocessed.shape}")
print(f"   Columns: {list(preprocessed.columns)}")

# Split features and target
y_true = preprocessed["art"]
X = preprocessed.iloc[:, :-1]
X = np.expand_dims(X, axis=1)
print(f"\n3. Input shape for model: {X.shape}")
print(f"   y_true shape: {y_true.shape}")

# Run inference
print("\n4. Running ABPImputer.predict()...")
abp = ABPImputer()
y_pred = abp.predict(X)

print(f"\n5. Results:")
print(f"   Prediction shape: {y_pred.shape}")
print(f"   SBP (max): {y_pred.max():.1f} mmHg")
print(f"   DBP (min): {y_pred.min():.1f} mmHg")
print(f"   Mean ABP: {y_pred.mean():.1f} mmHg")
print(f"   True ABP range: [{y_true.min():.1f}, {y_true.max():.1f}] mmHg")

# Verify physiological plausibility
sbp = y_pred.max()
dbp = y_pred.min()
mean_abp = y_pred.mean()

checks = []
checks.append(("SBP in range (80-200)", 80 <= sbp <= 200))
checks.append(("DBP in range (40-150)", 40 <= dbp <= 150))
checks.append(("Mean ABP in range (60-160)", 60 <= mean_abp <= 160))
checks.append(("SBP > DBP", sbp > dbp))
checks.append(("Shape correct", y_pred.shape == (1, 3200)))

print("\n6. Plausibility checks:")
all_pass = True
for name, passed in checks:
    status = "PASS" if passed else "FAIL"
    print(f"   {status}: {name}")
    if not passed:
        all_pass = False

print(f"\n{'ALL CHECKS PASSED' if all_pass else 'SOME CHECKS FAILED'}")
print("=" * 60)
