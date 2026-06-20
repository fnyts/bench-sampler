#include "MainComponent.h"

#include <iostream>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
std::string escapeField (std::string text)
{
    std::string escaped;
    escaped.reserve (text.size());
    for (const auto character : text)
    {
        if (character == '\\')
            escaped += "\\\\";
        else if (character == '\t')
            escaped += "\\t";
        else if (character == '\n')
            escaped += "\\n";
        else if (character == '\r')
            escaped += "\\r";
        else
            escaped += character;
    }
    return escaped;
}

void writeHelperLine (const std::vector<std::string>& fields)
{
    for (std::size_t index = 0; index < fields.size(); ++index)
    {
        if (index > 0)
            std::cout << '\t';
        std::cout << escapeField (fields[index]);
    }
    std::cout << '\n';
}

int runVst3ScanHelper (const juce::StringArray& parameters)
{
    if (parameters.size() < 2)
    {
        writeHelperLine ({ "SFB_PLUGIN_SCAN_FAIL", "", "Missing plugin path" });
        return 2;
    }

    auto pluginPath = parameters[1];
    if (pluginPath.isQuotedString())
        pluginPath = pluginPath.unquoted();

    try
    {
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile (descriptions, pluginPath);

        if (descriptions.isEmpty())
        {
            writeHelperLine ({ "SFB_PLUGIN_SCAN_FAIL", pluginPath.toStdString(), "No VST3 descriptions found" });
            return 3;
        }

        for (const auto* description : descriptions)
        {
            writeHelperLine ({ "SFB_PLUGIN_SCAN_FOUND",
                               description->name.toStdString(),
                               description->manufacturerName.toStdString(),
                               "VST3",
                               description->category.toStdString(),
                               description->fileOrIdentifier.toStdString(),
                               juce::String (description->uniqueId).toStdString(),
                               juce::String (juce::File { description->fileOrIdentifier }
                                                .getLastModificationTime()
                                                .toMilliseconds()).toStdString() });
        }

        return 0;
    }
    catch (const std::exception& exception)
    {
        writeHelperLine ({ "SFB_PLUGIN_SCAN_FAIL", pluginPath.toStdString(), exception.what() });
        return 4;
    }
    catch (...)
    {
        writeHelperLine ({ "SFB_PLUGIN_SCAN_FAIL", pluginPath.toStdString(), "Unknown scanner exception" });
        return 5;
    }
}

class SamplerFoodBenchApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Bench Sampler"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
        const auto parameters = getCommandLineParameterArray();
        if (parameters.size() > 0 && parameters[0] == "--sfb-vst3-scan-child")
        {
            setApplicationReturnValue (runVst3ScanHelper (parameters));
            quit();
            return;
        }

        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (std::move (name),
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            centreWithSize (1280, 860);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};
}

START_JUCE_APPLICATION (SamplerFoodBenchApplication)
