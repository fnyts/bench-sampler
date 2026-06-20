#include "PluginScanWindow.h"

#include "app/BenchPalette.h"

#include <algorithm>
#include <set>

namespace
{
juce::Colour colour (samplebench::Rgb value)
{
    return juce::Colour::fromRGB (value.red, value.green, value.blue);
}

void configureButton (juce::TextButton& button)
{
    button.setColour (juce::TextButton::buttonColourId, colour (samplebench::palette::button));
    button.setColour (juce::TextButton::textColourOffId, colour (samplebench::palette::text));
    button.setColour (juce::TextButton::textColourOnId, colour (samplebench::palette::text));
}

void configureLabel (juce::Component& parent, juce::Label& label, const juce::String& text, float size = 14.0f, bool bold = false)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (juce::Font { juce::FontOptions { size, bold ? juce::Font::bold : juce::Font::plain } });
    label.setColour (juce::Label::textColourId, colour (samplebench::palette::text));
    parent.addAndMakeVisible (label);
}

juce::String pathToString (const std::filesystem::path& path)
{
    return juce::String::fromUTF8 (path.u8string().c_str());
}

juce::File pathToFile (const std::filesystem::path& path)
{
    return juce::File { pathToString (path) };
}

std::filesystem::path stringToPath (const juce::String& text)
{
    return std::filesystem::path { text.toStdString() };
}

std::string unescapeField (const std::string& text)
{
    std::string unescaped;
    unescaped.reserve (text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] != '\\' || index + 1 >= text.size())
        {
            unescaped += text[index];
            continue;
        }

        const auto next = text[++index];
        if (next == 't')
            unescaped += '\t';
        else if (next == 'n')
            unescaped += '\n';
        else if (next == 'r')
            unescaped += '\r';
        else
            unescaped += next;
    }
    return unescaped;
}

std::vector<std::string> splitHelperLine (const juce::String& line)
{
    std::vector<std::string> fields;
    std::string current;
    const auto text = line.toStdString();
    for (const auto character : text)
    {
        if (character == '\t')
        {
            fields.push_back (unescapeField (current));
            current.clear();
        }
        else
        {
            current += character;
        }
    }
    fields.push_back (unescapeField (current));
    return fields;
}

juce::String pluginRowText (const samplebench::CachedPluginDescription& plugin)
{
    auto text = juce::String::fromUTF8 (plugin.name.c_str());
    if (! plugin.manufacturer.empty())
        text += "  -  " + juce::String::fromUTF8 (plugin.manufacturer.c_str());
    text += "  -  " + juce::String::fromUTF8 (plugin.format.c_str());
    if (! plugin.category.empty())
        text += "  -  " + juce::String::fromUTF8 (plugin.category.c_str());
    if (! plugin.filePath.empty())
        text += "  -  " + pathToString (plugin.filePath);
    return text;
}

juce::String failedRowText (const samplebench::FailedPluginDescription& plugin)
{
    auto text = plugin.name.empty() ? pathToString (plugin.filePath)
                                    : juce::String::fromUTF8 (plugin.name.c_str()) + "  -  " + pathToString (plugin.filePath);
    if (! plugin.reason.empty())
        text += "  -  " + juce::String::fromUTF8 (plugin.reason.c_str());
    return text;
}

juce::String isoNow()
{
    return juce::Time::getCurrentTime().toISO8601 (true);
}
}

PluginScanService::PluginScanService (std::vector<std::filesystem::path> scanPaths,
                                      std::vector<samplebench::FailedPluginDescription> blockedPluginsToSkip)
    : juce::Thread { "Bench Sampler Plugin Scanner" },
      paths { std::move (scanPaths) },
      blockedPlugins { std::move (blockedPluginsToSkip) }
{
}

PluginScanService::~PluginScanService()
{
    cancel();
}

void PluginScanService::start()
{
    {
        const std::lock_guard lock { snapshotMutex };
        current.running = true;
        current.finished = false;
        current.cancelled = false;
        current.status = "Scanning...";
        current.progress = 0.0;
        current.foundPlugins.clear();
        current.failedPlugins.clear();
    }
    startThread();
}

void PluginScanService::cancel()
{
    signalThreadShouldExit();
    stopThread (1500);
}

PluginScanService::Snapshot PluginScanService::snapshot() const
{
    const std::lock_guard lock { snapshotMutex };
    return current;
}

void PluginScanService::updateStatus (juce::String status, double progress)
{
    const std::lock_guard lock { snapshotMutex };
    current.status = std::move (status);
    current.progress = progress;
}

