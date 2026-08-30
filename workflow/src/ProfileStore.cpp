#include <inputrack/ProfileStore.h>

namespace inputrack {
namespace {
constexpr int schemaVersion = 1;

juce::var toVar(const WorkflowProfile& profile)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("name", profile.name);
    object->setProperty("inputDevice", profile.inputDevice);
    object->setProperty("outputDevice", profile.outputDevice);
    object->setProperty("chain", profile.chainJson);
    juce::Array<juce::var> applications;
    for (const auto& application : profile.applications) applications.add(application);
    object->setProperty("applications", applications);
    return object;
}

std::optional<WorkflowProfile> fromVar(const juce::var& value)
{
    auto* object = value.getDynamicObject();
    if (object == nullptr) return {};
    WorkflowProfile profile;
    profile.name = object->getProperty("name").toString().trim();
    profile.inputDevice = object->getProperty("inputDevice").toString();
    profile.outputDevice = object->getProperty("outputDevice").toString();
    profile.chainJson = object->getProperty("chain").toString();
    if (auto* applications = object->getProperty("applications").getArray())
        for (const auto& application : *applications)
            profile.applications.addIfNotAlreadyThere(application.toString().trim().toLowerCase());
    if (profile.name.isEmpty() || profile.chainJson.isEmpty()) return {};
    return profile;
}
}

ProfileStore::ProfileStore(juce::File storageFile) : file(std::move(storageFile)) {}

bool ProfileStore::load(juce::String& error)
{
    profiles.clear();
    if (!file.existsAsFile()) return true;
    const auto parsed = juce::JSON::parse(file.loadFileAsString());
    auto* root = parsed.getDynamicObject();
    if (root == nullptr || static_cast<int>(root->getProperty("schemaVersion")) != schemaVersion) {
        error = "The profile library has an unsupported format.";
        return false;
    }
    auto* entries = root->getProperty("profiles").getArray();
    if (entries == nullptr) {
        error = "The profile library contains no profile list.";
        return false;
    }
    for (const auto& entry : *entries) {
        auto profile = fromVar(entry);
        if (!profile.has_value()) {
            error = "The profile library contains an invalid profile.";
            profiles.clear();
            return false;
        }
        upsert(std::move(*profile));
    }
    return true;
}

bool ProfileStore::save(juce::String& error) const
{
    juce::Array<juce::var> entries;
    for (const auto& profile : profiles) entries.add(toVar(profile));
    auto* root = new juce::DynamicObject();
    root->setProperty("schemaVersion", schemaVersion);
    root->setProperty("profiles", entries);
    if (!file.getParentDirectory().createDirectory()
        || !file.replaceWithText(juce::JSON::toString(juce::var(root), true))) {
        error = "Could not write " + file.getFullPathName();
        return false;
    }
    return true;
}

const juce::Array<WorkflowProfile>& ProfileStore::all() const noexcept { return profiles; }

std::optional<WorkflowProfile> ProfileStore::find(const juce::String& name) const
{
    for (const auto& profile : profiles)
        if (profile.name.equalsIgnoreCase(name)) return profile;
    return {};
}

std::optional<WorkflowProfile> ProfileStore::matchApplication(const juce::String& executable) const
{
    const auto candidate = executable.trim().toLowerCase();
    if (candidate.isEmpty()) return {};
    for (const auto& profile : profiles)
        for (const auto& application : profile.applications)
            if (application.equalsIgnoreCase(candidate)) return profile;
    return {};
}

juce::StringArray ProfileStore::applicationConflicts(
    const juce::StringArray& applications,
    const juce::String& excludedProfile) const
{
    juce::StringArray conflicts;
    for (const auto& application : applications) {
        const auto candidate = application.trim().toLowerCase();
        if (candidate.isEmpty()) continue;
        for (const auto& profile : profiles) {
            if (profile.name.equalsIgnoreCase(excludedProfile)) continue;
            for (const auto& boundApplication : profile.applications)
                if (boundApplication.equalsIgnoreCase(candidate)) {
                    conflicts.addIfNotAlreadyThere(candidate);
                    break;
                }
        }
    }
    return conflicts;
}

void ProfileStore::upsert(WorkflowProfile profile)
{
    profile.name = profile.name.trim();
    profile.applications.removeEmptyStrings();
    profile.applications.trim();
    for (auto& application : profile.applications) application = application.toLowerCase();
    for (auto& current : profiles) {
        if (current.name.equalsIgnoreCase(profile.name)) {
            current = std::move(profile);
            return;
        }
    }
    profiles.add(std::move(profile));
}

bool ProfileStore::remove(const juce::String& name)
{
    for (int i = 0; i < profiles.size(); ++i)
        if (profiles.getReference(i).name.equalsIgnoreCase(name)) {
            profiles.remove(i);
            return true;
        }
    return false;
}
}
