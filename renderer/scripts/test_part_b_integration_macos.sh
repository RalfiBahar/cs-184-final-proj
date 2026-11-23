#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then echo "This integration build script targets macOS." >&2; exit 1; fi
if ! grep -q "Volume::direct_volume_lighting" src/pathtracer/volume_integrator.cpp; then echo "Apply docs/part_b_integration.patch from the repository root first." >&2; exit 1; fi
mkdir -p out/verify-integration
clang++ -std=c++14 -O2 -Wno-deprecated-declarations -Wl,-dead_strip \
 -I src/imgui -I src/imgui/backends -I src -I CGL/include -I CGL/include/CGL -I CGL/deps/glew/include -I CGL/deps/glfw/include \
 tests/part_b_integration_tests.cpp \
 src/pathtracer/pathtracer.cpp src/pathtracer/volume_integrator.cpp \
 src/pathtracer/bsdf.cpp src/pathtracer/advanced_bsdf.cpp src/pathtracer/sampler.cpp \
 src/pathtracer/camera.cpp src/pathtracer/camera_lens.cpp \
 src/scene/bbox.cpp src/scene/bvh.cpp src/scene/object.cpp src/scene/sphere.cpp \
 src/scene/triangle.cpp src/scene/light.cpp src/scene/environment_light.cpp \
 src/volume/medium.cpp src/util/lodepng.cpp src/util/sphere_drawing.cpp src/application/visual_debugger.cpp src/imgui/imgui.cpp src/imgui/imgui_draw.cpp src/imgui/imgui_tables.cpp src/imgui/imgui_widgets.cpp \
 CGL/src/vector2D.cpp CGL/src/vector3D.cpp CGL/src/vector4D.cpp \
 CGL/src/matrix3x3.cpp CGL/src/matrix4x4.cpp CGL/src/color.cpp \
 -framework OpenGL -o out/verify-integration/part_b_integration_tests
out/verify-integration/part_b_integration_tests out/verify-integration
