#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>

namespace {
juce::String valueAfter(const juce::StringArray& arguments, const juce::String& option)
{
    const auto index = arguments.indexOf(option);
    return juce::isPositiveAndBelow(index, arguments.size() - 1)
        ? arguments[index + 1] : juce::String{};
}
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    juce::StringArray arguments;
    for (int i = 1; i < argc; ++i)
        arguments.add(juce::String::fromUTF8(argv[i]));

    const auto formatName = valueAfter(arguments, "--format");
    const auto identifier = valueAfter(arguments, "--plugin");
    const juce::File output(valueAfter(arguments, "--output"));
    if (formatName.isEmpty() || identifier.isEmpty() || output == juce::File{}) {
        std::cerr << "Usage: InputRackPluginScanner --format FORMAT --plugin ID --output FILE\n";
        return 2;
    }

    juce::AudioPluginFormatManager formats;
    formats.addDefaultFormats();
    juce::AudioPluginFormat* format = nullptr;
    for (auto* candidate : formats.getFormats())
        if (candidate->getName() == formatName) {
            format = candidate;
            break;
        }
    if (format == nullptr) {
        std::cerr << "Requested plug-in format is unavailable\n";
        return 3;
    }

    juce::KnownPluginList scanned;
    juce::OwnedArray<juce::PluginDescription> found;
    scanned.scanAndAddFile(identifier, false, found, *format);
    const auto xml = scanned.createXml();
    if (xml == nullptr || !output.replaceWithText(xml->toString())) {
        std::cerr << "Could not write scan result\n";
        return 4;
    }
    return 0;
}
