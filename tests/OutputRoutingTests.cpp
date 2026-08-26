#include <inputrack/PluginChainEngine.h>
#include <iostream>

namespace {
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main()
{
    using Engine = inputrack::PluginChainEngine;

    // Recognising the cable decides whether the app tells the user to pick a
    // different output device, so both directions have to be reliable.
    expect(Engine::looksLikeVirtualCable("CABLE Input (VB-Audio Virtual Cable)"),
           "VB-Cable render endpoint is recognised");
    expect(Engine::looksLikeVirtualCable("SteelSeries Sonar - Microphone"),
           "Sonar endpoint is recognised");
    expect(Engine::looksLikeVirtualCable("voicemeeter aux input"),
           "matching ignores case");
    expect(!Engine::looksLikeVirtualCable("MONITOR L/R (Volt 1)"),
           "a physical interface is not mistaken for a cable");
    expect(!Engine::looksLikeVirtualCable(""),
           "an empty device name is not a cable");

    // The capture name is shown verbatim as the device to select elsewhere, so
    // a wrong guess would send the user looking for something that lists no
    // such device.
    expect(Engine::pairedCaptureName("CABLE Input (VB-Audio Virtual Cable)")
               == "CABLE Output (VB-Audio Virtual Cable)",
           "VB-Cable render name maps to its capture side");
    expect(Engine::pairedCaptureName("SteelSeries Sonar - Microphone")
               == "SteelSeries Sonar - Microphone",
           "Sonar uses one name on both sides");
    expect(Engine::pairedCaptureName("MONITOR L/R (Volt 1)").isEmpty(),
           "an unknown device yields no capture name instead of a guess");

    if (failures == 0) std::cout << "OutputRoutingTests passed\n";
    return failures == 0 ? 0 : 1;
}