void PluginScanService::run()
{
    juce::FileSearchPath searchPath;
    for (const auto& path : paths)
        searchPath.add (pathToFile (path));

    Snapshot finalSnapshot;
    finalSnapshot.running = false;
    finalSnapshot.finished = true;
    finalSnapshot.cancelled = false;
    finalSnapshot.progress = 1.0;
    finalSnapshot.blockedPlugins = blockedPlugins;
    for (auto& blocked : finalSnapshot.blockedPlugins)
        blocked.reason = "Skipped until Rescan All or Clear Failed";

    if (searchPath.getNumPaths() == 0)
    {
        updateStatus ("No plugin paths configured", 1.0);
    }
    else
    {
        juce::VST3PluginFormat vst3Format;
        auto filesToScan = vst3Format.searchPathsForPlugins (searchPath, true, false);
        std::set<juce::String> skippedFiles;
        for (const auto& blocked : blockedPlugins)
            skippedFiles.insert (pathToString (blocked.filePath));

        for (int index = filesToScan.size(); --index >= 0;)
            if (skippedFiles.count (filesToScan[index]) > 0)
                filesToScan.remove (index);

        const auto executable = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        const auto total = std::max (1, filesToScan.size());

        for (int index = 0; index < filesToScan.size(); ++index)
        {
            if (threadShouldExit())
            {
                finalSnapshot.cancelled = true;
                break;
            }

            const auto pluginFile = filesToScan[index];
            updateStatus ("Scanning: " + juce::File { pluginFile }.getFileNameWithoutExtension(),
                          static_cast<double> (index) / static_cast<double> (total));

            juce::ChildProcess child;
            juce::StringArray arguments;
            arguments.add (executable.getFullPathName());
            arguments.add ("--sfb-vst3-scan-child");
            arguments.add (pluginFile);

            // VST3 metadata reads can run arbitrary plugin code. Scan one plugin per child
            // process so a bad plugin becomes a failed row instead of an app crash.
            auto addFailedPlugin = [&finalSnapshot, pluginFile] (const juce::String& reason)
            {
                samplebench::FailedPluginDescription cached;
                cached.name = juce::File { pluginFile }.getFileName().toStdString();
                cached.format = "VST3";
                cached.filePath = stringToPath (pluginFile);
                cached.reason = reason.toStdString();
                cached.modifiedTimeMillis = juce::File { pluginFile }.getLastModificationTime().toMilliseconds();
                finalSnapshot.failedPlugins.push_back (std::move (cached));
            };

            if (! child.start (arguments, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
            {
                addFailedPlugin ("Could not start isolated scanner");
                continue;
            }

            const auto timeoutAt = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (30000);
            while (child.isRunning()
                   && ! threadShouldExit()
                   && juce::Time::getMillisecondCounter() < timeoutAt)
            {
                juce::Thread::sleep (10);
            }

            if (threadShouldExit())
            {
                child.kill();
                finalSnapshot.cancelled = true;
                break;
            }

            if (child.isRunning())
            {
                child.kill();
                addFailedPlugin ("Isolated scanner timed out");
                continue;
            }

            const auto output = child.readAllProcessOutput();
            const auto exitCode = child.getExitCode();
            auto foundAny = false;
            auto reportedFailure = false;

            for (const auto& line : juce::StringArray::fromLines (output))
            {
                const auto fields = splitHelperLine (line);
                if (fields.empty())
                    continue;

                if (fields[0] == "SFB_PLUGIN_SCAN_FOUND" && fields.size() >= 8)
                {
                    foundAny = true;
                    finalSnapshot.foundPlugins.push_back ({ fields[1],
                                                            fields[2],
                                                            fields[3],
                                                            fields[4],
                                                            fields[5],
                                                            fields[6],
                                                            std::atoll (fields[7].c_str()) });
                }
                else if (fields[0] == "SFB_PLUGIN_SCAN_FAIL" && fields.size() >= 3)
                {
                    reportedFailure = true;
                    addFailedPlugin (juce::String::fromUTF8 (fields[2].c_str()));
                }
            }

            if (! foundAny && ! reportedFailure)
                addFailedPlugin ("Isolated scanner crashed or returned no metadata (exit " + juce::String (static_cast<int> (exitCode)) + ")");
        }
    }

    finalSnapshot.status = finalSnapshot.cancelled
        ? "Scan cancelled"
        : "Found " + juce::String (static_cast<int> (finalSnapshot.foundPlugins.size()))
            + " plugins, " + juce::String (static_cast<int> (finalSnapshot.failedPlugins.size())) + " failed"
            + (finalSnapshot.blockedPlugins.empty()
                ? juce::String {}
                : ", " + juce::String (static_cast<int> (finalSnapshot.blockedPlugins.size())) + " skipped");

    const std::lock_guard lock { snapshotMutex };
    current = std::move (finalSnapshot);
}

PluginScanListModel::PluginScanListModel (PluginScanComponent& owner, PluginScanListKind kind)
    : component { owner },
      listKind { kind }
{
}

int PluginScanListModel::getNumRows()
{
    return component.getNumRowsForList (listKind);
}

void PluginScanListModel::paintListBoxItem (int rowNumber,
                                            juce::Graphics& g,
                                            int width,
                                            int height,
                                            bool rowIsSelected)
{
    component.paintListRow (listKind, rowNumber, g, width, height, rowIsSelected);
}

PluginScanComponent::PluginScanComponent (juce::File cacheFile)
    : registryFile { std::move (cacheFile) },
      pathsModel { *this, PluginScanListKind::paths },
      foundModel { *this, PluginScanListKind::found },
      failedModel { *this, PluginScanListKind::failed }
{
    setOpaque (true);
    configureLabel (*this, titleLabel, "Settings / Preferences", 20.0f, true);
    configureLabel (*this, explainerLabel, "Plugins are scanned only when you click Scan. Cached results are loaded on startup.", 13.0f);
    explainerLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::mutedText));
    configureLabel (*this, pathsLabel, "Plugin Paths", 15.0f, true);
    configureLabel (*this, foundLabel, "Found Plugins", 15.0f, true);
    configureLabel (*this, failedLabel, "Failed / Blocked Plugins", 15.0f, true);
    configureLabel (*this, statusLabel, "No plugin scan yet", 13.0f);
    statusLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::mutedText));

    for (auto* list : { &pathsList, &foundList, &failedList })
    {
        list->setRowHeight (24);
        list->setColour (juce::ListBox::backgroundColourId, colour (samplebench::palette::editor));
        addAndMakeVisible (*list);
    }

    addFolderButton.onClick = [this] { addFolder(); };
    removeFolderButton.onClick = [this] { removeSelectedFolder(); };
    resetPathsButton.onClick = [this] { resetPaths(); };
    scanButton.onClick = [this] { scanPlugins (false); };
    rescanButton.onClick = [this] { scanPlugins (true); };
    cancelButton.onClick = [this] { cancelScan(); };
    clearCacheButton.onClick = [this] { clearCache(); };
    clearFailedButton.onClick = [this] { clearFailed(); };

    for (auto* button : { &addFolderButton, &removeFolderButton, &resetPathsButton, &scanButton,
                          &rescanButton, &cancelButton, &clearCacheButton, &clearFailedButton })
    {
        configureButton (*button);
        addAndMakeVisible (*button);
    }

    loadRegistry();
    refreshLists();
    refreshStatusText();
    startTimerHz (8);
}

