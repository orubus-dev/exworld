cmake_minimum_required(VERSION 3.20)
project(exworld_android LANGUAGES C CXX)

if(NOT ANDROID)
  message(FATAL_ERROR "EXWORLD Android build requires the Android NDK CMake toolchain")
endif()

if(NOT DEFINED EXGINE_ROOT OR NOT EXISTS "${EXGINE_ROOT}/CMakeLists.txt")
  if(DEFINED ENV{EXGINE_ROOT} AND EXISTS "$ENV{EXGINE_ROOT}/CMakeLists.txt")
    set(EXGINE_ROOT "$ENV{EXGINE_ROOT}")
  else()
    message(FATAL_ERROR "EXGINE_ROOT must point to the checked-out EXGINE repository")
  endif()
endif()

set(EXGINE_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(EXGINE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${EXGINE_ROOT} ${CMAKE_BINARY_DIR}/exgine-core EXCLUDE_FROM_ALL)

set(EXWORLD_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

set(ANDROID_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
set(ANDROID_GLUE_SOURCE "${ANDROID_GLUE_DIR}/android_native_app_glue.c")
if(NOT EXISTS "${ANDROID_GLUE_SOURCE}")
  message(FATAL_ERROR "Android NDK native_app_glue not found: ${ANDROID_GLUE_SOURCE}")
endif()

set(EXWORLD_SOURCES
  ${EXWORLD_ROOT}/src/game.cpp
  ${EXWORLD_ROOT}/src/player.cpp
  ${EXWORLD_ROOT}/src/vehicle.cpp
  ${EXWORLD_ROOT}/src/world.cpp
  ${EXWORLD_ROOT}/src/animation_driver.cpp
  ${EXWORLD_ROOT}/src/sound_director.cpp
  ${EXWORLD_ROOT}/src/camera.cpp
  ${EXWORLD_ROOT}/src/citygen.cpp
  ${EXWORLD_ROOT}/src/wanted.cpp
  ${EXWORLD_ROOT}/src/input.cpp
  ${EXWORLD_ROOT}/src/police.cpp
  ${EXWORLD_ROOT}/src/interior.cpp
  ${EXWORLD_ROOT}/src/save.cpp
  ${EXWORLD_ROOT}/src/asset_package.cpp
  ${EXWORLD_ROOT}/src/hud.cpp
  ${EXWORLD_ROOT}/src/ui.cpp
  ${EXWORLD_ROOT}/src/visuals.cpp
)

add_library(exworld_android SHARED
  app/src/main/cpp/native_activity.cpp
  ${ANDROID_GLUE_SOURCE}
  ${EXWORLD_SOURCES}
)

target_include_directories(exworld_android PRIVATE
  ${ANDROID_GLUE_DIR}
  ${EXWORLD_ROOT}/include
  ${EXGINE_ROOT}/include
)
target_compile_features(exworld_android PRIVATE cxx_std_20)
target_compile_definitions(exworld_android PRIVATE EXWORLD_ANDROID=1)
target_link_libraries(exworld_android PRIVATE
  exgine_core android log EGL GLESv3 aaudio
)
