add_library(inputrack_scanner STATIC src/IsolatedPluginScanner.cpp)
target_include_directories(inputrack_scanner PUBLIC include)
target_link_libraries(inputrack_scanner PUBLIC
    juce::juce_audio_processors
    juce::juce_core)
target_compile_definitions(inputrack_scanner PUBLIC
    JUCE_PLUGINHOST_VST3=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)

juce_add_console_app(InputRackPluginScanner
    PRODUCT_NAME "InputRackPluginScanner"
    COMPANY_NAME "InputRack")
target_sources(InputRackPluginScanner PRIVATE src/Main.cpp)
target_link_libraries(InputRackPluginScanner PRIVATE
    inputrack_scanner
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