PluginScanComponent::~PluginScanComponent()
{
    if (scanner)
        scanner->cancel();
}

void PluginScanComponent::resized()
{
    auto area = getLocalBounds().reduced (18);
    titleLabel.setBounds (area.removeFromTop (28));
    explainerLabel.setBounds (area.removeFromTop (24));
    statusLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);

    auto pathSection = area.removeFromTop (160);
    pathsLabel.setBounds (pathSection.removeFromTop (24));
    auto pathButtons = pathSection.removeFromBottom (34);
    addFolderButton.setBounds (pathButtons.removeFromLeft (104));
    pathButtons.removeFromLeft (8);
    removeFolderButton.setBounds (pathButtons.removeFromLeft (122));
    pathButtons.removeFromLeft (8);
    resetPathsButton.setBounds (pathButtons.removeFromLeft (112));
    pathButtons.removeFromLeft (16);
    scanButton.setBounds (pathButtons.removeFromLeft (112));
    pathButtons.removeFromLeft (8);
    rescanButton.setBounds (pathButtons.removeFromLeft (104));
    pathButtons.removeFromLeft (8);
    cancelButton.setBounds (pathButtons.removeFromLeft (96));
    pathsList.setBounds (pathSection);

    area.removeFromTop (12);
    auto lists = area;
    auto foundArea = lists.removeFromLeft (lists.getWidth() / 2 - 6);
    lists.removeFromLeft (12);
    foundLabel.setBounds (foundArea.removeFromTop (24));
    auto foundButtons = foundArea.removeFromBottom (34);
    clearCacheButton.setBounds (foundButtons.removeFromLeft (108));
    foundList.setBounds (foundArea);

    failedLabel.setBounds (lists.removeFromTop (24));
    auto failedButtons = lists.removeFromBottom (34);
    clearFailedButton.setBounds (failedButtons.removeFromLeft (108));
    failedList.setBounds (lists);
}

