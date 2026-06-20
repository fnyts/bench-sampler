#include "model/SampleBenchModel.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>

namespace
{
void testPackStartsWithFourSamplerBuckets()
{
    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");
    const auto& buckets = pack.buckets();

    assert (pack.name() == "EP133_BREAKS_01");
    assert (buckets.size() == 4);
    assert (buckets[0].id == samplebench::BucketId::drums);
    assert (buckets[0].folderName == "A_DRUMS");
    assert (buckets[1].id == samplebench::BucketId::bass);
    assert (buckets[1].folderName == "B_BASS");
    assert (buckets[2].id == samplebench::BucketId::melody);
    assert (buckets[2].folderName == "C_MELODY");
    assert (buckets[3].id == samplebench::BucketId::other);
    assert (buckets[3].folderName == "D_OTHER");
}

void testImportAddsAudioFileToRequestedBucket()
{
    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");

    const auto imported = pack.importSample (samplebench::BucketId::drums,
                                             std::filesystem::path { "/samples/amen.wav" });

    const auto& drums = pack.samplesInBucket (samplebench::BucketId::drums);
    assert (drums.size() == 1);
    assert (drums.front().id == imported.id);
    assert (drums.front().sourcePath == std::filesystem::path { "/samples/amen.wav" });
    assert (drums.front().displayName == "amen.wav");
    assert (drums.front().shortName == "amen");
    assert (drums.front().bucket == samplebench::BucketId::drums);
    assert (drums.front().type == samplebench::SampleType::oneShot);
}

void testPackReportsWhetherItHasImportedSamples()
{
    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");
    assert (! pack.hasSamples());

    pack.importSample (samplebench::BucketId::drums, "/samples/amen.wav");

    assert (pack.hasSamples());
}

void testSelectingSampleMakesItAvailableForBenchView()
{
    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");
    const auto amen = pack.importSample (samplebench::BucketId::drums, "/samples/amen.wav");
    const auto sub = pack.importSample (samplebench::BucketId::bass, "/samples/sub.wav");

    assert (! pack.selectedSample().has_value());

    const auto selected = pack.selectSample (sub.id);

    assert (selected);
    assert (pack.selectedSample().has_value());
    assert (pack.selectedSample()->id == sub.id);
    assert (pack.selectedSample()->bucket == samplebench::BucketId::bass);
    assert (pack.selectedSample()->displayName == "sub.wav");
    assert (pack.samplesInBucket (samplebench::BucketId::drums).front().id == amen.id);
}

void testClearingSelectionLeavesBenchWithoutSample()
{
    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");
    const auto amen = pack.importSample (samplebench::BucketId::drums, "/samples/amen.wav");
    assert (pack.selectSample (amen.id));
    assert (pack.selectedSample().has_value());

    pack.clearSelection();

    assert (! pack.selectedSample().has_value());
}

void testMovingSampleToBucketUpdatesBucketMembership()
{
    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");
    const auto melody = pack.importSample (samplebench::BucketId::drums, "/samples/melody.wav");

    assert (pack.moveSampleToBucket (melody.id, samplebench::BucketId::melody));

    assert (pack.samplesInBucket (samplebench::BucketId::drums).empty());
    const auto& melodies = pack.samplesInBucket (samplebench::BucketId::melody);
    assert (melodies.size() == 1);
    assert (melodies.front().id == melody.id);
    assert (melodies.front().bucket == samplebench::BucketId::melody);
    assert (melodies.front().sourcePath == melody.sourcePath);
}

void testMovingSelectedSampleKeepsSelectionValid()
{
    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");
    const auto melody = pack.importSample (samplebench::BucketId::drums, "/samples/melody.wav");
    assert (pack.selectSample (melody.id));

    assert (pack.moveSampleToBucket (melody.id, samplebench::BucketId::melody));

    assert (pack.selectedSample().has_value());
    assert (pack.selectedSample()->id == melody.id);
    assert (pack.selectedSample()->bucket == samplebench::BucketId::melody);
}

void testMovingSampleToSameBucketIsNoOpSuccess()
{
    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");
    const auto kick = pack.importSample (samplebench::BucketId::drums, "/samples/kick.wav");

    assert (pack.moveSampleToBucket (kick.id, samplebench::BucketId::drums));

    assert (pack.samplesInBucket (samplebench::BucketId::drums).size() == 1);
    assert (pack.samplesInBucket (samplebench::BucketId::drums).front().id == kick.id);
}

void testMovingMissingSampleFails()
{
    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");

    assert (! pack.moveSampleToBucket (999, samplebench::BucketId::melody));
}

void testPackSessionPersistsAndReloads()
{
    const auto file = std::filesystem::temp_directory_path() / "sampler_food_bench_pack_session_test.sfbpack";
    std::filesystem::remove (file);

    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");
    const auto amen = pack.importSample (samplebench::BucketId::drums, "/samples/amen.wav");
    const auto sub = pack.importSample (samplebench::BucketId::bass, "/samples/sub.wav");

    samplebench::BenchSettings settings = amen.bench;
    settings.type = samplebench::SampleType::loop;
    settings.musicalBpm = 92.0;
    settings.bars = 4;
    settings.key = "Am";
    settings.capture.captureStartBar = 2;
    settings.capture.warmupBars = 1;
    settings.capture.keepBars = 8;
    settings.capture.tailBars = 2;
    settings.name = "amen_808";
    settings.flavor = samplebench::RenderFlavor::wet;
    settings.version = 3;
    settings.speedTrickEnabled = true;
    settings.gainEnabled = true;
    settings.gainDecibels = 3.5f;
    settings.crushEnabled = true;
    settings.crushBits = 9;
    settings.customEffectChain = true;
    settings.effectChain = { samplebench::BuiltInEffectId::gain, samplebench::BuiltInEffectId::crush };
    settings.sourceBedMode = samplebench::SourceBedMode::extendForFx;
    settings.bedLengthBars = 32;
    settings.bedTriggerMode = samplebench::BedTriggerMode::oncePerBar;
    assert (pack.updateBenchSettings (amen.id, settings));

    samplebench::RenderedVariation variation;
    variation.filePath = "/exports/amen_808_wet_v3_spdup_092BPM.wav";
    variation.displayName = "amen_808_wet_v3_spdup_092BPM.wav";
    variation.finalFilename = "amen_808_wet_v3_spdup_092BPM.wav";
    variation.settings = settings;
    assert (pack.keepVariation (amen.id, variation));
    assert (pack.selectSample (sub.id));

    assert (pack.saveSessionToDisk (file));

    const auto loaded = samplebench::Pack::loadSessionFromDisk (file);
    assert (loaded.has_value());
    assert (loaded->name() == "EP133_BREAKS_01");
    assert (loaded->samplesInBucket (samplebench::BucketId::drums).size() == 1);
    assert (loaded->samplesInBucket (samplebench::BucketId::bass).size() == 1);

    const auto& loadedAmen = loaded->samplesInBucket (samplebench::BucketId::drums).front();
    assert (loadedAmen.id == amen.id);
    assert (loadedAmen.sourcePath == std::filesystem::path { "/samples/amen.wav" });
    assert (loadedAmen.bench.name == "amen_808");
    assert (loadedAmen.bench.musicalBpm == 92.0);
    assert (loadedAmen.bench.capture.keepBars == 8);
    assert (loadedAmen.bench.flavor == samplebench::RenderFlavor::wet);
    assert (loadedAmen.bench.speedTrickEnabled);
    assert (loadedAmen.bench.effectChain.size() == 2);
    assert (loadedAmen.bench.sourceBedMode == samplebench::SourceBedMode::extendForFx);
    assert (loadedAmen.variations.size() == 1);
    assert (loadedAmen.variations.front().finalFilename == "amen_808_wet_v3_spdup_092BPM.wav");

    assert (loaded->selectedSample().has_value());
    assert (loaded->selectedSample()->id == sub.id);
    assert (loaded->selectedSample()->bucket == samplebench::BucketId::bass);

    std::filesystem::remove (file);
}

void testPackRenameChangesDisplayName()
{
    auto pack = samplebench::Pack::create ("UNTITLED_PACK");

    assert (pack.rename ("EP133 Breaks 01"));

    assert (pack.name() == "EP133 Breaks 01");
}

void testPackRenameRejectsEmptyName()
{
    auto pack = samplebench::Pack::create ("EP133 Breaks 01");

    assert (! pack.rename ("   "));

    assert (pack.name() == "EP133 Breaks 01");
}

void testSafeExportFolderNameSanitizesInvalidCharacters()
{
    assert (samplebench::safeExportFolderName ("EP133 Breaks 01") == "EP133_Breaks_01");
    assert (samplebench::safeExportFolderName ("Breaks:/\\*? Pack") == "Breaks_Pack");
    assert (samplebench::safeExportFolderName ("   ") == "UNTITLED_PACK");
}

void testPackExportCreatesBucketFoldersAndNotes()
{
    const auto root = std::filesystem::temp_directory_path() / "sampler_food_bench_export_structure_test";
    std::filesystem::remove_all (root);
    std::filesystem::create_directories (root);

    auto pack = samplebench::Pack::create ("EP133 Breaks 01");

    samplebench::ExportOptions options;
    const auto result = samplebench::exportPackToFolder (pack, root, options, "2026-06-20 13:05");

    assert (result.success);
    assert (std::filesystem::is_directory (result.folderPath / "A_DRUMS"));
    assert (std::filesystem::is_directory (result.folderPath / "B_BASS"));
    assert (std::filesystem::is_directory (result.folderPath / "C_MELODY"));
    assert (std::filesystem::is_directory (result.folderPath / "D_OTHER"));
    assert (std::filesystem::is_regular_file (result.folderPath / "EP133_Breaks_01_notes.txt"));

    std::filesystem::remove_all (root);
}

void testPackExportIncludesKeptBouncesOnly()
{
    const auto root = std::filesystem::temp_directory_path() / "sampler_food_bench_export_kept_test";
    const auto sourceDir = std::filesystem::temp_directory_path() / "sampler_food_bench_export_sources";
    std::filesystem::remove_all (root);
    std::filesystem::remove_all (sourceDir);
    std::filesystem::create_directories (sourceDir);
    const auto keptFile = sourceDir / "kept.wav";
    const auto unkeptFile = sourceDir / "render_preview.wav";
    {
        std::ofstream kept { keptFile, std::ios::binary };
        kept << "kept bounce";
        std::ofstream unkept { unkeptFile, std::ios::binary };
        unkept << "unkept preview";
    }

    auto pack = samplebench::Pack::create ("EP133 Breaks 01");
    const auto amen = pack.importSample (samplebench::BucketId::drums, "/samples/amen.wav");

    samplebench::BenchSettings settings = amen.bench;
    settings.type = samplebench::SampleType::loop;
    settings.musicalBpm = 150.0;
    settings.name = "amen";
    settings.flavor = samplebench::RenderFlavor::wet;

    samplebench::RenderedVariation kept;
    kept.filePath = keptFile;
    kept.finalFilename = "amen_wet_v1_150BPM.wav";
    kept.displayName = kept.finalFilename;
    kept.sourceSampleId = amen.id;
    kept.sourcePath = amen.sourcePath;
    kept.settings = settings;
    assert (pack.keepVariation (amen.id, kept));

    samplebench::ExportOptions options;
    const auto result = samplebench::exportPackToFolder (pack, root, options, "2026-06-20 13:05");

    assert (result.success);
    assert (result.exportedFiles == 1);
    assert (std::filesystem::is_regular_file (result.folderPath / "A_DRUMS" / "amen_wet_v1_150BPM.wav"));
    assert (! std::filesystem::exists (result.folderPath / "A_DRUMS" / "render_preview.wav"));

    std::filesystem::remove_all (root);
    std::filesystem::remove_all (sourceDir);
}

void testPackExportNotesContainPackNameAndBucketSections()
{
    const auto root = std::filesystem::temp_directory_path() / "sampler_food_bench_export_notes_test";
    std::filesystem::remove_all (root);

    auto pack = samplebench::Pack::create ("EP133 Breaks 01");
    samplebench::ExportOptions options;
    const auto result = samplebench::exportPackToFolder (pack, root, options, "2026-06-20 13:05");
    const auto notes = result.folderPath / "EP133_Breaks_01_notes.txt";

    std::ifstream stream { notes };
    const std::string text { std::istreambuf_iterator<char> { stream }, std::istreambuf_iterator<char> {} };

    assert (text.find ("Pack: EP133 Breaks 01") != std::string::npos);
    assert (text.find ("[A_DRUMS]") != std::string::npos);
    assert (text.find ("[B_BASS]") != std::string::npos);
    assert (text.find ("No sampler upload or device control.") != std::string::npos);

    std::filesystem::remove_all (root);
}

void testDuplicateExportFilenamesPreserveBpmLast()
{
    const auto root = std::filesystem::temp_directory_path() / "sampler_food_bench_export_duplicates_test";
    const auto sourceDir = std::filesystem::temp_directory_path() / "sampler_food_bench_export_duplicate_sources";
    std::filesystem::remove_all (root);
    std::filesystem::remove_all (sourceDir);
    std::filesystem::create_directories (sourceDir);
    const auto firstFile = sourceDir / "first.wav";
    const auto secondFile = sourceDir / "second.wav";
    {
        std::ofstream first { firstFile, std::ios::binary };
        first << "first";
        std::ofstream second { secondFile, std::ios::binary };
        second << "second";
    }

    auto pack = samplebench::Pack::create ("EP133 Breaks 01");
    const auto first = pack.importSample (samplebench::BucketId::drums, "/samples/first.wav");
    const auto second = pack.importSample (samplebench::BucketId::drums, "/samples/second.wav");

    samplebench::RenderedVariation firstVariation;
    firstVariation.filePath = firstFile;
    firstVariation.finalFilename = "blaze_dry_v1_140BPM.wav";
    firstVariation.displayName = firstVariation.finalFilename;
    firstVariation.sourceSampleId = first.id;
    firstVariation.sourcePath = first.sourcePath;
    assert (pack.keepVariation (first.id, firstVariation));

    samplebench::RenderedVariation secondVariation = firstVariation;
    secondVariation.filePath = secondFile;
    secondVariation.sourceSampleId = second.id;
    secondVariation.sourcePath = second.sourcePath;
    assert (pack.keepVariation (second.id, secondVariation));

    samplebench::ExportOptions options;
    const auto result = samplebench::exportPackToFolder (pack, root, options, "2026-06-20 13:05");

    assert (result.success);
    assert (std::filesystem::is_regular_file (result.folderPath / "A_DRUMS" / "blaze_dry_v1_140BPM.wav"));
    assert (std::filesystem::is_regular_file (result.folderPath / "A_DRUMS" / "blaze_dry_v1_02_140BPM.wav"));
    assert (! std::filesystem::exists (result.folderPath / "A_DRUMS" / "blaze_dry_v1_140BPM_02.wav"));

    std::filesystem::remove_all (root);
    std::filesystem::remove_all (sourceDir);
}

void testDefaultMacVst3PathsIncludeSystemAndUserLocations()
{
    const auto paths = samplebench::defaultVst3ScanPaths (samplebench::PluginScanPlatform::macOS,
                                                          std::filesystem::path { "/Users/alex" });

    assert (paths.size() == 2);
    assert (paths[0] == std::filesystem::path { "/Library/Audio/Plug-Ins/VST3" });
    assert (paths[1] == std::filesystem::path { "/Users/alex/Library/Audio/Plug-Ins/VST3" });
}

void testDefaultWindowsVst3PathSuggestionIsCustomizable()
{
    auto settings = samplebench::PluginScanSettings::withPlatformDefaults (samplebench::PluginScanPlatform::windows,
                                                                           std::filesystem::path {});

    assert (settings.scanPaths.size() == 1);
    assert (settings.scanPaths[0] == std::filesystem::path { "C:\\Program Files\\Common Files\\VST3" });

    const auto added = settings.addPath (std::filesystem::path { "D:\\Audio\\VST3" });

    assert (added);
    assert (settings.scanPaths.size() == 2);
    assert (settings.scanPaths[1] == std::filesystem::path { "D:\\Audio\\VST3" });
}

void testPluginScanPathsIgnoreDuplicatesAndCanRemoveCustomPaths()
{
    samplebench::PluginScanSettings settings;

    assert (settings.addPath ("/Library/Audio/Plug-Ins/VST3"));
    assert (! settings.addPath ("/Library/Audio/Plug-Ins/VST3/"));
    assert (settings.scanPaths.size() == 1);

    assert (settings.removePath ("/Library/Audio/Plug-Ins/VST3"));
    assert (settings.scanPaths.empty());
}

void testPluginScanPathsPersistAndReload()
{
    const auto file = std::filesystem::temp_directory_path() / "sampler_food_bench_plugin_paths_test.txt";
    std::filesystem::remove (file);

    samplebench::PluginRegistry registry;
    registry.settings.addPath ("/Library/Audio/Plug-Ins/VST3");
    registry.settings.addPath ("/Users/alex/Library/Audio/Plug-Ins/VST3");

    assert (registry.saveToDisk (file));

    const auto loaded = samplebench::PluginRegistry::loadFromDisk (file);

    assert (loaded.settings.scanPaths == registry.settings.scanPaths);
    std::filesystem::remove (file);
}

void testFoundAndFailedPluginCachePersistsAndReloads()
{
    const auto file = std::filesystem::temp_directory_path() / "sampler_food_bench_plugin_cache_test.txt";
    std::filesystem::remove (file);

    samplebench::PluginRegistry registry;
    registry.settings.addPath ("/Library/Audio/Plug-Ins/VST3");
    registry.lastScanTime = "2026-06-19T20:30:00Z";
    registry.foundPlugins.push_back ({ "Space Verb",
                                       "Acme Audio",
                                       "VST3",
                                       "Reverb",
                                       "/Library/Audio/Plug-Ins/VST3/Space Verb.vst3",
                                       "acme.spaceverb",
                                       123456 });
    registry.failedPlugins.push_back ({ "Crash Synth",
                                        "Unknown",
                                        "VST3",
                                        "/Library/Audio/Plug-Ins/VST3/Crash Synth.vst3",
                                        "Scan failed",
                                        987654 });
    registry.blockedPlugins.push_back ({ "Blocked Delay",
                                         "Unknown",
                                         "VST3",
                                         "/Library/Audio/Plug-Ins/VST3/Blocked Delay.vst3",
                                         "Skipped until Rescan All",
                                         333444 });

    assert (registry.saveToDisk (file));

    const auto loaded = samplebench::PluginRegistry::loadFromDisk (file);

    assert (loaded.lastScanTime == "2026-06-19T20:30:00Z");
    assert (loaded.foundPlugins.size() == 1);
    assert (loaded.foundPlugins[0].name == "Space Verb");
    assert (loaded.foundPlugins[0].manufacturer == "Acme Audio");
    assert (loaded.foundPlugins[0].category == "Reverb");
    assert (loaded.foundPlugins[0].filePath == std::filesystem::path { "/Library/Audio/Plug-Ins/VST3/Space Verb.vst3" });
    assert (loaded.foundPlugins[0].uniqueId == "acme.spaceverb");
    assert (loaded.foundPlugins[0].modifiedTimeMillis == 123456);
    assert (loaded.failedPlugins.size() == 1);
    assert (loaded.failedPlugins[0].name == "Crash Synth");
    assert (loaded.failedPlugins[0].reason == "Scan failed");
    assert (loaded.failedPlugins[0].modifiedTimeMillis == 987654);
    assert (loaded.blockedPlugins.size() == 1);
    assert (loaded.blockedPlugins[0].name == "Blocked Delay");
    assert (loaded.blockedPlugins[0].reason == "Skipped until Rescan All");
    assert (loaded.blockedPlugins[0].modifiedTimeMillis == 333444);
    std::filesystem::remove (file);
}

void testStartupLoadsPluginCacheWithoutScanning()
{
    const auto file = std::filesystem::temp_directory_path() / "sampler_food_bench_plugin_startup_cache_test.txt";
    std::filesystem::remove (file);

    samplebench::PluginRegistry registry;
    registry.foundPlugins.push_back ({ "Cached EQ", "Acme", "VST3", "EQ", "/tmp/Cached EQ.vst3", "cached.eq", 42 });
    assert (registry.saveToDisk (file));

    const auto loaded = samplebench::PluginRegistry::loadFromDisk (file);

    assert (loaded.foundPlugins.size() == 1);
    assert (loaded.scanHasRunThisSession == false);
    std::filesystem::remove (file);
}

void testChangingPluginScanPathsMarksRescanRecommended()
{
    samplebench::PluginRegistry registry;
    registry.cachedScanPaths = { "/Library/Audio/Plug-Ins/VST3" };
    registry.settings.scanPaths = { "/Library/Audio/Plug-Ins/VST3" };

    assert (! registry.rescanRecommended());

    registry.settings.addPath ("/Users/alex/Library/Audio/Plug-Ins/VST3");

    assert (registry.rescanRecommended());
}

void testClearPluginCachePreservesPaths()
{
    samplebench::PluginRegistry registry;
    registry.settings.addPath ("/Library/Audio/Plug-Ins/VST3");
    registry.cachedScanPaths = registry.settings.scanPaths;
    registry.lastScanTime = "2026-06-19T20:30:00Z";
    registry.foundPlugins.push_back ({ "Cached EQ", "Acme", "VST3", "EQ", "/tmp/Cached EQ.vst3", "cached.eq", 42 });
    registry.failedPlugins.push_back ({ "Bad", "Acme", "VST3", "/tmp/Bad.vst3", "Failed", 43 });

    registry.clearCachePreservingPaths();

    assert (registry.settings.scanPaths.size() == 1);
    assert (registry.foundPlugins.empty());
    assert (registry.failedPlugins.empty());
    assert (registry.lastScanTime.empty());
    assert (registry.cachedScanPaths.empty());
}

void testResetPluginPathsRestoresPlatformDefaults()
{
    samplebench::PluginRegistry registry;
    registry.settings.addPath ("/Custom/VST3");

    registry.resetPathsToDefaults (samplebench::PluginScanPlatform::macOS, "/Users/alex");

    assert (registry.settings.scanPaths.size() == 2);
    assert (registry.settings.scanPaths[0] == std::filesystem::path { "/Library/Audio/Plug-Ins/VST3" });
    assert (registry.settings.scanPaths[1] == std::filesystem::path { "/Users/alex/Library/Audio/Plug-Ins/VST3" });
    assert (registry.rescanRecommended());
}
}

