cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{DEVKITPRO})
    set(CMAKE_TOOLCHAIN_FILE
      "$ENV{DEVKITPRO}/cmake/WiiU.cmake")
  else()
    message(FATAL_ERROR
      "Please set DEVKITPRO in your environment.")
  endif()
endif()

project(CafeMP C CXX)

include(${CMAKE_TOOLCHAIN_FILE})

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra -ffunction-sections")

set(GEN_DIR ${CMAKE_BINARY_DIR}/generated)
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders)

file(MAKE_DIRECTORY ${GEN_DIR})

function(add_shader NAME FRAG_FILE)
    set(GSH_FILE ${GEN_DIR}/${NAME}.gsh)
    set(HEADER_FILE ${GEN_DIR}/${NAME}_shader.h)

    add_custom_command(
        OUTPUT
            ${GSH_FILE}
            ${HEADER_FILE}

        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/glslcompiler.elf
            -vs ${SHADER_DIR}/video.vert.glsl
            -ps ${SHADER_DIR}/${FRAG_FILE}
            -o ${GSH_FILE}

        COMMAND ${CMAKE_COMMAND}
            -DINPUT=${GSH_FILE}
            -DOUTPUT=${HEADER_FILE}
            -DSYMBOL=${NAME}_shader
            -P ${CMAKE_CURRENT_SOURCE_DIR}/bin2h.cmake

        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/glslcompiler.elf
            ${SHADER_DIR}/video.vert.glsl
            ${SHADER_DIR}/${FRAG_FILE}
    )

    set(${NAME}_GSH ${GSH_FILE} PARENT_SCOPE)
    set(${NAME}_HDR ${HEADER_FILE} PARENT_SCOPE)
endfunction()

add_shader(yuv420p yuv420p.frag.glsl)
add_shader(nv12 nv12.frag.glsl)

set(SRC_FILES
  src/main.cpp
  src/input/input_actions.cpp
  src/logger/logger.cpp
  src/settings/settings.cpp
  src/player/media_player.cpp
  src/player/photo_viewer.cpp
  src/player/pdf_viewer.cpp

  src/ui/menu.cpp
  src/ui/scene.cpp

  src/ui/scenes/scene_main_menu.cpp
  src/ui/scenes/scene_media_player.cpp
  src/ui/scenes/scene_file_browser.cpp
  src/ui/scenes/scene_photo_viewer.cpp
  src/ui/scenes/scene_pdf_viewer.cpp

  src/ui/widgets/widget_cursor.cpp
  src/ui/widgets/widget_player_hud.cpp
  src/ui/widgets/widget_sidebar.cpp
  src/ui/widgets/widget_tooltip.cpp

  src/utils/app_state.cpp
  src/utils/display.cpp
  src/utils/media_info.cpp
  src/utils/power_manager.cpp
)

set(VENDOR_FILES
  src/vendor/logger/wiiu-stdout.cpp

  src/vendor/pdf/wiiu_mupdf_hb_wrappers.c
  src/vendor/pdf/wiiu_time_utils.c

  src/vendor/ui/imgui.cpp
  src/vendor/ui/imgui_draw.cpp
  src/vendor/ui/imgui_tables.cpp
  src/vendor/ui/imgui_widgets.cpp

  src/vendor/ui/backends/imgui_impl_gx2.cpp
  src/vendor/ui/backends/imgui_impl_wiiu.cpp
)

add_executable(CafeMP
  ${SRC_FILES}
  ${VENDOR_FILES}

  ${yuv420p_HDR}
  ${nv12_HDR}
)

target_include_directories(CafeMP PRIVATE
  src
  src/vendor
  src/vendor/ui
  src/vendor/ui/backends

  ${GEN_DIR}

  $ENV{DEVKITPRO}/wut/include
  $ENV{DEVKITPRO}/portlibs/ppc/include
  $ENV{DEVKITPRO}/portlibs/wiiu/include
  $ENV{DEVKITPRO}/portlibs/wiiu/include/freetype2
)

target_compile_definitions(CafeMP PRIVATE
  IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
  DEBUG
  __WIIU__
)

target_link_libraries(CafeMP
  mupdf
  mupdf-third
  harfbuzz
  freetype
  png
  bz2
  jansson
  gif
  swresample
  avformat
  avcodec
  avutil
  SDL2
  m
  wut
)

