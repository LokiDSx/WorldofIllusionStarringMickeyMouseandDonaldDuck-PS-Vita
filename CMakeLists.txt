cmake_minimum_required(VERSION 3.20)
project(WorldOfIllusionVita CXX)

if(NOT VITA)
    message(FATAL_ERROR "This is the PS Vita port: configure with the VitaSDK toolchain.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- the Mega Drive core + World of Illusion module (from Mickey & Donald Collection) ----
add_executable(woi
    src/core/m68k.cpp
    src/core/z80.cpp
    src/core/vdp.cpp
    src/core/ym2612.cpp
    src/core/psg.cpp
    src/core/md.cpp
    src/game/common.cpp
    src/game/coi.cpp
    src/game/qs.cpp
    src/game/woi.cpp

    src/app/session.cpp
    src/app/palette.cpp
    src/app/settings.cpp
    src/app/audio_mixer.cpp
    src/app/soundtrack.cpp
    src/app/romfind.cpp
    src/app/unzip.cpp

    src/app/vita_main.cpp
    src/app/sys_vita.cpp
    src/platform/vita/audio_sdl.cpp
)
target_include_directories(woi PRIVATE src src/app src/core src/game src/platform)
target_compile_options(woi PRIVATE -Wno-narrowing)

find_package(SDL2 REQUIRED CONFIG)
include("${VITASDK}/share/vita.cmake" REQUIRED)

# vita2d (GXM) for the picture and the pause menu; SDL2 only for its audio queue.
target_link_libraries(woi PRIVATE
    SDL2::SDL2
    vita2d
    ScePgf_stub
    ScePvf_stub
    SceGxm_stub
    SceDisplay_stub
    SceCtrl_stub
    ScePower_stub
    SceAppMgr_stub
    SceSysmem_stub
    SceKernelThreadMgr_stub
)

vita_create_self(woi-eboot woi)

vita_create_vpk(WorldOfIllusion.vpk WOLI00001 woi-eboot
    VERSION 01.00
    NAME "World of Illusion"
    FILE sce_sys/icon0.png sce_sys/icon0.png
    FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
    FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
    FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
