add_library(vocalchain_update STATIC src/UpdateChecker.cpp)
target_include_directories(vocalchain_update PUBLIC include)
# juce::SHA256 verifies the download; juce::URL reaches GitHub over WinINet,
# which JUCE links itself on Windows, so no extra platform library is needed.
target_link_libraries(vocalchain_update PUBLIC
    juce::juce_core juce::juce_cryptography juce::juce_events)
