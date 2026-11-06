cmake_minimum_required(VERSION 3.22.1)
project(cybiko_android C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CORE_SOURCES
    ../core/memory.c
    ../core/hd66421.c
    ../core/timer8.c
    ../core/timer16.c
    ../core/rtc.c
    ../core/speaker.c
    ../core/keyboard.c
    ../core/h8s_cpu.c
    ../core/address_bus.c
    ../core/cfs.c
    ../core/emulator.c
)

set(ANDROID_SOURCES
    cybiko_input.c
    cybiko_audio.c
    cybiko_jni.c
)

add_library(cybiko SHARED
    ${CORE_SOURCES}
    ${ANDROID_SOURCES}
)

target_compile_options(cybiko PRIVATE -O3 -fomit-frame-pointer)

target_include_directories(cybiko PRIVATE
    ..
    ../core
    ../hal
    .
)

target_link_libraries(cybiko PRIVATE
    log
    aaudio
    jnigraphics
)
