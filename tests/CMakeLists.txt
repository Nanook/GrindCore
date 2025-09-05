# CMakeLists.txt for tests
cmake_minimum_required(VERSION 3.20)
project(GrindCoreTests C)

add_executable(lzma_test lzma_test.c)
target_include_directories(lzma_test PRIVATE ../libs/GrindCore ../external/sevenzip/lzma)
target_link_libraries(lzma_test PRIVATE GrindCore)
