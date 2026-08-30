#pragma once
#include <juce_core/juce_core.h>
#include <optional>

namespace inputrack {
struct WorkflowProfile {
    juce::String name;
    juce::String inputDevice;
    juce::String outputDevice;
    juce::StringArray applications;
    juce::String chainJson;
};

/** Persistent named racks and their optional foreground-application bindings. */
class ProfileStore final {
public:
    explicit ProfileStore(juce::File storageFile);

    bool load(juce::String& error);
    bool save(juce::String& error) const;
    const juce::Array<WorkflowProfile>& all() const noexcept;
    std::optional<WorkflowProfile> find(const juce::String& name) const;
    std::optional<WorkflowProfile> matchApplication(const juce::String& executable) const;
    juce::StringArray applicationConflicts(
        const juce::StringArray& applications,
        const juce::String& excludedProfile = {}) const;
    void upsert(WorkflowProfile profile);
    bool remove(const juce::String& name);

private:
    juce::File file;
    juce::Array<WorkflowProfile> profiles;
};
}