int main()
{
    testPackStartsWithFourSamplerBuckets();
    testImportAddsAudioFileToRequestedBucket();
    testPackReportsWhetherItHasImportedSamples();
    testSelectingSampleMakesItAvailableForBenchView();
    testClearingSelectionLeavesBenchWithoutSample();
    testMovingSampleToBucketUpdatesBucketMembership();
    testMovingSelectedSampleKeepsSelectionValid();
    testMovingSampleToSameBucketIsNoOpSuccess();
    testMovingMissingSampleFails();
    testPackSessionPersistsAndReloads();
    testPackRenameChangesDisplayName();
    testPackRenameRejectsEmptyName();
    testSafeExportFolderNameSanitizesInvalidCharacters();
    testPackExportCreatesBucketFoldersAndNotes();
    testPackExportIncludesKeptBouncesOnly();
    testPackExportNotesContainPackNameAndBucketSections();
    testDuplicateExportFilenamesPreserveBpmLast();
    testDefaultMacVst3PathsIncludeSystemAndUserLocations();
    testDefaultWindowsVst3PathSuggestionIsCustomizable();
    testPluginScanPathsIgnoreDuplicatesAndCanRemoveCustomPaths();
    testPluginScanPathsPersistAndReload();
    testFoundAndFailedPluginCachePersistsAndReloads();
    testStartupLoadsPluginCacheWithoutScanning();
    testChangingPluginScanPathsMarksRescanRecommended();
    testClearPluginCachePreservesPaths();
    testResetPluginPathsRestoresPlatformDefaults();

    std::cout << "SampleBenchModelTests passed\n";
    return 0;
}
