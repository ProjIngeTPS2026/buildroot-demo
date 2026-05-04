cmake_minimum_required(VERSION 3.16)

project(apping LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.12 COMPONENTS Core Gui Multimedia Network Widgets QUIET)
if (Qt5_FOUND)
    set(QT_PACKAGE_PREFIX Qt5)
else()
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Multimedia Network Widgets)
    set(QT_PACKAGE_PREFIX Qt6)
    qt_standard_project_setup(REQUIRES 6.5)
endif()

add_library(apping_common
    src/common/app_paths.cpp
    src/common/app_paths.h
    src/common/json_protocol.cpp
    src/common/json_protocol.h
    src/common/models.cpp
    src/common/models.h
    src/common/network_utils.cpp
    src/common/network_utils.h
    src/common/roc_integration.cpp
    src/common/roc_integration.h
    src/common/simple_http_server.cpp
    src/common/simple_http_server.h
    src/common/wav_utils.cpp
    src/common/wav_utils.h
)

target_link_libraries(apping_common
    PUBLIC
        ${QT_PACKAGE_PREFIX}::Core
        ${QT_PACKAGE_PREFIX}::Network
        ${QT_PACKAGE_PREFIX}::Multimedia
)

target_include_directories(apping_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(fixed-base-service
    src/fixed_base_service/base_config.cpp
    src/fixed_base_service/base_config.h
    src/fixed_base_service/base_service.cpp
    src/fixed_base_service/base_service.h
    src/fixed_base_service/discovery_broadcaster.cpp
    src/fixed_base_service/discovery_broadcaster.h
    src/fixed_base_service/library_manager.cpp
    src/fixed_base_service/library_manager.h
    src/fixed_base_service/live_session_controller.cpp
    src/fixed_base_service/live_session_controller.h
    src/fixed_base_service/main.cpp
    src/fixed_base_service/playback_controller.cpp
    src/fixed_base_service/playback_controller.h
)

target_link_libraries(fixed-base-service
    PRIVATE
        apping_common
        ${QT_PACKAGE_PREFIX}::Core
        ${QT_PACKAGE_PREFIX}::Multimedia
        ${QT_PACKAGE_PREFIX}::Network
)

add_executable(portable-console
    src/portable_console/asset_library_model.cpp
    src/portable_console/asset_library_model.h
    src/portable_console/audio_waveform_widget.cpp
    src/portable_console/audio_waveform_widget.h
    src/portable_console/base_directory_model.cpp
    src/portable_console/base_directory_model.h
    src/portable_console/base_list_delegate.cpp
    src/portable_console/base_list_delegate.h
    src/portable_console/base_map_widget.cpp
    src/portable_console/base_map_widget.h
    src/portable_console/main.cpp
    src/portable_console/mainwindow.cpp
    src/portable_console/mainwindow.h
    src/portable_console/mainwindow.ui
    src/portable_console/offline_tile_server.cpp
    src/portable_console/offline_tile_server.h
    src/portable_console/portable_config.cpp
    src/portable_console/portable_config.h
    src/portable_console/portable_controller.cpp
    src/portable_console/portable_controller.h
)

target_link_libraries(portable-console
    PRIVATE
        apping_common
        ${QT_PACKAGE_PREFIX}::Core
        ${QT_PACKAGE_PREFIX}::Gui
        ${QT_PACKAGE_PREFIX}::Multimedia
        ${QT_PACKAGE_PREFIX}::Network
        ${QT_PACKAGE_PREFIX}::Widgets
)

add_executable(sim-launcher
    src/sim_launcher/main.cpp
)

target_link_libraries(sim-launcher
    PRIVATE
        apping_common
        ${QT_PACKAGE_PREFIX}::Core
)

install(TARGETS fixed-base-service portable-console sim-launcher)
