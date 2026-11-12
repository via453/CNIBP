#!/usr/bin/env bash
# 一键构建 STM32 固件（复刻 EIDE/Keil 的参数），产物放 build/
#
# 用法：
#   bash C:/Users/Zelvia/cnibp.staging/v1/tools/build_firmware.sh
#
# 依赖：D:\keil5\ARM\ARMCC\bin\{armcc,armlink,fromelf}.exe
# 说明：源文件清单取自 .eide 生成的 build/Target 1/builder.params，
#      因此新增/删除源文件后需先在 EIDE 里构建一次以刷新该清单。

set -e

PROJ="C:/Users/Zelvia/cnibp.staging/v1/firmware/ads1292_rtos"
ARMCC="D:/keil5/ARM/ARMCC/bin/armcc.exe"
ARMLINK="D:/keil5/ARM/ARMCC/bin/armlink.exe"
FROMELF="D:/keil5/ARM/ARMCC/bin/fromelf.exe"
PARAMS="$PROJ/build/Target 1/builder.params"
SCT="$PROJ/build/Target 1/PhysioCard.sct"
OUTDIR="C:/Users/Zelvia/AppData/Local/Temp/cnibp_fw_build"

INC=(-I. -IStart -ILibrary -IUser -ITasks
     -IFreeRTOS/port/RVDS/ARM_CM3 -IFreeRTOS/include
     -I.cmsis/include "-IRTE/_Target 1")
FLAGS=(-c --apcs=interwork --cpu Cortex-M3 --li --c99 -O0 --split_sections
       --diag_suppress=1 --diag_suppress=1295 -g
       --no_depend_system_headers)

[ -f "$PARAMS" ] || { echo "缺少 $PARAMS，请先在 EIDE 里构建一次"; exit 1; }
[ -f "$SCT" ]    || { echo "缺少 $SCT"; exit 1; }

rm -rf "$OUTDIR"; mkdir -p "$OUTDIR/obj"

# 从 builder.params 提取源文件清单（去掉 JSON 外壳与 CR）
grep -oE '"[^"]+\.(c|s)"' "$PARAMS" | tr -d '"' | tr -d '\r' | sort -u > "$OUTDIR/srcs.txt"

n=0; fail=0
while read -r f; do
  [ -z "$f" ] && continue
  o="$OUTDIR/obj/$(echo "$f" | tr '/' '_' | sed 's/\.\(c\|s\)$/.o/')"
  msg=$("$ARMCC" "${FLAGS[@]}" "${INC[@]}" -D USE_STDPERIPH_DRIVER -o "$o" "$f" 2>&1) || true
  if echo "$msg" | grep -q "Error:"; then
    echo "FAIL $f"; echo "$msg" | grep "Error:" | head -4; fail=$((fail+1))
  elif echo "$msg" | grep -q "Warning"; then
    echo "WARN $f"; echo "$msg" | grep "Warning" | head -2
  fi
  n=$((n+1))
done < "$OUTDIR/srcs.txt"

[ "$fail" -eq 0 ] || { echo "编译失败：$fail 个文件"; exit 1; }
echo "编译完成：$n 个源文件，0 错误"

"$ARMLINK" --scatter "$SCT" --info sizes,totals \
  --list "$OUTDIR/build.map" --output "$OUTDIR/PhysioCard.axf" "$OUTDIR/obj"/*.o
"$FROMELF" --i32combined --output "$OUTDIR/PhysioCard.hex" "$OUTDIR/PhysioCard.axf"

cp "$OUTDIR/PhysioCard.hex" "$PROJ/build/PhysioCard_v1.hex"
cp "$OUTDIR/PhysioCard.axf" "$PROJ/build/PhysioCard_v1.axf"

echo ""
grep -E "Total RO  Size|Total RW  Size|Total ROM Size" "$OUTDIR/build.map"
echo ""
echo "产物：$PROJ/build/PhysioCard_v1.hex"