function(wut_create_wuhb_custom target)
  cmake_parse_arguments(PARSE_ARGV 1 WUHBTOOL
    ""
    "TARGET;OUTPUT;CONTENT;NAME;SHORTNAME;AUTHOR;ICON;TVSPLASH;DRCSPLASH;BOOTSOUND"
    ""
  )

  if (NOT WUT_WUHBTOOL_EXE)
    message(FATAL_ERROR "Could not find wuhbtool: install wut-tools")
  endif()

  set(intarget "${target}")
  set(outtarget "${target}_wuhb")

  if(DEFINED WUHBTOOL_TARGET)
    set(intarget "${WUHBTOOL_TARGET}")
  endif()

  if(NOT TARGET "${intarget}")
    message(FATAL_ERROR "wut_create_wuhb: target '${intarget}' not defined")
  endif()

  get_target_property(rpl_path "${intarget}" WUT_RPL)
  get_target_property(is_rpx   "${intarget}" WUT_IS_RPX)

  if(NOT is_rpx OR NOT rpl_path)
    message(FATAL_ERROR "wut_create_wuhb: target must be a valid .rpx (use wut_create_rpx)")
  endif()

  if(DEFINED WUHBTOOL_OUTPUT)
    get_filename_component(output "${WUHBTOOL_OUTPUT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  else()
    __dkp_target_derive_name(output "${intarget}" ".wuhb")
  endif()

  set(args "${rpl_path}" "${output}")
  set(deps "${intarget}")

  if(DEFINED WUHBTOOL_CONTENT)
    if(TARGET "${WUHBTOOL_CONTENT}")
      get_target_property(folder "${WUHBTOOL_CONTENT}" DKP_ASSET_FOLDER)

      if(NOT folder)
        message(FATAL_ERROR "wut_create_wuhb: invalid asset target")
      endif()

      list(APPEND args "--content=${folder}")

      add_dependencies(${outtarget} ${WUHBTOOL_CONTENT})
    else()
      get_filename_component(content_dir "${WUHBTOOL_CONTENT}" ABSOLUTE)

      if(NOT IS_DIRECTORY "${content_dir}")
        message(FATAL_ERROR "wut_create_wuhb: content dir not found: ${content_dir}")
      endif()

      list(APPEND args "--content=${content_dir}")
    endif()
  endif()

  if(WUHBTOOL_NAME)
    list(APPEND args "--name" "${WUHBTOOL_NAME}")
  endif()

  if(WUHBTOOL_SHORTNAME)
    list(APPEND args "--short-name" "${WUHBTOOL_SHORTNAME}")
  endif()

  if(WUHBTOOL_AUTHOR)
    list(APPEND args "--author" "${WUHBTOOL_AUTHOR}")
  endif()

  foreach(opt ICON TVSPLASH DRCSPLASH)
    if(WUHBTOOL_${opt})
      get_filename_component(path "${WUHBTOOL_${opt}}" ABSOLUTE)
      string(TOLOWER "${opt}" opt_lower)
      string(REPLACE "splash" "-image" opt_flag "${opt_lower}")

      list(APPEND args "--${opt_flag}=${path}")
      list(APPEND deps "${path}")
    endif()
  endforeach()

  if(WUHBTOOL_BOOTSOUND)
    get_filename_component(bootsound "${WUHBTOOL_BOOTSOUND}" ABSOLUTE)

    if(NOT EXISTS "${bootsound}")
      message(FATAL_ERROR "Boot sound not found: ${bootsound}")
    endif()

    list(APPEND args "--boot-sound=${bootsound}")
    list(APPEND deps "${bootsound}")
  endif()

  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${WUT_WUHBTOOL_EXE}" ${args}
    DEPENDS ${deps}
    BYPRODUCTS "${output}"
    COMMENT "Building WUHB ${outtarget}"
    VERBATIM
  )

  add_custom_target(${outtarget} ALL DEPENDS "${output}")

  dkp_set_target_file(${outtarget} "${output}")
endfunction()

wut_create_rpx(CafeMP)

wut_create_wuhb_custom(CafeMP
  NAME "Café Media Player Dev"
  SHORTNAME "CaféMP Dev"
  AUTHOR "Whateveritwas"

  CONTENT content/

  ICON branding/icon-dev.png
  TVSPLASH branding/splash_tv.png
  DRCSPLASH branding/splash_drc.png
  BOOTSOUND branding/bootSound.btsnd
)
