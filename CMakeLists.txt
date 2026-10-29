cmake_minimum_required(VERSION 3.20)
project(exworld VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED EXGINE_ROOT)
  set(EXGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../exgine" CACHE PATH "Path to EXGINE source root")
endif()

if(NOT EXISTS "${EXGINE_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "EXGINE not found at ${EXGINE_ROOT}. Set -DEXGINE_ROOT=...")
endif()

if(NOT TARGET exgine_core)
  set(EXGINE_BUILD_DEMO OFF CACHE BOOL "" FORCE)
  set(EXGINE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  add_subdirectory("${EXGINE_ROOT}" "${CMAKE_BINARY_DIR}/exgine" EXCLUDE_FROM_ALL)
endif()

set(EXWORLD_SOURCES
  src/main.cpp
  src/game.cpp
  src/player.cpp
  src/vehicle.cpp
  src/world.cpp
  src/animation_driver.cpp
  src/sound_director.cpp
  src/camera.cpp
  src/citygen.cpp
  src/wanted.cpp
  src/input.cpp
  src/police.cpp
  src/interior.cpp
  src/save.cpp
  src/asset_package.cpp
  src/hud.cpp
  src/ui.cpp
  src/visuals.cpp
)

add_executable(exworld ${EXWORLD_SOURCES})

target_include_directories(exworld PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${EXGINE_ROOT}/include
)

target_link_libraries(exworld PRIVATE exgine_core)

add_custom_command(TARGET exworld POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/content
    $<TARGET_FILE_DIR:exworld>/content
)
