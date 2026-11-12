#!/usr/bin/env python3
"""串口协议体检：统计 V4 三帧的帧率与 XOR 校验通过率。

用法：
    python probe_serial.py --port COM37 --baud 115200 --duration 30

预期（v1 固件）：
    ECG ≈ 250/s, PPG ≈ 100/s, TEMP ≈ 1/s, bad ≈ 0
"""
import argparse
import time

import serial

FRAME_LEN = {0xAA: 16, 0xBB: 17, 0xCC: 5}


def main():
    ap = argparse.ArgumentParser(description="V4 串口帧体检")
    ap.add_argument("--port", default="COM37")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--duration", type=int, default=30)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=1)
    counts = {"ecg": 0, "ppg": 0, "temp": 0, "bad": 0}
    t0 = time.time()

    while time.time() - t0 < args.duration:
        h = ser.read(1)
        if not h:
            continue
        b = h[0]
        n = FRAME_LEN.get(b)
        if n is None:
            continue
        rest = ser.read(n - 1)
        if len(rest) != n - 1:
            counts["bad"] += 1
            continue
        pkt = h + rest
        c = 0
        for x in pkt[:-1]:
            c ^= x
        if c != pkt[-1]:
            counts["bad"] += 1
            continue
        if b == 0xAA:
            counts["ecg"] += 1
        elif b == 0xBB:
            counts["ppg"] += 1
        else:
            counts["temp"] += 1

    ser.close()
    d = max(args.duration, 1)
    print(
        f"ECG {counts['ecg'] / d:.1f}/s  "
        f"PPG {counts['ppg'] / d:.1f}/s  "
        f"TEMP {counts['temp'] / d:.2f}/s  "
        f"bad {counts['bad']}"
    )


if __name__ == "__main__":
    main()
