cmake_minimum_required(VERSION 3.24)
project(VisualESPOverlay VERSION 0.3.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ESP_BUILD_OVERLAY "Build the Windows DirectX 11 application" ${WIN32})
option(ESP_BUILD_CONFIG_TESTS "Build JSON and runtime tests (requires JSON dependency)" ${ESP_BUILD_OVERLAY})
option(ESP_BUILD_TOOLS "Build portable frame_tool" ${ESP_BUILD_OVERLAY})
option(ESP_BUILD_RENDER_TESTS "Build CPU ImGui rendering tests" ${ESP_BUILD_OVERLAY})
include(CTest)
if(BUILD_TESTING)
  add_executable(core_tests tests/core_tests.cpp src/demo_provider.cpp)
  target_include_directories(core_tests PRIVATE include)
  add_test(NAME core_tests COMMAND core_tests)
endif()

if(ESP_BUILD_OVERLAY OR ESP_BUILD_TOOLS OR (BUILD_TESTING AND (ESP_BUILD_CONFIG_TESTS OR ESP_BUILD_RENDER_TESTS)))
  include(FetchContent)
  FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03)
  FetchContent_MakeAvailable(json)
  find_package(Threads REQUIRED)
  add_library(esp_runtime STATIC src/dbd_connection.cpp src/frame_io.cpp src/frame_sources.cpp src/runtime_provider.cpp src/demo_provider.cpp)
  target_include_directories(esp_runtime PUBLIC include)
  target_link_libraries(esp_runtime PUBLIC nlohmann_json::nlohmann_json Threads::Threads)
  if(MSVC)
    target_compile_options(esp_runtime PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(esp_runtime PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(ESP_BUILD_TOOLS)
    add_executable(frame_tool tools/frame_tool.cpp)
    target_link_libraries(frame_tool PRIVATE esp_runtime)
    if(MINGW)
      target_link_options(frame_tool PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
  endif()
  if(BUILD_TESTING AND ESP_BUILD_CONFIG_TESTS)
    add_executable(config_tests tests/config_tests.cpp src/config.cpp)
    target_include_directories(config_tests PRIVATE include)
    target_link_libraries(config_tests PRIVATE esp_runtime)
    add_executable(runtime_tests tests/runtime_tests.cpp)
    target_link_libraries(runtime_tests PRIVATE esp_runtime)
    foreach(group serialization validation concurrency recovery recording scale)
      add_test(NAME frame_${group} COMMAND runtime_tests ${group})
      set_tests_properties(frame_${group} PROPERTIES TIMEOUT 30)
    endforeach()
    add_test(NAME config_tests COMMAND config_tests)
    add_executable(dbd_connection_tests tests/dbd_connection_tests.cpp)
    target_link_libraries(dbd_connection_tests PRIVATE esp_runtime)
    add_test(NAME dbd_transport_is_not_game_connection COMMAND dbd_connection_tests)
    if(ESP_BUILD_TOOLS)
      add_test(NAME dbd_cli_expected_unavailable COMMAND ${CMAKE_COMMAND}
        -DTOOL=$<TARGET_FILE:frame_tool> -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/dbd_cli_test.cmake)
    endif()
  endif()
endif()

if(ESP_BUILD_OVERLAY OR (BUILD_TESTING AND ESP_BUILD_RENDER_TESTS))
  include(FetchContent)
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG f5befd2d29e66809cd1110a152e375a7f1981f06)
  FetchContent_MakeAvailable(imgui)
  add_library(imgui_core STATIC ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
  target_include_directories(imgui_core PUBLIC ${imgui_SOURCE_DIR})
  if(MSVC)
    target_compile_options(imgui_core PRIVATE /utf-8)
  endif()
  if(WIN32)
    target_link_libraries(imgui_core PUBLIC user32 imm32)
  endif()
  if(BUILD_TESTING AND ESP_BUILD_RENDER_TESTS)
    add_executable(render_tests tests/render_tests.cpp src/renderer.cpp src/config.cpp)
    target_link_libraries(render_tests PRIVATE imgui_core esp_runtime)
    add_test(NAME render_budget_and_killer_view COMMAND render_tests)
  endif()
endif()

if(ESP_BUILD_OVERLAY)
  if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The overlay requires Windows x64. Portable tests: -DESP_BUILD_OVERLAY=OFF")
  endif()
  add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
  target_include_directories(imgui_backend PUBLIC ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(imgui_backend PUBLIC imgui_core)
  target_compile_definitions(imgui_backend PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
  target_link_libraries(imgui_backend PUBLIC d3d11 dxgi d3dcompiler dwmapi)
  add_executable(VisualESPOverlay WIN32 src/main.cpp src/overlay.cpp src/renderer.cpp src/config.cpp)
  target_include_directories(VisualESPOverlay PRIVATE include)
  target_link_libraries(VisualESPOverlay PRIVATE imgui_backend esp_runtime dcomp user32 gdi32)
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
      ${CMAKE_CURRENT_SOURCE_DIR}/config.example.json $<TARGET_FILE_DIR:VisualESPOverlay>/config.example.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_CURRENT_SOURCE_DIR}/examples/frame.json $<TARGET_FILE_DIR:VisualESPOverlay>/example.frame.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${imgui_SOURCE_DIR}/LICENSE.txt $<TARGET_FILE_DIR:VisualESPOverlay>/LICENSE-imgui.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${json_SOURCE_DIR}/LICENSE.MIT $<TARGET_FILE_DIR:VisualESPOverlay>/LICENSE-json.txt)
endif()
