add_library(inputrack_update STATIC src/UpdateChecker.cpp)
target_include_directories(inputrack_update PUBLIC include)
# juce::SHA256 verifies the download; juce::URL reaches GitHub over WinINet,
# which JUCE links itself on Windows, so no extra platform library is needed.
target_link_libraries(inputrack_update PUBLIC
    juce::juce_core juce::juce_cryptography juce::juce_events)
