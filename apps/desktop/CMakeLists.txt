juce_add_gui_app(InputRack
    PRODUCT_NAME "InputRack"
    COMPANY_NAME "InputRack"
    BUNDLE_ID "app.inputrack.desktop"
    ICON_BIG "${CMAKE_SOURCE_DIR}/voice.png"
    ICON_SMALL "${CMAKE_SOURCE_DIR}/voice.png")

target_sources(InputRack PRIVATE src/Main.cpp src/MainComponent.cpp)
target_include_directories(InputRack PRIVATE src)
target_link_libraries(InputRack PRIVATE
    inputrack_engine inputrack_platform inputrack_scanner
    juce::juce_audio_utils juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
if(NOT INPUTRACK_STORE_BUILD)
    target_link_libraries(InputRack PRIVATE inputrack_update)
endif()

# Dark native title bar on Windows goes through DwmSetWindowAttribute.
if(WIN32)
    target_link_libraries(InputRack PRIVATE dwmapi)
endif()
target_compile_definitions(InputRack PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:InputRack,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    INPUTRACK_STORE_BUILD=$<BOOL:${INPUTRACK_STORE_BUILD}>)

# The desktop app locates the crash-isolated scanner beside its executable.
# Keep that true for local artifact folders as well as for the installer.
add_dependencies(InputRack InputRackPluginScanner)
add_custom_command(TARGET InputRack POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:InputRackPluginScanner>
        $<TARGET_FILE_DIR:InputRack>)
