#include <inputrack/ChainState.h>
#include <stdexcept>

namespace inputrack {
namespace {
const juce::Identifier chainType{"inputRack"}, pluginType{"plugin"};
const juce::Identifier schemaKey{"schemaVersion"}, nameKey{"name"};
const juce::Identifier bypassedKey{"bypassed"}, stateKey{"state"};
const juce::Identifier typeKey{"_type"}, propertiesKey{"properties"}, childrenKey{"children"};
}

ChainState::ChainState() : root(chainType)
{
    root.setProperty(schemaKey, currentSchemaVersion, nullptr);
    root.setProperty(nameKey, "Untitled Rack", nullptr);
}

ChainState::ChainState(juce::ValueTree state) : root(std::move(state))
{
    if (!root.isValid() || !root.hasType(chainType))
        throw std::invalid_argument("Invalid InputRack preset root");
    if (static_cast<int>(root.getProperty(schemaKey, 0)) > currentSchemaVersion)
        throw std::invalid_argument("Preset was created by a newer InputRack version");
}

const juce::ValueTree& ChainState::valueTree() const noexcept { return root; }
juce::String ChainState::name() const { return root.getProperty(nameKey).toString(); }
void ChainState::setName(const juce::String& value) { root.setProperty(nameKey, value, nullptr); }
int ChainState::size() const noexcept { return root.getNumChildren(); }
juce::ValueTree ChainState::pluginAt(int index) const { return root.getChild(index); }

void ChainState::addPlugin(const juce::PluginDescription& description,
                           const juce::MemoryBlock& pluginState, int insertIndex)
{
    juce::ValueTree plugin(pluginType);
    plugin.setProperty("name", description.name, nullptr);
    plugin.setProperty("manufacturer", description.manufacturerName, nullptr);
    plugin.setProperty("fileOrIdentifier", description.fileOrIdentifier, nullptr);
    plugin.setProperty("uniqueId", description.uniqueId, nullptr);
    plugin.setProperty("deprecatedUid", description.deprecatedUid, nullptr);
    plugin.setProperty("format", description.pluginFormatName, nullptr);
    plugin.setProperty(bypassedKey, false, nullptr);
    plugin.setProperty(stateKey, pluginState.toBase64Encoding(), nullptr);
    root.addChild(plugin, insertIndex, nullptr);
}

void ChainState::removePlugin(int index)
{
    if (juce::isPositiveAndBelow(index, size())) root.removeChild(index, nullptr);
}

void ChainState::movePlugin(int from, int to)
{
    if (juce::isPositiveAndBelow(from, size()) && juce::isPositiveAndBelow(to, size()))
        root.moveChild(from, to, nullptr);
}

void ChainState::setBypassed(int index, bool value)
{
    if (auto plugin = pluginAt(index); plugin.isValid())
        plugin.setProperty(bypassedKey, value, nullptr);
}

bool ChainState::isBypassed(int index) const
{
    if (auto plugin = pluginAt(index); plugin.isValid())
        return plugin.getProperty(bypassedKey, false);
    return false;
}

void ChainState::setPluginState(int index, const juce::MemoryBlock& state)
{
    if (auto plugin = pluginAt(index); plugin.isValid())
        plugin.setProperty(stateKey, state.toBase64Encoding(), nullptr);
}

juce::MemoryBlock ChainState::pluginState(int index) const
{
    juce::MemoryBlock result;
    if (auto plugin = pluginAt(index); plugin.isValid())
        result.fromBase64Encoding(plugin.getProperty(stateKey).toString());
    return result;
}

juce::var ChainState::treeToVar(const juce::ValueTree& tree)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty(typeKey, tree.getType().toString());
    auto properties = std::make_unique<juce::DynamicObject>();
    for (int i = 0; i < tree.getNumProperties(); ++i) {
        const auto key = tree.getPropertyName(i);
        properties->setProperty(key, tree.getProperty(key));
    }
    object->setProperty(propertiesKey, juce::var(properties.release()));
    juce::Array<juce::var> children;
    for (int i = 0; i < tree.getNumChildren(); ++i) children.add(treeToVar(tree.getChild(i)));
    object->setProperty(childrenKey, children);
    return juce::var(object.release());
}

juce::ValueTree ChainState::varToTree(const juce::var& value)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr) throw std::invalid_argument("Preset node must be an object");
    const auto type = object->getProperty(typeKey).toString();
    if (type.isEmpty()) throw std::invalid_argument("Preset node has no type");
    juce::ValueTree tree{juce::Identifier(type)};
    if (const auto* properties = object->getProperty(propertiesKey).getDynamicObject())
        for (const auto& property : properties->getProperties())
            tree.setProperty(property.name, property.value, nullptr);
    if (const auto* children = object->getProperty(childrenKey).getArray())
        for (const auto& child : *children) tree.addChild(varToTree(child), -1, nullptr);
    return tree;
}

juce::String ChainState::toJson() const { return juce::JSON::toString(treeToVar(root), true); }

ChainState ChainState::fromJson(const juce::String& json)
{
    const auto parsed = juce::JSON::parse(json);
    if (parsed.isVoid()) throw std::invalid_argument("Invalid preset JSON");
    return ChainState(varToTree(parsed));
}
}
