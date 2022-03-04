cmake_minimum_required(VERSION 3.0.0)
project(FPGA-readBridge VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(FPGA-readBridge main.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})