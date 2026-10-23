cmake_minimum_required(VERSION 3.24)
project(VisualESPOverlay VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
  add_executable(core_tests tests/core_tests.cpp src/demo_provider.cpp)
  target_include_directories(core_tests PRIVATE include)
  add_test(NAME core_tests COMMAND core_tests)
endif()

option(ESP_BUILD_OVERLAY "Build the Windows DirectX 11 application" ${WIN32})
if(ESP_BUILD_OVERLAY)
  if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The overlay requires Windows x64. Portable tests: -DESP_BUILD_OVERLAY=OFF")
  endif()
  include(FetchContent)
  # Exact upstream revisions; Git HTTPS transport verification stays enabled.
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG f5befd2d29e66809cd1110a152e375a7f1981f06)
  FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03)
  FetchContent_MakeAvailable(imgui json)
  if(BUILD_TESTING)
    add_executable(config_tests tests/config_tests.cpp src/config.cpp)
    target_include_directories(config_tests PRIVATE include)
    target_link_libraries(config_tests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME config_tests COMMAND config_tests)
  endif()
  add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
  target_include_directories(imgui_backend PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_compile_definitions(imgui_backend PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
  target_link_libraries(imgui_backend PUBLIC d3d11 dxgi d3dcompiler dwmapi)
  add_executable(VisualESPOverlay WIN32 src/main.cpp src/overlay.cpp src/renderer.cpp src/config.cpp src/demo_provider.cpp)
  target_include_directories(VisualESPOverlay PRIVATE include)
  target_link_libraries(VisualESPOverlay PRIVATE imgui_backend nlohmann_json::nlohmann_json dcomp user32 gdi32)
  if(MSVC)
    target_compile_options(VisualESPOverlay PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(imgui_backend PRIVATE /utf-8)
  elseif(MINGW)
    target_link_options(VisualESPOverlay PRIVATE -municode -static -static-libgcc -static-libstdc++)
  endif()
  add_custom_command(TARGET VisualESPOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_CURRENT_SOURCE_DIR}/offsets.json $<TARGET_FILE_DIR:VisualESPOverlay>/offsets.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${imgui_SOURCE_DIR}/LICENSE.txt $<TARGET_FILE_DIR:VisualESPOverlay>/LICENSE-imgui.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${json_SOURCE_DIR}/LICENSE.MIT $<TARGET_FILE_DIR:VisualESPOverlay>/LICENSE-json.txt)
endif()
