add_library(inputrack_commerce STATIC
    src/EntitlementService.cpp
    src/FeatureAccess.cpp
    src/TrialPolicy.cpp)
target_include_directories(inputrack_commerce PUBLIC include)
target_link_libraries(inputrack_commerce PUBLIC
    juce::juce_events
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
target_compile_definitions(inputrack_commerce PRIVATE
    INPUTRACK_STORE_BUILD=$<BOOL:${INPUTRACK_STORE_BUILD}>
    INPUTRACK_PRO_OFFER_TOKEN="inputrack.pro")
if(WIN32 AND INPUTRACK_STORE_BUILD)
    target_link_libraries(inputrack_commerce PRIVATE windowsapp runtimeobject)
endif()
