cmake_minimum_required(VERSION 3.16)
project(Simple-x264-Launcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# UI files directory
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/gui)

# Source files (excluding Windows-only: avisynth, nvencc, updater)
set(SOURCES
    src/main.cpp
    src/global.cpp
    src/mutils_compat.cpp
    src/job_object.cpp
    src/mediainfo.cpp
    src/input_filter.cpp
    # Models
    src/model_options.cpp
    src/model_jobList.cpp
    src/model_recently.cpp
    src/model_preferences.cpp
    src/model_clipInfo.cpp
    src/model_logFile.cpp
    # Validators
    src/string_validator.cpp
    # Encoders
    src/encoder_abstract.cpp
    src/encoder_factory.cpp
    src/encoder_x264.cpp
    src/encoder_x265.cpp
    # Sources (VapourSynth only)
    src/source_abstract.cpp
    src/source_factory.cpp
    src/source_vapoursynth.cpp
    # Tools
    src/tool_abstract.cpp
    # Threads
    src/thread_abstract.cpp
    src/thread_startup.cpp
    src/thread_binaries.cpp
    src/thread_encode.cpp
    src/thread_vapoursynth.cpp
    src/thread_ipc_recv.cpp
    src/thread_ipc_send.cpp
    # Windows/Dialogs
    src/win_main.cpp
    src/win_about.cpp
    src/win_addJob.cpp
    src/win_editor.cpp
    src/win_help.cpp
    src/win_preferences.cpp
)

# Header files
set(HEADERS
    src/global.h
    src/mutils_compat.h
    src/cli.h
    src/ipc.h
    src/job_object.h
    src/mediainfo.h
    src/input_filter.h
    src/string_validator.h
    src/version.h
    # Models
    src/model_options.h
    src/model_jobList.h
    src/model_clipInfo.h
    src/model_preferences.h
    src/model_recently.h
    src/model_status.h
    src/model_sysinfo.h
    # Encoders
    src/encoder_abstract.h
    src/encoder_factory.h
    src/encoder_x264.h
    src/encoder_x265.h
    # Sources
    src/source_abstract.h
    src/source_factory.h
    src/source_vapoursynth.h
    # Tools
    src/tool_abstract.h
    # Threads
    src/thread_abstract.h
    src/thread_startup.h
    src/thread_binaries.h
    src/thread_encode.h
    src/thread_vapoursynth.h
    src/thread_ipc_recv.h
    src/thread_ipc_send.h
    # Windows/Dialogs
    src/win_main.h
    src/win_about.h
    src/win_addJob.h
    src/win_editor.h
    src/win_help.h
    src/win_preferences.h
)

# UI files (excluding win_updater.ui)
set(UI_FILES
    gui/win_main.ui
    gui/win_about.ui
    gui/win_addJob.ui
    gui/win_editor.ui
    gui/win_help.ui
    gui/win_preferences.ui
)

# Resource files
set(RESOURCES
    res/resources.qrc
)

# macOS app icon
set(MACOS_ICON ${CMAKE_SOURCE_DIR}/res/icons/movie.icns)
set_source_files_properties(${MACOS_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

# Create the executable (macOS bundle)
add_executable(${PROJECT_NAME} MACOSX_BUNDLE
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
    ${RESOURCES}
    ${MACOS_ICON}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Link Qt6 libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# macOS specific: link IOKit for power management
if(APPLE)
    find_library(IOKIT_FRAMEWORK IOKit)
    if(IOKIT_FRAMEWORK)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${IOKIT_FRAMEWORK})
    endif()
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    if(COREFOUNDATION_FRAMEWORK)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${COREFOUNDATION_FRAMEWORK})
    endif()
endif()

# Set bundle properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Simple x264 Launcher"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.muldersoft.simple-x264-launcher"
    MACOSX_BUNDLE_BUNDLE_VERSION "3.0.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "3.0"
    MACOSX_BUNDLE_ICON_FILE "movie.icns"
)
