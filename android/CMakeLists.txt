cmake_minimum_required(VERSION 3.22.1)
project(wmmt C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(SRC_DIR "${ROOT_DIR}/src")

add_library(phys_rc OBJECT
    "${SRC_DIR}/phys/phys_rc.c"
)
target_include_directories(phys_rc PRIVATE "${SRC_DIR}/phys")
target_compile_definitions(phys_rc PRIVATE RC_FPT=double)
target_compile_options(phys_rc PRIVATE -O2 -ffp-contract=off -w)

add_library(wmmt SHARED
    "${SRC_DIR}/main_android.c"
    "${SRC_DIR}/app.c"
    "${SRC_DIR}/render.c"
    "${SRC_DIR}/hud.c"
    "${SRC_DIR}/car.c"
    "${SRC_DIR}/collide.c"
    "${SRC_DIR}/sound.c"
    "${SRC_DIR}/anm.c"
    "${SRC_DIR}/menu.c"
    "${SRC_DIR}/lights.c"
    "${SRC_DIR}/spark.c"
    "${SRC_DIR}/net.c"
    "${SRC_DIR}/vs_host.c"
    "${SRC_DIR}/title.c"
    "${SRC_DIR}/gk.c"
    "${SRC_DIR}/phys/rc_runtime.c"
    $<TARGET_OBJECTS:phys_rc>
)

target_include_directories(wmmt PRIVATE "${SRC_DIR}" "${SRC_DIR}/phys")
target_compile_definitions(wmmt PRIVATE WMMT_EGL)
target_compile_options(wmmt PRIVATE
    -O2
    -Wall
    -Wno-unused-function
    -ffp-contract=off
)

target_link_libraries(wmmt PRIVATE
    android
    EGL
    GLESv2
    OpenSLES
    log
    z
    m
)

target_link_options(wmmt PRIVATE -Wl,-z,max-page-size=16384)
