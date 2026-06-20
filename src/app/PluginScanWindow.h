#pragma once

#include "model/SampleBenchModel.h"

#include <atomic>
#include <memory>
#include <mutex>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

class PluginScanService final : private juce::Thread
{
public:
    struct Snapshot
    {
        std::vector<samplebench::CachedPluginDescription> foundPlugins;
        std::vector<samplebench::FailedPluginDescription> failedPlugins;
        std::vector<samplebench::FailedPluginDescription> blockedPlugins;
        juce::String status;
        double progress = 0.0;
        bool running = false;
        bool finished = false;
        bool cancelled = false;
    };

    PluginScanService (std::vector<std::filesystem::path> scanPaths,
                       std::vector<samplebench::FailedPluginDescription> blockedPluginsToSkip);
    ~PluginScanService() override;

    void start();
    void cancel();
    [[nodiscard]] Snapshot snapshot() const;

private:
    void run() override;
    void updateStatus (juce::String status, double progress);

    std::vector<std::filesystem::path> paths;
    std::vector<samplebench::FailedPluginDescription> blockedPlugins;
    mutable std::mutex snapshotMutex;
    Snapshot current;
};

class PluginScanComponent;

enum class PluginScanListKind
{
    paths,
    found,
    failed
};

class PluginScanListModel final : public juce::ListBoxModel
{
public:
    PluginScanListModel (PluginScanComponent& owner, PluginScanListKind kind);

    int getNumRows() override;
    void paintListBoxItem (int rowNumber,
                           juce::Graphics& g,
                           int width,
                           int height,
                           bool rowIsSelected) override;

private:
    PluginScanComponent& component;
    PluginScanListKind listKind;
};

class PluginScanComponent final : public juce::Component,
                                  private juce::Timer
{
public:
    explicit PluginScanComponent (juce::File cacheFile);
    ~PluginScanComponent() override;

    void resized() override;

private:
    friend class PluginScanListModel;

    [[nodiscard]] int getNumRowsForList (PluginScanListKind kind) const;
    void paintListRow (PluginScanListKind kind,
                       int rowNumber,
                       juce::Graphics& g,
                       int width,
                       int height,
                       bool rowIsSelected);
    void timerCallback() override;

    void loadRegistry();
    void saveRegistry();
    void refreshLists();
    void refreshStatusText();
    void addFolder();
    void removeSelectedFolder();
    void resetPaths();
    void scanPlugins (bool clearFirst);
    void cancelScan();
    void clearCache();
    void clearFailed();
    void finishScanIfReady();
    [[nodiscard]] samplebench::PluginScanPlatform currentPlatform() const;
    [[nodiscard]] juce::File userHomeDirectory() const;
    [[nodiscard]] juce::String statusSummary() const;
    [[nodiscard]] long long modifiedTimeMillisForPath (const std::filesystem::path& path) const;

    juce::File registryFile;
    samplebench::PluginRegistry registry;
    std::unique_ptr<PluginScanService> scanner;
    std::unique_ptr<juce::FileChooser> folderChooser;
    PluginScanListModel pathsModel;
    PluginScanListModel foundModel;
    PluginScanListModel failedModel;

    juce::Label titleLabel;
    juce::Label explainerLabel;
    juce::Label pathsLabel;
    juce::Label foundLabel;
    juce::Label failedLabel;
    juce::Label statusLabel;
    juce::ListBox pathsList { "Plugin Paths", &pathsModel };
    juce::ListBox foundList { "Found Plugins", &foundModel };
    juce::ListBox failedList { "Failed Plugins", &failedModel };
    juce::TextButton addFolderButton { "Add Folder" };
    juce::TextButton removeFolderButton { "Remove Folder" };
    juce::TextButton resetPathsButton { "Reset Paths" };
    juce::TextButton scanButton { "Scan Plugins" };
    juce::TextButton rescanButton { "Rescan All" };
    juce::TextButton cancelButton { "Stop Scan" };
    juce::TextButton clearCacheButton { "Clear Cache" };
    juce::TextButton clearFailedButton { "Clear Failed" };
};

class PluginScanWindow final : public juce::DocumentWindow
{
public:
    explicit PluginScanWindow (juce::File cacheFile);
    void closeButtonPressed() override;
};
