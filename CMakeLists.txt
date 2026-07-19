set(HLHOST_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(HLSDK_ROOT_DIR "${PROJECT_SOURCE_DIR}/third_party/halflife-sdk")
set(HLHOST_CHANGELEVEL_POLICY_RUNNER
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_changelevel_policy_regressions.ps1")

find_program(HLHOST_POWERSHELL_EXECUTABLE
    NAMES powershell pwsh
    REQUIRED)

add_library(common STATIC
    include/common/logger.h
    include/common/text_encoding.h
    src/common/logger.cpp
    src/common/text_encoding.cpp
)
target_include_directories(common PUBLIC "${HLHOST_INCLUDE_DIR}")
set_target_properties(common PROPERTIES FOLDER "host")

add_library(platform STATIC
    include/platform/environment.h
    src/platform/environment.cpp
)
target_include_directories(platform PUBLIC "${HLHOST_INCLUDE_DIR}")
target_link_libraries(platform PRIVATE common)
set_target_properties(platform PROPERTIES FOLDER "host")

add_library(filesystem STATIC
    include/filesystem/file_system.h
    include/filesystem/valve_directory.h
    src/filesystem/file_system.cpp
    src/filesystem/valve_directory.cpp
)
target_include_directories(filesystem PUBLIC "${HLHOST_INCLUDE_DIR}")
set_target_properties(filesystem PROPERTIES FOLDER "host")

add_library(goldsrc_network STATIC
    include/network/ipv4_endpoint.h
    include/network/goldsrc_connectionless.h
    include/network/goldsrc_netchan.h
    include/network/udp_socket.h
    src/network/ipv4_endpoint.cpp
    src/network/goldsrc_connectionless.cpp
    src/network/goldsrc_netchan.cpp
    src/network/udp_socket_win32.cpp
)
target_include_directories(goldsrc_network PUBLIC "${HLHOST_INCLUDE_DIR}")
if(WIN32)
    target_link_libraries(goldsrc_network PRIVATE ws2_32 bcrypt)
endif()
set_target_properties(goldsrc_network PROPERTIES FOLDER "host")

add_library(game_api STATIC
    include/game_api/cfg_executor.h
    include/game_api/cvar_registry.h
    include/game_api/dll_module.h
    include/game_api/dll_search_directories.h
    include/game_api/game_module_loader.h
    include/game_api/hl_server_module.h
    include/game_api/server_command_buffer.h
    include/game_api/server_command_dispatcher.h
    include/game_api/win32_error.h
    src/game_api/cfg_executor.cpp
    src/game_api/cvar_registry.cpp
    src/game_api/dll_module.cpp
    src/game_api/dll_search_directories.cpp
    src/game_api/entity_think_scheduler.cpp
    src/game_api/entity_think_scheduler.h
    src/game_api/brush_door_bootstrap_controller.cpp
    src/game_api/brush_door_bootstrap_controller.h
    src/game_api/entity_lump_parser.cpp
    src/game_api/entity_lump_parser.h
    src/game_api/map_logic_dispatcher.cpp
    src/game_api/map_logic_dispatcher.h
    src/game_api/path_mover_runtime.cpp
    src/game_api/path_mover_runtime.h
    src/game_api/path_arrival_calibrator.cpp
    src/game_api/path_arrival_calibrator.h
    src/game_api/path_advance_controller.cpp
    src/game_api/path_advance_controller.h
    src/game_api/path_node_event_semantics_controller.cpp
    src/game_api/path_node_event_semantics_controller.h
    src/game_api/path_node_message_runtime.cpp
    src/game_api/path_node_message_runtime.h
    src/game_api/path_node_event_dispatcher.cpp
    src/game_api/path_node_event_dispatcher.h
    src/game_api/path_mover_controller.cpp
    src/game_api/path_mover_controller.h
    src/game_api/path_traversal_controller.cpp
    src/game_api/path_traversal_controller.h
    src/game_api/entity_text_block.cpp
    src/game_api/entity_text_block.h
    src/game_api/entity_var_access.cpp
    src/game_api/entity_var_access.h
    src/game_api/game_module_loader.cpp
    src/game_api/goldsrc_udp_handshake_runtime.inc
    src/game_api/hl_server_module.cpp
    src/game_api/scripted_logic_tracker.cpp
    src/game_api/scripted_logic_tracker.h
    src/game_api/scripted_movement_controller.cpp
    src/game_api/scripted_movement_controller.h
    src/game_api/server_bootstrap.cpp
    src/game_api/server_bootstrap.h
    src/game_api/server_command_buffer.cpp
    src/game_api/server_command_dispatcher.cpp
    src/game_api/server_frame_loop.cpp
    src/game_api/server_frame_loop.h
    src/game_api/sound_precache_registry.cpp
    src/game_api/sound_precache_registry.h
    src/game_api/spawn_trace.cpp
    src/game_api/spawn_trace.h
    src/game_api/track_path_resolver.cpp
    src/game_api/track_path_resolver.h
    src/game_api/world_bootstrap.cpp
    src/game_api/world_bootstrap.h
    src/game_api/win32_error.cpp
)
target_include_directories(game_api
    PUBLIC
        "${HLHOST_INCLUDE_DIR}"
    PRIVATE
        "${HLSDK_ROOT_DIR}/common"
        "${HLSDK_ROOT_DIR}/dlls"
        "${HLSDK_ROOT_DIR}/engine"
        "${HLSDK_ROOT_DIR}/public"
)
target_link_libraries(game_api PRIVATE common filesystem goldsrc_network)
set_source_files_properties(
    src/game_api/hl_server_module.cpp
    PROPERTIES
        COMPILE_OPTIONS "/bigobj"
)
set_target_properties(game_api PROPERTIES FOLDER "host")

add_executable(hlhost
    include/app/host_application.h
    include/app/launch_options.h
    src/app/host_application.cpp
    src/app/launch_options.cpp
    src/app/main.cpp
)

target_include_directories(hlhost PRIVATE "${HLHOST_INCLUDE_DIR}")
target_link_libraries(hlhost
    PRIVATE
        common
        platform
        filesystem
        game_api
)

set_target_properties(hlhost PROPERTIES
    FOLDER "host"
    VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

enable_testing()
add_executable(goldsrc_connectionless_tests
    src/tests/goldsrc_connectionless_tests.cpp
)
target_include_directories(goldsrc_connectionless_tests PRIVATE "${HLHOST_INCLUDE_DIR}")
target_link_libraries(goldsrc_connectionless_tests PRIVATE goldsrc_network)
if(MSVC)
    target_compile_options(goldsrc_connectionless_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(goldsrc_connectionless_tests PRIVATE -UNDEBUG)
endif()
set_target_properties(goldsrc_connectionless_tests PROPERTIES FOLDER "host/tests")
add_test(NAME goldsrc_connectionless_tests COMMAND goldsrc_connectionless_tests)

add_custom_target(goldsrc_connectionless_unit_tests
    COMMAND "$<TARGET_FILE:goldsrc_connectionless_tests>"
    DEPENDS goldsrc_connectionless_tests
    USES_TERMINAL
    COMMENT "Running GoldSrc connectionless codec, challenge, and admission tests"
)
set_target_properties(goldsrc_connectionless_unit_tests PROPERTIES FOLDER "host/tests")

add_executable(goldsrc_netchan_tests
    src/tests/goldsrc_netchan_tests.cpp
)
target_include_directories(goldsrc_netchan_tests PRIVATE "${HLHOST_INCLUDE_DIR}")
target_link_libraries(goldsrc_netchan_tests PRIVATE goldsrc_network)
if(MSVC)
    target_compile_options(goldsrc_netchan_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(goldsrc_netchan_tests PRIVATE -UNDEBUG)
endif()
set_target_properties(goldsrc_netchan_tests PROPERTIES FOLDER "host/tests")
add_test(NAME goldsrc_netchan_tests COMMAND goldsrc_netchan_tests)

add_custom_target(goldsrc_netchan_unit_tests
    COMMAND "$<TARGET_FILE:goldsrc_netchan_tests>"
    DEPENDS goldsrc_netchan_tests
    USES_TERMINAL
    COMMENT "Running GoldSrc netchan codec, sequence, and reliable-channel tests"
)
set_target_properties(goldsrc_netchan_unit_tests PROPERTIES FOLDER "host/tests")

add_custom_target(changelevel_policy_regressions
    COMMAND
        "${HLHOST_POWERSHELL_EXECUTABLE}"
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${HLHOST_CHANGELEVEL_POLICY_RUNNER}"
        -ExecutablePath "$<TARGET_FILE:hlhost>"
    DEPENDS hlhost
    USES_TERMINAL
    COMMENT "Running accepted changelevel policy regressions"
)

set_target_properties(changelevel_policy_regressions PROPERTIES
    FOLDER "host"
)
