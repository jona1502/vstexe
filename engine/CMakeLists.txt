add_library(vocalchain_engine STATIC src/ChainState.cpp src/PluginChainEngine.cpp)
target_include_directories(vocalchain_engine PUBLIC include)
target_link_libraries(vocalchain_engine PUBLIC
    vocalchain_platform
    juce::juce_audio_devices juce::juce_audio_processors juce::juce_audio_utils
    juce::juce_data_structures)
target_compile_definitions(vocalchain_engine PUBLIC
    JUCE_PLUGINHOST_VST3=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
