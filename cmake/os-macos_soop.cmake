set(helper_basename browser-helper)
set(helper_output_name_soop "SOOPStudio Helper") # 이름 변경 필요할 수 있음 - Davin
set(helper_suffixes "::" " (GPU):_gpu:.gpu" " (Plugin):_plugin:.plugin" " (Renderer):_renderer:.renderer")

foreach(helper IN LISTS helper_suffixes)
  string(REPLACE ":" ";" helper ${helper})
  list(GET helper 0 helper_name)
  list(GET helper 1 helper_target)
  list(GET helper 2 helper_plist)

  set(_soop "_soop")
  set(target_name_soop ${helper_basename}${helper_target}${_soop})
  set(target_output_name_soop "${helper_output_name_soop}${helper_name}")
  set(EXECUTABLE_NAME_SOOP "${target_output_name_soop}")
  set(BUNDLE_ID_SUFFIX_SOOP ${helper_plist}${_soop})

  configure_file(cmake/macos/soop/Info-helper.plist.in Info-Helper${helper_plist}${_soop}.plist)

  add_executable(${target_name_soop} MACOSX_BUNDLE EXCLUDE_FROM_ALL)
  add_executable(OBS::${target_name_soop} ALIAS ${target_name_soop})

  target_sources(${target_name_soop} PRIVATE browser-app.cpp browser-app.hpp obs-browser-page/obs-browser-page-main.cpp
                                        cef-headers.hpp)
  target_compile_definitions(${target_name_soop} PRIVATE ENABLE_BROWSER_SHARED_TEXTURE)
  if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 14.0.3)
    target_compile_options(${target_name_soop} PRIVATE -Wno-error=unqualified-std-cast-call)
  endif()

  target_include_directories(${target_name_soop} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps"
                                                    "${CMAKE_CURRENT_SOURCE_DIR}/obs-browser-page")

  target_link_libraries(${target_name_soop} PRIVATE CEF::Wrapper nlohmann_json::nlohmann_json)

  set_target_properties(
    ${target_name_soop}
    PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info-Helper${helper_plist}${_soop}.plist"
               OUTPUT_NAME "${target_output_name_soop}"
               FOLDER plugins/obs-browser/Helpers
               XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.obsproject.obs-studio.helper${helper_plist}${_soop}
               XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
               "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos/soop/entitlements-helper${helper_plist}${_soop}.plist")
endforeach()