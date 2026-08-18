cmake_minimum_required(VERSION 3.22)
project(MusiCue VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.7
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(MusiCue
    PRODUCT_NAME "MusiCue"
    COMPANY_NAME "MusiCue"
    BUNDLE_ID "com.musicue.app"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/appicon/appicon_musicue.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Assets/appicon/appicon_musicue_small.png"
)

juce_add_binary_data(MusiCueAssets
    SOURCES
    Assets/icons/add.svg
    Assets/icons/play_arrow.svg
    Assets/icons/stop.svg
    Assets/icons/pause.svg
    Assets/icons/fiber_manual_record.svg
    Assets/icons/undo.svg
    Assets/icons/view_agenda.svg
    Assets/icons/chevron_right.svg
    Assets/icons/expand_more.svg
    Assets/icons/settings.svg
    Assets/icons/workspace_open.svg
    Assets/icons/workspace_save.svg
    Assets/icons/workspace_save_as.svg
    Assets/appicon/appicon_musicue.png
)

target_sources(MusiCue PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/AudioEngine.cpp
    Source/CueListComponent.cpp
    Source/ToolbarComponent.cpp
    Source/InspectorComponent.cpp
    Source/WorkspaceFile.cpp
    Source/SettingsWindow.cpp
    Source/WorkspaceLauncherComponent.cpp
)

target_compile_definitions(MusiCue PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MusiCue,JUCE_TARGET_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MusiCue,JUCE_VERSION>"
)

target_link_libraries(MusiCue PRIVATE
    MusiCueAssets
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if (BUILD_TESTING)
    juce_add_console_app(MusiCueTests PRODUCT_NAME "MusiCue Tests")

    target_sources(MusiCueTests PRIVATE
        Source/FadeCueTests.cpp
        Source/WorkspaceFile.cpp
    )

    target_link_libraries(MusiCueTests PRIVATE
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )

    add_test(NAME MusiCueTests COMMAND MusiCueTests)
endif()
