cmake_minimum_required(VERSION 3.11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(Assignment3)

# Embed MSVC compiler debug information in objects instead of relying on the
# compiler PDB server (which can fail with C1902 in isolated build environments).
# The linker still produces the final executable PDB for debugging.
if(MSVC)
  foreach(language C CXX)
    foreach(config DEBUG RELWITHDEBINFO)
      string(REPLACE "/Zi" "/Z7" CMAKE_${language}_FLAGS_${config}
             "${CMAKE_${language}_FLAGS_${config}}")
    endforeach()
  endforeach()
endif()

#-------------------------------------------------------------------------------
# Build options
#-------------------------------------------------------------------------------
option(BUILD_DEBUG     "Build with debug settings"    OFF)
option(BUILD_DOCS      "Build documentation"          OFF)


set(BUILD_DEBUG ${BUILD_DEBUG} CACHE BOOL "Build debug" FORCE)

if (BUILD_DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
endif()

#-------------------------------------------------------------------------------
# Set target
#-------------------------------------------------------------------------------
set(APPLICATION_3_1_SOURCE

    # Scene Object & Structure
    src/scene/sphere.cpp
    src/scene/triangle.cpp
    src/scene/light.cpp
    src/scene/bvh.cpp
    src/scene/bbox.cpp

    # Pathtracer
    src/pathtracer/camera.cpp
    src/pathtracer/bsdf.cpp
    src/pathtracer/pathtracer.cpp
    src/pathtracer/volume_integrator.cpp
    src/volume/medium.cpp
    src/volume/transmittance.cpp

    # Imgui
    src/imgui/imgui.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/backends/imgui_impl_glfw.cpp
    src/imgui/backends/imgui_impl_opengl2.cpp

    src/application/pathtracer_launcher_gui.cpp
)

set(APPLICATION_3_2_SOURCE
    src/scene/object.cpp

    # Collada Parser
    src/scene/collada/collada.cpp
    src/scene/collada/camera_info.cpp
    src/scene/collada/light_info.cpp
    src/scene/collada/sphere_info.cpp
    src/scene/collada/polymesh_info.cpp
    src/scene/collada/material_info.cpp

    # Dynamic Scene
    src/scene/gl_scene/mesh.cpp
    src/scene/gl_scene/scene.cpp
    src/scene/gl_scene/sphere.cpp

    # MeshEdit
    src/util/halfEdgeMesh.cpp
    src/application/meshEdit.cpp

    src/pathtracer/sampler.cpp
    src/pathtracer/advanced_bsdf.cpp
    src/scene/environment_light.cpp
    src/pathtracer/camera_lens.cpp
    src/pathtracer/raytraced_renderer.cpp

    # misc
    src/util/sphere_drawing.cpp
    src/util/lodepng.cpp

    # Application
    src/application/application.cpp
    src/application/main.cpp
    src/application/visual_debugger.cpp

    src/imgui/imgui.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/backends/imgui_impl_glfw.cpp
    src/imgui/backends/imgui_impl_opengl2.cpp
)

set(APPLICATION_HEADERS
    # Collada Parser
    src/scene/collada/camera_info.h
    src/scene/collada/collada_info.h
    src/scene/collada/collada.h
    src/scene/collada/light_info.h
    src/scene/collada/material_info.h
    src/scene/collada/polymesh_info.h
    src/scene/collada/sphere_info.h
    # Dynamic Scene
    src/scene/gl_scene/ambient_light.h
    src/scene/gl_scene/area_light.h
    src/scene/gl_scene/directional_light.h
    src/scene/gl_scene/draw_style.h
    src/scene/gl_scene/environment_light.h
    src/scene/gl_scene/material.h
    src/scene/gl_scene/mesh_view.h
    src/scene/gl_scene/mesh.h
    src/scene/gl_scene/point_light.h
    src/scene/gl_scene/scene.h
    src/scene/gl_scene/sphere.h
    src/scene/gl_scene/spot_light.h
    # Scene Object & Structure
    src/scene/aggregate.h
    src/scene/bbox.h
    src/scene/bvh.h
    src/scene/environment_light.h
    src/scene/light.h
    src/scene/object.h
    src/scene/primitive.h
    src/scene/scene.h
    src/scene/sphere.h
    src/scene/triangle.h
    # MeshEdit
    src/util/halfEdgeMesh.h
    src/util/image.h
    src/util/mutablePriorityQueue.h
    src/util/random_util.h
    src/util/work_queue.h
    # Pathtracer
    src/pathtracer/bsdf.h
    src/pathtracer/camera.h
    src/pathtracer/intersection.h
    src/pathtracer/pathtracer.h
    src/pathtracer/ray.h
    src/pathtracer/raytraced_renderer.h
    src/pathtracer/sampler.h
    # misc
    src/util/sphere_drawing.h
    src/util/lodepng.h
    # Application
    src/application/application.h
    src/application/meshEdit.h
    src/application/renderer.h
)

if (WIN32)
    list(APPEND APPLICATION_3_1_SOURCE src/util/win32/getopt.c)
endif()

if (WIN32 OR APPLE)
  add_library(pt31 STATIC ${APPLICATION_3_1_SOURCE} ${APPLICATION_HEADERS})
else()
  add_library(pt31 SHARED ${APPLICATION_3_1_SOURCE} ${APPLICATION_HEADERS})
endif()

target_include_directories(pt31 PUBLIC src)

add_executable(pathtracer ${APPLICATION_3_2_SOURCE} ${APPLICATION_HEADERS})
target_include_directories(pathtracer PUBLIC src)

target_link_libraries(pt31 PUBLIC CGL)
target_link_libraries(pathtracer PUBLIC pt31)

set(CGL_INCLUDE_DIRS CGL/include CGL/deps/glew/include CGL/deps/glfw/include ./src/imgui ./src/imgui/backends)

#-------------------------------------------------------------------------------
# Find dependencies
#-------------------------------------------------------------------------------
add_subdirectory(CGL)
target_include_directories(pt31 PUBLIC ${CGL_INCLUDE_DIRS})
target_include_directories(pathtracer PUBLIC ${CGL_INCLUDE_DIRS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CGL_CXX_FLAGS}")

set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL REQUIRED)
target_link_libraries(pt31 PUBLIC OpenGL::GL)
target_link_libraries(pt31 PUBLIC OpenGL::GLU)

#-------------------------------------------------------------------------------
# Add subdirectories
#-------------------------------------------------------------------------------

# build documentation
if(BUILD_DOCS)
  find_package(DOXYGEN)
  if(DOXYGEN_FOUND AND BUILD_DOCS)
    add_subdirectory(docs)
  endif()
endif()

# Install settings
set(CMAKE_INSTALL_PREFIX "${Assignment1_SOURCE_DIR}/")

# All volume modules share the application's support code and compiler flags.
option(BUILD_VOLUME_TESTS "Build A/B/C/D correctness and integration tests" OFF)
if(BUILD_VOLUME_TESTS)
  enable_testing()
  set(VOLUME_TEST_SUPPORT ${APPLICATION_3_2_SOURCE})
  list(REMOVE_ITEM VOLUME_TEST_SUPPORT src/application/main.cpp)
  add_library(volume_test_support OBJECT ${VOLUME_TEST_SUPPORT})
  target_include_directories(volume_test_support PRIVATE src ${CGL_INCLUDE_DIRS})
  function(add_volume_test target source)
    add_executable(${target} ${source} $<TARGET_OBJECTS:volume_test_support>)
    target_include_directories(${target} PRIVATE src ${CGL_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE pt31)
  endfunction()
  add_volume_test(volume_tests tests/volume_tests.cpp)
  add_test(NAME volume_tests COMMAND volume_tests)
  include(tests/part_b.cmake)
  add_volume_test(part_b_integration_tests tests/part_b_integration_tests.cpp)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-results/part-b-integration")
  add_test(NAME part_b_integration_tests COMMAND part_b_integration_tests
           "${CMAKE_CURRENT_BINARY_DIR}/test-results/part-b-integration")
  add_volume_test(combined_volume_tests tests/combined_volume_tests.cpp)
  add_test(NAME combined_volume_tests COMMAND combined_volume_tests)
  # A and B's integration suite create identically named temporary sidecars.
  # Isolate working directories so ctest --parallel cannot race on those files.
  foreach(test volume_tests phase_lighting_tests part_b_integration_tests combined_volume_tests)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work/${test}")
    set_tests_properties(${test} PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work/${test}")
  endforeach()
  add_executable(transmittance_tests experiments/transmittance/eval_transmittance.cpp
                 src/volume/medium.cpp src/volume/transmittance.cpp)
  target_include_directories(transmittance_tests PRIVATE src ${CGL_INCLUDE_DIRS})
  set_property(TARGET transmittance_tests PROPERTY CXX_STANDARD 17)
  foreach(seed 184 2026)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-results/transmittance-${seed}")
    add_test(NAME transmittance_${seed} COMMAND transmittance_tests
             "${CMAKE_CURRENT_BINARY_DIR}/test-results/transmittance-${seed}" 200000 ${seed})
  endforeach()
endif()
