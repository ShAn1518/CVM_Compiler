cmake_minimum_required(VERSION 3.10)
project(CVM_PlusPlus)

set(CMAKE_CXX_STANDARD 17)


include_directories(include)


add_executable(cvm src/main.cpp)
