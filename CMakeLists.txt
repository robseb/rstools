cmake_minimum_required(VERSION 3.10)
project(rstools)

set(CMAKE_CXX_STANDARD 17)

include_directories("${CMAKE_SOURCE_DIR}/FPGA-status")    
include_directories("${CMAKE_SOURCE_DIR}/FPGA-readBridge")  
include_directories("${CMAKE_SOURCE_DIR}/FPGA-writeBridge") 
include_directories("${CMAKE_SOURCE_DIR}/FPGA-dumpBridge") 
include_directories("${CMAKE_SOURCE_DIR}/FPGA-reset") 
include_directories("${CMAKE_SOURCE_DIR}/FPGA-writeConfig")  