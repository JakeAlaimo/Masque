cmake_minimum_required(VERSION 3.21)

project(MasqueOblivion VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

find_package(obse64_common CONFIG REQUIRED)
find_package(xbyak CONFIG REQUIRED)
find_package(obse64 CONFIG REQUIRED)

add_library(MasqueOblivion SHARED src/Main.cpp)
target_link_libraries(MasqueOblivion PUBLIC obse64::obse64_common xbyak::xbyak obse64::obse64)