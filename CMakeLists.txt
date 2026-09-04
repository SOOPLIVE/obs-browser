cmake_minimum_required(VERSION 3.16...3.25)

option(ENABLE_BROWSER "Enable browser source plugin (required Chromium Embedded Framework)" OFF)

if(NOT ENABLE_BROWSER)
  target_disable(obs-browser)
  target_disable_feature(obs-browser "Browser sources are not enabled by default (set CEF_ROOT_DIR and ENABLE_BROWSER)")

  return()
endif()

find_package(CEF 95 REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)
find_package(CURL REQUIRED)

add_library(obs-browser MODULE)
add_library(OBS::browser ALIAS obs-browser)

option(ENABLE_BROWSER_PANELS "Enable Qt web browser panel support" ON)
mark_as_advanced(ENABLE_BROWSER_PANELS)

target_sources(
  obs-browser
  PRIVATE obs-browser-plugin.cpp
          obs-browser-source.cpp
          obs-browser-source.hpp
          obs-browser-source-audio.cpp
          CommonUtils.h
          browser-app.cpp
          browser-app.hpp
          browser-client.cpp
          browser-client.hpp
          browser-scheme.cpp
          browser-scheme.hpp
          browser-version.h
          source/soop-source-plugin.h
          source/chat-source-plugin.cpp
          source/chat-score-source-plugin.cpp
          source/commerce-source-plugin.cpp
          source/kbo-graphic-source-plugin.cpp
          source/football-graphic-source-plugin.cpp
          source/videoballoon-source-plugin.cpp
          source/mission-source-plugin.cpp
		  source/soop-particle-effect-source-plugin.cpp
          source/chat-mood-check-source-plugin.cpp
		  source/aimanager-source-plugin.cpp
          source/painter-source-plugin.cpp
		  source/anime-subtitle-source-plugin.cpp
          cef-headers.hpp
          deps/base64/base64.cpp
          deps/base64/base64.hpp
          deps/signal-restore.cpp
          deps/signal-restore.hpp
          deps/wide-string.cpp
          deps/wide-string.hpp
          deps/geometry_util.cc
          deps/geometry_util.h
          deps/osr_ime_handler_win.cc
          deps/osr_ime_handler_win.h)

target_include_directories(obs-browser PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps")

target_compile_features(obs-browser PRIVATE cxx_std_17)
target_link_libraries(obs-browser PRIVATE OBS::libobs OBS::frontend-api OBS::websocket-api nlohmann_json::nlohmann_json CURL::libcurl)

if(OS_WINDOWS)
  include(cmake/os-windows.cmake)
elseif(OS_MACOS)
  include(cmake/os-macos.cmake)
  include(cmake/os-macos_soop.cmake)
elseif(OS_LINUX)
  include(cmake/os-linux.cmake)
endif()

if(ENABLE_BROWSER_PANELS)
  include(cmake/feature-panels.cmake)
endif()

set_target_properties_obs(obs-browser PROPERTIES FOLDER plugins/obs-browser PREFIX "")
