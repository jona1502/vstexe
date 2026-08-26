#include <inputrack/ChainState.h>
#include <iostream>
#include <stdexcept>

namespace {
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

void expectInvalid(const juce::String& json, const char* message)
{
    try {
        inputrack::ChainState::fromJson(json);
        expect(false, message);
    } catch (const std::invalid_argument&) {
    }
}
}

/*
 * currentSchemaVersion only ever moves forward, so today there is nothing
 * older to migrate from. What has to hold regardless is the version boundary
 * itself: a preset from a future InputRack must be rejected instead of
 * partially loaded, and a preset from this or an earlier InputRack must keep
 * loading once the schema does grow, even carrying properties this version
 * has never heard of.
 */
int main()
{
    expect(inputrack::ChainState::currentSchemaVersion >= 1,
           "the schema starts at version 1 or later");

    expectInvalid(
        R"({"_type":"inputRack","properties":{"schemaVersion":0},"children":[]})",
        "schema version 0 is rejected");
    expectInvalid(
        R"({"_type":"inputRack","properties":{"schemaVersion":-1},"children":[]})",
        "a negative schema version is rejected");

    const auto future = juce::String(
        R"({"_type":"inputRack","properties":{"schemaVersion":)")
        + juce::String(inputrack::ChainState::currentSchemaVersion + 1)
        + R"(},"children":[]})";
    expectInvalid(future,
                  "a preset from a newer InputRack version is rejected, not silently truncated");

    // A future migration may add properties this reader has never heard of,
    // on the root and on individual plug-in entries; those must be ignored
    // rather than rejected, or every schema bump becomes a breaking one.
    const auto forwardTolerantJson =
        R"({"_type":"inputRack","properties":{"schemaVersion":1,"name":"Streaming Voice",)"
        R"("futureRootFlag":true},"children":[{"_type":"plugin","properties":{)"
        R"("name":"Compressor","manufacturer":"InputRack","fileOrIdentifier":"comp",)"
        R"("uniqueId":1,"deprecatedUid":0,"format":"VST3","bypassed":false,"state":"",)"
        R"("futurePluginField":"ignored"},"children":[]}]})";
    const auto forwardTolerant = inputrack::ChainState::fromJson(forwardTolerantJson);
    expect(forwardTolerant.size() == 1,
           "unknown extra properties do not stop a preset from loading");
    expect(forwardTolerant.pluginAt(0).getProperty("name") == "Compressor",
           "known properties still read correctly alongside unknown ones");

    // The version travels with the rack, so a round trip must not silently
    // rewrite it to whatever the current build happens to be.
    inputrack::ChainState state;
    const auto roundTripped = inputrack::ChainState::fromJson(state.toJson());
    expect(roundTripped.valueTree().getProperty("schemaVersion")
               == inputrack::ChainState::currentSchemaVersion,
           "the schema version round-trips unchanged");

    if (failures == 0) std::cout << "PresetMigrationTests passed\n";
    return failures == 0 ? 0 : 1;
}
