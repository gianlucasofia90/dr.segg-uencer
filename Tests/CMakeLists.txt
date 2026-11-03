juce_add_console_app(DrSeggUencerTests PRODUCT_NAME "DrSeggUencerTests")
target_sources(DrSeggUencerTests PRIVATE TestMain.cpp ${PROJECT_SOURCE_DIR}/Source/SeggProcessor.cpp)
target_include_directories(DrSeggUencerTests PRIVATE ${PROJECT_SOURCE_DIR}/Source)
target_compile_definitions(DrSeggUencerTests PRIVATE
    SEGG_NO_PLUGIN_ENTRY=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_Name="DrSeggUencerTests" JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(DrSeggUencerTests PRIVATE
    DrSeggUencerFonts
    juce::juce_audio_processors juce::juce_audio_devices juce::juce_gui_basics juce::juce_audio_basics
    juce::juce_recommended_config_flags)
