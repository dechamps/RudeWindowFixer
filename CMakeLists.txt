cmake_minimum_required(VERSION 3.20)
project(RudeWindowFixer)

add_compile_options(/WX /W4 /permissive- /analyze)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(RudeWindowFixer WIN32 "RudeWindowFixer.c")
install(TARGETS RudeWindowFixer RUNTIME)
