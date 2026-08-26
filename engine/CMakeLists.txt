add_library(inputrack_engine STATIC
    src/ChainState.cpp src/ChannelAdapterProcessor.cpp src/PluginBrowsing.cpp
    src/PluginChainEngine.cpp)
target_include_directories(inputrack_engine PUBLIC include)
target_link_libraries(inputrack_engine PUBLIC
    inputrack_platform
    juce::juce_audio_devices juce::juce_audio_processors juce::juce_audio_utils
    juce::juce_data_structures)
target_compile_definitions(inputrack_engine PUBLIC
    JUCE_PLUGINHOST_VST3=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
