cmake_minimum_required(VERSION 3.20)
project(RudeWindowFixer)

add_compile_options(/WX /W4 /permissive- /analyze)

add_executable(RudeWindowFixer WIN32 "RudeWindowFixer.c")
set_property(TARGET RudeWindowFixer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
install(TARGETS RudeWindowFixer RUNTIME)
