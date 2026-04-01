juce_add_plugin(FunBoxVenus
    PRODUCT_NAME "FunBox Venus"
    COMPANY_NAME "FunBox"
    BUNDLE_ID "com.funbox.venus"
    PLUGIN_MANUFACTURER_CODE Fnbx
    PLUGIN_CODE Vnus
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(FunBoxVenus
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(FunBoxVenus
    PRIVATE
        Source
        Source/DSP
)

target_compile_definitions(FunBoxVenus
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(FunBoxVenus
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