int PluginScanComponent::getNumRowsForList (PluginScanListKind kind) const
{
    if (kind == PluginScanListKind::paths)
        return static_cast<int> (registry.settings.scanPaths.size());
    if (kind == PluginScanListKind::found)
        return static_cast<int> (registry.foundPlugins.size());
    return static_cast<int> (registry.failedPlugins.size() + registry.blockedPlugins.size());
}

void PluginScanComponent::paintListRow (PluginScanListKind kind,
                                        int rowNumber,
                                        juce::Graphics& g,
                                        int width,
                                        int height,
                                        bool rowIsSelected)
{
    g.fillAll (rowIsSelected ? colour (samplebench::palette::accent)
                              : colour (samplebench::palette::editor));
    g.setColour (rowIsSelected ? colour (samplebench::palette::inverseText)
                                : colour (samplebench::palette::text));

    juce::String text;
    if (kind == PluginScanListKind::paths && rowNumber < static_cast<int> (registry.settings.scanPaths.size()))
        text = pathToString (registry.settings.scanPaths[static_cast<std::size_t> (rowNumber)]);
    else if (kind == PluginScanListKind::found && rowNumber < static_cast<int> (registry.foundPlugins.size()))
        text = pluginRowText (registry.foundPlugins[static_cast<std::size_t> (rowNumber)]);
    else
    {
        const auto failedCount = static_cast<int> (registry.failedPlugins.size());
        if (rowNumber < failedCount)
            text = failedRowText (registry.failedPlugins[static_cast<std::size_t> (rowNumber)]);
        else
        {
            const auto blockedIndex = rowNumber - failedCount;
            if (blockedIndex < static_cast<int> (registry.blockedPlugins.size()))
                text = failedRowText (registry.blockedPlugins[static_cast<std::size_t> (blockedIndex)]);
        }
    }

    g.drawText (text, 8, 0, width - 16, height, juce::Justification::centredLeft);
}

void PluginScanComponent::timerCallback()
{
    finishScanIfReady();
    refreshStatusText();
}

void PluginScanComponent::loadRegistry()
{
    registry = samplebench::PluginRegistry::loadFromDisk (stringToPath (registryFile.getFullPathName()));
    if (registry.settings.scanPaths.empty())
        registry.resetPathsToDefaults (currentPlatform(), userHomeDirectory().getFullPathName().toStdString());
}

void PluginScanComponent::saveRegistry()
{
    [[maybe_unused]] const auto saved = registry.saveToDisk (stringToPath (registryFile.getFullPathName()));
}

void PluginScanComponent::refreshLists()
{
    pathsList.updateContent();
    foundList.updateContent();
    failedList.updateContent();
    pathsList.repaint();
    foundList.repaint();
    failedList.repaint();
}

void PluginScanComponent::refreshStatusText()
{
    statusLabel.setText (statusSummary(), juce::dontSendNotification);
    const auto scanning = scanner != nullptr && scanner->snapshot().running;
    scanButton.setEnabled (! scanning);
    rescanButton.setEnabled (! scanning);
    cancelButton.setEnabled (scanning);
    addFolderButton.setEnabled (! scanning);
    removeFolderButton.setEnabled (! scanning);
    resetPathsButton.setEnabled (! scanning);
    clearCacheButton.setEnabled (! scanning);
    clearFailedButton.setEnabled (! scanning);
}

void PluginScanComponent::addFolder()
{
    folderChooser = std::make_unique<juce::FileChooser> ("Choose a VST3 folder to scan",
                                                         juce::File {},
                                                         juce::String {});
    folderChooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories,
                                [this] (const juce::FileChooser& chooser)
                                {
                                    const auto folder = chooser.getResult();
                                    if (folder == juce::File {})
                                        return;

                                    if (registry.settings.addPath (stringToPath (folder.getFullPathName())))
                                    {
                                        saveRegistry();
                                        refreshLists();
                                        refreshStatusText();
                                    }
                                });
}

