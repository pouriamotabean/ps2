cmake_minimum_required(VERSION 3.22)

project(PS VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_binary_data(PSBinaryData
    NAMESPACE PSBinaryData
    SOURCES
        Assets/Skin/bg.png
        Assets/Skin/panel.png
        Assets/Skin/button_normal.png
        Assets/Skin/button_hover.png
        Assets/Skin/button_pressed.png
        Assets/Skin/track.png
        Assets/Skin/selected.png
        Assets/Skin/flag_good.png
        Assets/Skin/flag_warn.png
        Assets/Skin/flag_critical.png
        Assets/Skin/logo.png
        Assets/Fonts/Inter-Variable.ttf
        Assets/Fonts/JetBrainsMono-Regular.ttf
        Assets/Fonts/JetBrainsMono-Bold.ttf
)

juce_add_gui_app(PS
    PRODUCT_NAME "PS"
    COMPANY_NAME "Pouria Motabean"
    ICON_BIG "Assets/Icon.png"
    ICON_SMALL "Assets/Icon.png"
    VERSION 0.2.0
)

juce_generate_juce_header(PS)

target_sources(PS PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/LoudnessMeter.cpp
    Source/SpotifyProcessor.cpp
    Source/SegmentedControl.cpp
    Source/ResultsPanel.cpp
    Source/DetailsPanel.cpp
    Source/PSLookAndFeel.cpp
    Source/AudioPlayer.cpp
    Source/SpectrumDisplay.cpp
    Source/WaveformDisplay.cpp
    Source/PSSkin.cpp
    Source/PSFonts.cpp
)

target_compile_definitions(PS PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PS,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PS,JUCE_VERSION>"
)

target_link_libraries(PS PRIVATE
    PSBinaryData
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
