#!/bin/bash
# Interpreter vs dynarec correctness check on a PC, using PPSSPP's headless
# runner to run the PSP build.
#
# Needs:
#   - PPSSPPHeadless (build PPSSPP with -DHEADLESS=ON -DHEADLESS_CROSS=ON)
#   - a PSP build made with -DDAEDALUS_STATE_CHECK=ON (the installed
#     DaedalusX64 folder containing EBOOT.PBP and Plugins)
#   - N64 ROMs to test (for example krom's public domain CPU tests)
#
# Usage:
#   Tools/statecheck_ppsspp.sh <PPSSPPHeadless> <DaedalusX64 folder> <frames> <out dir> rom1 [rom2 ...]
#
# For each ROM it runs the interpreter and the dynarec, keeps both dumps in
# <out dir>/<rom name>/, and prints one PASS/FAIL line per ROM using
# Tools/statecheck_compare.py. Exit code is 1 if any ROM failed.
#
# Results on PPSSPP are a good correctness signal but say nothing about speed
# on a real PSP.

set -u

if [ $# -lt 5 ]; then
	sed -n '2,22p' "$0"
	exit 2
fi

PPSSPP=$1
DAED=$2
FRAMES=$3
OUT=$4
shift 4

HERE=$(cd "$(dirname "$0")" && pwd)
COMPARE="$HERE/statecheck_compare.py"
mkdir -p "$OUT"

failed=0
printf "%-40s %-8s %s\n" "ROM" "RESULT" "DETAIL"

for rom in "$@"; do
	name=$(basename "$rom")
	name=${name%.*}
	work=$(mktemp -d)
	mkdir -p "$work/ms/PSP/GAME"
	cp -r "$DAED" "$work/ms/PSP/GAME/DaedalusX64"
	game="$work/ms/PSP/GAME/DaedalusX64"
	cp "$rom" "$game/Roms/test.n64"
	mkdir -p "$OUT/$name"

	for mode in interpreter dynarec; do
		printf 'rom=Roms/test.n64\nmode=%s\nframes=%s\n' "$mode" "$FRAMES" > "$game/statecheck.txt"
		timeout 600 "$PPSSPP" --memstick="$work/ms" --timeout-wall=540 --me-core "$game/EBOOT.PBP" > "$OUT/$name/ppsspp_$mode.log" 2>&1
		cp "$game/statecheck_$mode.txt" "$OUT/$name/" 2>/dev/null
		# Keep the perf log files too when the build has DAEDALUS_PERF_LOG
		if [ -d "$game/PerfLogs" ]; then
			mkdir -p "$OUT/$name/PerfLogs_$mode"
			cp "$game/PerfLogs/"* "$OUT/$name/PerfLogs_$mode/" 2>/dev/null
			rm -rf "$game/PerfLogs"
		fi
	done

	a="$OUT/$name/statecheck_interpreter.txt"
	b="$OUT/$name/statecheck_dynarec.txt"
	if [ ! -s "$a" ] || [ ! -s "$b" ]; then
		printf "%-40s %-8s %s\n" "$name" "ERROR" "a run produced no dump (see ppsspp_*.log)"
		failed=1
	else
		python3 "$COMPARE" "$a" "$b" > "$OUT/$name/compare.txt"
		rc=$?
		frames_a=$(grep -vc '^#' "$a")
		frames_b=$(grep -vc '^#' "$b")
		detail=$(grep -m1 -E '^(IDENTICAL|RAM differs|INCONCLUSIVE)' "$OUT/$name/compare.txt")
		if [ "$frames_a" != "$FRAMES" ] || [ "$frames_b" != "$FRAMES" ]; then
			printf "%-40s %-8s %s\n" "$name" "ERROR" "incomplete run: $frames_a / $frames_b frames"
			failed=1
		elif [ $rc -eq 3 ]; then
			printf "%-40s %-8s %s\n" "$name" "UNSURE" "$detail"
		elif [ $rc -eq 0 ]; then
			printf "%-40s %-8s %s\n" "$name" "PASS" "$detail"
		else
			printf "%-40s %-8s %s\n" "$name" "FAIL" "$detail"
			failed=1
		fi
	fi
	rm -rf "$work"
done

exit $failed
