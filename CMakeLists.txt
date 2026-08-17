# Project setup and configuration
cmake_minimum_required(VERSION 3.28.3)

project(edge_streamer)
set(TARGET_NAME "EdgeStreamer")

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Getting Packages
find_package(PkgConfig REQUIRED)
find_package(HailoRT REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0 gobject-2.0)
find_package(OpenCV)

include_directories(include)

# Adding executables
add_executable(${TARGET_NAME} ${SOURCES})

target_include_directories(
  ${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${GSTREAMER_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(
  ${TARGET_NAME} PRIVATE
  ${GSTREAMER_LIBRARIES}
  HailoRT::libhailort
  PkgConfig::GLIB
  ${OpenCV_LIBS}
)

# CMake custom log commands
get_target_property(EXEC_DIR EdgeStreamer RUNTIME_OUTPUT_DIRECTORY)
if(NOT EXEC_DIR)
  set(EXEC_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif()
add_custom_command(
    TARGET EdgeStreamer POST_BUILD
    COMMAND
    ${CMAKE_COMMAND}
    -E echo "\nExecutable output path: $<TARGET_FILE:EdgeStreamer>"
)
message(STATUS "\nCMake configuration completed successfully.")
message(STATUS
  "\nTarget executable is: ${EXEC_DIR}/${TARGET_NAME}\n")
