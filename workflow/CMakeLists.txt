add_library(inputrack_workflow STATIC src/ProfileStore.cpp)
target_include_directories(inputrack_workflow PUBLIC include)
target_link_libraries(inputrack_workflow PUBLIC
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
