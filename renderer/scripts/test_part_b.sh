#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_dir="${BUILD_DIR:-out/part-b}"
result_dir="${RESULT_DIR:-experiments/part_b}"
mkdir -p "$build_dir" "$result_dir"
flags=(-std=c++14 -O2 -Wno-deprecated-declarations)
if [[ "${SANITIZE:-0}" == 1 ]]; then
  flags+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer)
fi
if [[ "$(uname -s)" == Darwin ]]; then
  platform=(-framework OpenGL)
else
  platform=(-lGL)
fi
"${CXX:-c++}" "${flags[@]}" -I src -I CGL/include -I CGL/include/CGL -I CGL/deps/glew/include \
  tests/phase_lighting_tests.cpp src/scene/bvh.cpp src/scene/bbox.cpp \
  src/scene/light.cpp src/pathtracer/sampler.cpp src/volume/medium.cpp src/volume/transmittance.cpp \
  CGL/src/vector3D.cpp CGL/src/matrix3x3.cpp "${platform[@]}" \
  -o "$build_dir/phase_lighting_tests"
"$build_dir/phase_lighting_tests" "$result_dir"
