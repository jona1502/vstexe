juce_add_gui_app(VocalChain
    PRODUCT_NAME "VocalChain"
    COMPANY_NAME "VocalChain"
    BUNDLE_ID "app.vocalchain.desktop"
    ICON_BIG "${CMAKE_SOURCE_DIR}/voice.png"
    ICON_SMALL "${CMAKE_SOURCE_DIR}/voice.png")

target_sources(VocalChain PRIVATE src/Main.cpp src/MainComponent.cpp)
target_include_directories(VocalChain PRIVATE src)
target_link_libraries(VocalChain PRIVATE
    vocalchain_engine vocalchain_platform
    juce::juce_audio_utils juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Dark native title bar on Windows goes through DwmSetWindowAttribute.
if(WIN32)
    target_link_libraries(VocalChain PRIVATE dwmapi)
endif()
target_compile_definitions(VocalChain PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:VocalChain,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
