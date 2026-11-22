#!/usr/bin/env bash
# Build and run the transmittance evaluation (Owner D). Needs only a C++17
# compiler and python3 with matplotlib; no OpenGL/GLFW/vcpkg and no full renderer build.
#
# Usage (from anywhere):  renderer/experiments/transmittance/run.sh [trials] [base_seed]
# Output: renderer/experiments/transmittance/results/{transmittance.csv,timing.csv,
#         transmittance.png,run_info.txt}
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RENDERER="$(cd "$HERE/../.." && pwd)"
TRIALS="${1:-200000}"
SEED="${2:-184}"
OUT="$HERE/results"
BIN="$OUT/eval_transmittance"
CXX="${CXX:-c++}"
mkdir -p "$OUT"

# Deliberately skips src/scene/bbox.cpp (needs GLEW); the volume code only uses
# the header-only parts of BBox/Ray/Vector3D.
"$CXX" -std=c++17 -O2 -I "$RENDERER/src" -I "$RENDERER/CGL/include" \
  "$HERE/eval_transmittance.cpp" \
  "$RENDERER/src/volume/medium.cpp" \
  "$RENDERER/src/volume/transmittance.cpp" \
  -o "$BIN"

"$BIN" "$OUT" "$TRIALS" "$SEED" | tee "$OUT/console.txt"

COMMIT="$(git -C "$RENDERER" rev-parse --short HEAD 2>/dev/null || echo unknown)"
DIRTY="$(git -C "$RENDERER" status --porcelain 2>/dev/null | head -1 | sed 's/.*/ (uncommitted changes)/')"
{
  echo "date: $(date '+%Y-%m-%d %H:%M:%S %Z')"
  echo "commit: ${COMMIT}${DIRTY}"
  echo "os: $(uname -srm)"
  echo "cpu: $(sysctl -n machdep.cpu.brand_string 2>/dev/null || grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2- || echo unknown)"
  echo "compiler: $("$CXX" --version | head -1)"
  echo "flags: -std=c++17 -O2"
  echo "threads: 1"
  echo "trials_per_fixture: $TRIALS"
  echo "base_seed: $SEED"
} > "$OUT/run_info.txt"

python3 "$HERE/plot_transmittance.py" "$OUT/transmittance.csv" "$OUT/transmittance.png"
echo "Results in $OUT"
