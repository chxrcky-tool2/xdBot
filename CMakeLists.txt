cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(xdBot VERSION 1.0.0)
 
add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/global.cpp
    src/macro.cpp
    src/keybinds.cpp
    src/utils/utils.cpp
    src/gdr/gdr.cpp

    src/hacks/other.cpp
    src/hacks/tps_bypass.cpp
    src/hacks/frame_stepper.cpp
    src/hacks/layout_mode.cpp
    src/hacks/show_trajectory.cpp
    src/hacks/coin_finder.cpp
    src/hacks/clickbot.cpp
    src/hacks/autoclicker.cpp

    src/ui/macro_editor.cpp
    src/ui/button_edit_layer.cpp
    src/ui/game_ui.cpp
    src/ui/xd_menu.cpp
    src/ui/load_macro_layer.cpp
    src/ui/clickbot_layer.cpp
    src/ui/button_setting.cpp
    src/ui/imgui_touch.cpp
    src/ui/xd_touch.cpp

    src/practice_fixes/input.cpp
    src/practice_fixes/player.cpp
    src/practice_fixes/play_layer.cpp
)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
target_compile_options(${PROJECT_NAME} PRIVATE -ferror-limit=0)

# ImGui overlay (replaces the old cocos2d menu).
# imgui-cocos is built as a static lib and linked into the mod. I will pin a <commit> once I picked a known-good one.
CPMAddPackage("gh:matcool/gd-imgui-cocos#geode")
target_link_libraries(${PROJECT_NAME} imgui-cocos)

# --- Patch imgui-cocos for Geode 6 cocos delegate signatures ---------------------------------
# Geode 6 changed: keyDown/keyUp take (key, double), insertText takes (text, len, key),
# scrollWheel returns bool. Wrap the old implementations so the overrides match.
set(_ic_dir "${gd-imgui-cocos_SOURCE_DIR}")
if (NOT _ic_dir)
    set(_ic_dir "${imgui-cocos_SOURCE_DIR}")
endif()
file(GLOB_RECURSE _ic_files "${_ic_dir}/*.hpp" "${_ic_dir}/*.cpp")
foreach(_f ${_ic_files})
    file(READ "${_f}" _c)
    if (_c MATCHES "enumKeyCodes key\\) override" OR _c MATCHES "scrollWheel" OR _c MATCHES "insertText")
        if (NOT _c MATCHES "XD_PATCHED")
            set(_n "${_c}")
            # header declarations -> inline wrappers calling *Impl
            string(REGEX REPLACE "void keyUp\\(cocos2d::enumKeyCodes key\\) override;"
                "void keyUp(cocos2d::enumKeyCodes key, double) override { keyUpImpl(key); } void keyUpImpl(cocos2d::enumKeyCodes key); /*XD_PATCHED*/" _n "${_n}")
            string(REGEX REPLACE "void keyDown\\(cocos2d::enumKeyCodes key\\) override;"
                "void keyDown(cocos2d::enumKeyCodes key, double) override { keyDownImpl(key); } void keyDownImpl(cocos2d::enumKeyCodes key); /*XD_PATCHED*/" _n "${_n}")
            string(REGEX REPLACE "void insertText\\(const char\\* text, int len\\) override;"
                "void insertText(const char* text, int len, cocos2d::enumKeyCodes) override { insertTextImpl(text, len); } void insertTextImpl(const char* text, int len); /*XD_PATCHED*/" _n "${_n}")
            string(REGEX REPLACE "void scrollWheel\\(float dy, float dx\\) override;"
                "bool scrollWheel(float dy, float dx) override { scrollWheelImpl(dy, dx); return false; } void scrollWheelImpl(float dy, float dx); /*XD_PATCHED*/" _n "${_n}")
            # cpp definitions -> *Impl
            string(REGEX REPLACE "::keyUp\\(cocos2d::enumKeyCodes key\\)" "::keyUpImpl(cocos2d::enumKeyCodes key)" _n "${_n}")
            string(REGEX REPLACE "::keyDown\\(cocos2d::enumKeyCodes key\\)" "::keyDownImpl(cocos2d::enumKeyCodes key)" _n "${_n}")
            string(REGEX REPLACE "::insertText\\(const char\\* text, int len\\)" "::insertTextImpl(const char* text, int len)" _n "${_n}")
            string(REGEX REPLACE "void ([A-Za-z_]+)::scrollWheel\\(float dy, float dx\\)" "void \\1::scrollWheelImpl(float dy, float dx)" _n "${_n}")
            if (NOT _n STREQUAL _c)
                file(WRITE "${_f}" "${_n}")
            endif()
        endif()
    endif()
endforeach()
