#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace inputrack {
class ChainState final {
public:
    static constexpr int currentSchemaVersion = 1;
    ChainState();
    explicit ChainState(juce::ValueTree state);
    const juce::ValueTree& valueTree() const noexcept;
    juce::String name() const;
    void setName(const juce::String&);
    int size() const noexcept;
    juce::ValueTree pluginAt(int) const;
    void addPlugin(const juce::PluginDescription&, const juce::MemoryBlock&, int insertIndex = -1);
    void removePlugin(int);
    void movePlugin(int from, int to);
    void setBypassed(int, bool);
    bool isBypassed(int) const;
    void setPluginState(int, const juce::MemoryBlock&);
    juce::MemoryBlock pluginState(int) const;
    juce::String toJson() const;
    static ChainState fromJson(const juce::String&);

private:
    static juce::var treeToVar(const juce::ValueTree&);
    static juce::ValueTree varToTree(const juce::var&);
    juce::ValueTree root;
};
}
