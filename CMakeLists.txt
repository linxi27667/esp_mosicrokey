cmake_minimum_required(VERSION 3.20)
set(IDF_TARGET esp32s31 CACHE STRING "Target")
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/third_party/esp-claw/components/claw_modules"
    "${CMAKE_CURRENT_LIST_DIR}/third_party/esp-claw/components/common/http_reuse")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mosicrokey VERSION 0.2.4)
