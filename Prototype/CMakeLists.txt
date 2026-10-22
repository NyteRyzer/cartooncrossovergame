cmake_minimum_required(VERSION 3.16)
project(MultiverseToonsPrototype CXX C)

# Playable Linux prototype of Project Multiverse Toons (raylib + OpenGL 3.3).
# It shares the engine-independent gameplay rules with the Unreal project
# (Omnitrix state machine, hover physics) and plays the greybox layouts.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(RAYLIB_VERSION 5.0)
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
set(PLATFORM Desktop CACHE STRING "" FORCE)
set(OPENGL_VERSION 3.3 CACHE STRING "" FORCE)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG ${RAYLIB_VERSION}
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)

add_executable(MultiverseToons
  src/main.cpp
  src/Game.cpp
  src/GameRender.cpp
  src/GameHud.cpp
  src/Render.cpp
  src/World.cpp)

target_include_directories(MultiverseToons PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../Source/MultiverseToons/Public)
target_link_libraries(MultiverseToons PRIVATE raylib)
target_compile_options(MultiverseToons PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
# Portable binary: no dependency on the build machine's libstdc++.
target_link_options(MultiverseToons PRIVATE -static-libstdc++ -static-libgcc)
set_target_properties(MultiverseToons PROPERTIES BUILD_RPATH "$ORIGIN")

# Level data exported from the same JSON layouts the Unreal greybox builder uses.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(TARGET MultiverseToons POST_BUILD
  COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tools/export_levels.py $<TARGET_FILE_DIR:MultiverseToons>/data/levels
  COMMENT "Exporting greybox layouts")
