juce_add_gui_app(InputRack
    PRODUCT_NAME "InputRack"
    COMPANY_NAME "InputRack"
    BUNDLE_ID "app.inputrack.desktop"
    ICON_BIG "${CMAKE_SOURCE_DIR}/voice.png"
    ICON_SMALL "${CMAKE_SOURCE_DIR}/voice.png")

target_sources(InputRack PRIVATE src/Main.cpp src/MainComponent.cpp)
target_include_directories(InputRack PRIVATE src)
target_link_libraries(InputRack PRIVATE
    inputrack_engine inputrack_platform inputrack_update
    juce::juce_audio_utils juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Dark native title bar on Windows goes through DwmSetWindowAttribute.
if(WIN32)
    target_link_libraries(InputRack PRIVATE dwmapi)
endif()
target_compile_definitions(InputRack PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:InputRack,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