void PluginScanComponent::removeSelectedFolder()
{
    const auto row = pathsList.getSelectedRow();
    if (row < 0 || row >= static_cast<int> (registry.settings.scanPaths.size()))
        return;

    registry.settings.scanPaths.erase (registry.settings.scanPaths.begin() + row);
    saveRegistry();
    refreshLists();
    refreshStatusText();
}

void PluginScanComponent::resetPaths()
{
    registry.resetPathsToDefaults (currentPlatform(), userHomeDirectory().getFullPathName().toStdString());
    saveRegistry();
    refreshLists();
    refreshStatusText();
}

void PluginScanComponent::scanPlugins (bool clearFirst)
{
    if (scanner && scanner->snapshot().running)
        return;

    if (clearFirst)
        registry.clearCachePreservingPaths();

    std::vector<samplebench::FailedPluginDescription> blockedPlugins;
    if (! clearFirst)
    {
        blockedPlugins = registry.failedPlugins;
        blockedPlugins.insert (blockedPlugins.end(), registry.blockedPlugins.begin(), registry.blockedPlugins.end());
    }

    scanner = std::make_unique<PluginScanService> (registry.settings.scanPaths, std::move (blockedPlugins));
    scanner->start();
    refreshStatusText();
}

void PluginScanComponent::cancelScan()
{
    if (scanner)
        scanner->cancel();
    finishScanIfReady();
}

void PluginScanComponent::clearCache()
{
    registry.clearCachePreservingPaths();
    saveRegistry();
    refreshLists();
    refreshStatusText();
}

void PluginScanComponent::clearFailed()
{
    registry.clearFailedPlugins();
    saveRegistry();
    refreshLists();
    refreshStatusText();
}

void PluginScanComponent::finishScanIfReady()
{
    if (! scanner)
        return;

    const auto snapshot = scanner->snapshot();
    if (! snapshot.finished)
        return;

    registry.foundPlugins = snapshot.foundPlugins;
    registry.failedPlugins = snapshot.failedPlugins;
    registry.blockedPlugins = snapshot.blockedPlugins;
    registry.cachedScanPaths = registry.settings.scanPaths;
    registry.lastScanTime = isoNow().toStdString();
    registry.scanHasRunThisSession = true;
    saveRegistry();
    scanner.reset();
    refreshLists();
}

samplebench::PluginScanPlatform PluginScanComponent::currentPlatform() const
{
   #if JUCE_MAC
    return samplebench::PluginScanPlatform::macOS;
   #elif JUCE_WINDOWS
    return samplebench::PluginScanPlatform::windows;
   #else
    return samplebench::PluginScanPlatform::other;
   #endif
}

juce::File PluginScanComponent::userHomeDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory);
}

juce::String PluginScanComponent::statusSummary() const
{
    if (scanner)
    {
        const auto snapshot = scanner->snapshot();
        if (snapshot.running)
            return snapshot.status + "  " + juce::String (static_cast<int> (snapshot.progress * 100.0)) + "%";
    }

    if (registry.lastScanTime.empty()
        && registry.foundPlugins.empty()
        && registry.failedPlugins.empty()
        && registry.blockedPlugins.empty())
        return "No plugin scan yet";

    auto text = "Found " + juce::String (static_cast<int> (registry.foundPlugins.size())) + " plugins";
    if (! registry.failedPlugins.empty())
        text += "  " + juce::String (static_cast<int> (registry.failedPlugins.size())) + " failed";
    if (! registry.blockedPlugins.empty())
        text += "  " + juce::String (static_cast<int> (registry.blockedPlugins.size())) + " skipped";
    if (! registry.lastScanTime.empty())
        text += "  Last scanned: " + juce::String::fromUTF8 (registry.lastScanTime.c_str());
    if (registry.rescanRecommended())
        text += "  Paths changed - rescan recommended";
    return text;
}

long long PluginScanComponent::modifiedTimeMillisForPath (const std::filesystem::path& path) const
{
    return pathToFile (path).getLastModificationTime().toMilliseconds();
}

PluginScanWindow::PluginScanWindow (juce::File cacheFile)
    : juce::DocumentWindow { "Settings / Preferences",
                             colour (samplebench::palette::panel),
                             juce::DocumentWindow::closeButton }
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setContentOwned (new PluginScanComponent { std::move (cacheFile) }, true);
    centreWithSize (880, 620);
    setVisible (true);
}

void PluginScanWindow::closeButtonPressed()
{
    setVisible (false);
}
