cmake_minimum_required(VERSION 3.0.0)
project(FPGA-status VERSION 0.1.0)

add_executable(FPGA-reset main.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})