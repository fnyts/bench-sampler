#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace samplebench
{
enum class BucketId
{
    drums,
    bass,
    melody,
    other
};

enum class SampleType
{
    oneShot,
    loop,
    texture
};

enum class RenderFlavor
{
    dry,
    wet
};

enum class SourceBedMode
{
    asIs,
    extendForFx
};

enum class BedTriggerMode
{
    loopContinuously,
    oncePerBar
};

enum class BuiltInEffectId
{
    gain,
    mono,
    normalize,
    limit,
    compressor,
    crush,
    filter,
    drive,
    eq,
    delay,
    reverb,
    tape
};

enum class FilterMode
{
    lowPass,
    highPass
};

enum class EffectProcessMode
{
    preview,
    bounce
};

enum class PlaybackTarget
{
    none,
    source,
    bounce
};

enum class PluginScanPlatform
{
    macOS,
    windows,
    other
};

struct PluginScanSettings
{
    std::vector<std::filesystem::path> scanPaths;

    [[nodiscard]] static PluginScanSettings withPlatformDefaults (PluginScanPlatform platform,
                                                                  const std::filesystem::path& homeDirectory);
    bool addPath (std::filesystem::path path);
    bool removePath (const std::filesystem::path& path);
    void resetToPlatformDefaults (PluginScanPlatform platform, const std::filesystem::path& homeDirectory);
};

struct CachedPluginDescription
{
    std::string name;
    std::string manufacturer;
    std::string format = "VST3";
    std::string category;
    std::filesystem::path filePath;
    std::string uniqueId;
    long long modifiedTimeMillis = 0;
};

struct FailedPluginDescription
{
    std::string name;
    std::string manufacturer;
    std::string format = "VST3";
    std::filesystem::path filePath;
    std::string reason;
    long long modifiedTimeMillis = 0;
};

struct PluginRegistry
{
    static constexpr int currentCacheVersion = 1;

    PluginScanSettings settings;
    std::vector<std::filesystem::path> cachedScanPaths;
    std::vector<CachedPluginDescription> foundPlugins;
    std::vector<FailedPluginDescription> failedPlugins;
    std::vector<FailedPluginDescription> blockedPlugins;
    std::string lastScanTime;
    std::string scanAppVersion = "0.1.0";
    int cacheVersion = currentCacheVersion;
    bool scanHasRunThisSession = false;

    [[nodiscard]] static PluginRegistry loadFromDisk (const std::filesystem::path& filePath);
    [[nodiscard]] bool saveToDisk (const std::filesystem::path& filePath) const;
    [[nodiscard]] bool rescanRecommended() const;
    void clearCachePreservingPaths();
    void clearFailedPlugins();
    void resetPathsToDefaults (PluginScanPlatform platform, const std::filesystem::path& homeDirectory);
};

[[nodiscard]] std::vector<std::filesystem::path> defaultVst3ScanPaths (PluginScanPlatform platform,
                                                                       const std::filesystem::path& homeDirectory);

enum class FxModuleKind
{
    builtIn,
    plugin
};

enum class PluginModuleStatus
{
    loaded,
    missing,
    failed
};

struct HostedPluginModule
{
    std::string name;
    std::string manufacturer;
    std::string format = "VST3";
    std::string category;
    std::filesystem::path filePath;
    std::string uniqueId;
    long long modifiedTimeMillis = 0;
    bool enabled = true;
    PluginModuleStatus status = PluginModuleStatus::loaded;
    std::string errorMessage;
    std::vector<std::uint8_t> stateBlob;
};

struct FxModule
{
    FxModuleKind kind = FxModuleKind::builtIn;
    BuiltInEffectId builtIn = BuiltInEffectId::gain;
    HostedPluginModule plugin;
};

struct PlaybackState
{
    PlaybackTarget target = PlaybackTarget::none;
    PlaybackTarget previewTarget = PlaybackTarget::source;
    bool isPlaying = false;
    bool loopEnabled = false;
};

struct CaptureSettings
{
    int captureStartBar = 1;
    int warmupBars = 0;
    int keepBars = 4;
    int tailBars = 0;
};

struct BenchSettings
{
    SampleType type = SampleType::oneShot;
    double musicalBpm = 0.0;
    int bars = 0;
    std::string key;
    CaptureSettings capture;
    std::string name;
    RenderFlavor flavor = RenderFlavor::dry;
    int version = 1;
    bool speedTrickEnabled = false;
    bool gainEnabled = true;
    float gainDecibels = 0.0f;
    bool normalizeEnabled = false;
    float normalizeTargetDecibels = -1.0f;
    bool monoEnabled = false;
    bool limitEnabled = false;
    float limitCeilingDecibels = -1.0f;
    float limitInputDecibels = 0.0f;
    float limitReleaseMs = 80.0f;
    bool compressorEnabled = false;
    float compressorThresholdDecibels = -18.0f;
    float compressorRatio = 4.0f;
    float compressorAttackMs = 10.0f;
    float compressorReleaseMs = 120.0f;
    float compressorMakeupDecibels = 0.0f;
    float compressorMix = 1.0f;
    bool crushEnabled = false;
    int crushBits = 12;
    double crushSampleRate = 22050.0;
    float crushMix = 1.0f;
    float crushOutputDecibels = 0.0f;
    bool filterEnabled = false;
    FilterMode filterMode = FilterMode::lowPass;
    float filterCutoffHz = 12000.0f;
    float filterResonance = 0.2f;
    bool driveEnabled = false;
    float driveAmount = 0.25f;
    float driveTone = 0.5f;
    float driveMix = 1.0f;
    float driveOutputDecibels = 0.0f;
    bool eqEnabled = false;
    float eqLowDecibels = 0.0f;
    float eqMidDecibels = 0.0f;
    float eqHighDecibels = 0.0f;
    bool delayEnabled = false;
    int delayDivision = 2;
    float delayFeedback = 0.25f;
    float delayMix = 0.2f;
    float delayTone = 0.35f;
    bool reverbEnabled = false;
    float reverbSize = 0.35f;
    float reverbDecaySeconds = 2.0f;
    float reverbMix = 0.2f;
    float reverbTone = 0.35f;
    bool tapeEnabled = false;
    float tapeDrive = 0.2f;
    float tapeWobble = 0.1f;
    float tapeTone = 0.35f;
    float tapeNoise = 0.0f;
    float tapeMix = 1.0f;
    bool customEffectChain = false;
    std::vector<BuiltInEffectId> effectChain;
    bool customFxModules = false;
    std::vector<FxModule> fxModules;
    SourceBedMode sourceBedMode = SourceBedMode::asIs;
    int bedLengthBars = 16;
    BedTriggerMode bedTriggerMode = BedTriggerMode::loopContinuously;
};

struct CaptureTiming
{
    double secondsPerBar = 0.0;
    double internalStartSeconds = 0.0;
    double keepStartSeconds = 0.0;
    double exportOffsetInInternalSeconds = 0.0;
    double internalDurationSeconds = 0.0;
    double exportedDurationSeconds = 0.0;
};

struct VisibleTimeRange
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
};

struct WaveformLaneLayout
{
    float timelineX = 0.0f;
    float timelineWidth = 0.0f;
    float labelX = 0.0f;
    float labelWidth = 0.0f;
};

struct UiRect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct FxDetailLayout
{
    UiRect utilityRow;
    UiRect knobLabelRow;
    UiRect knobRow;
    UiRect knobA;
    UiRect knobB;
    UiRect knobC;
    UiRect filterModeRow;
    UiRect hintRow;
};

struct CaptureRegions
{
    VisibleTimeRange warmup;
    VisibleTimeRange keep;
    VisibleTimeRange tail;
    VisibleTimeRange bounce;
    VisibleTimeRange render;
    double secondsPerBar = 0.0;
    int startBar = 1;
    int warmupBars = 0;
    int keepBars = 4;
    int tailBars = 0;
};

struct AudioBedRequest
{
    double sampleRate = 44100.0;
    double musicalBpm = 120.0;
    int bedLengthBars = 16;
    BedTriggerMode triggerMode = BedTriggerMode::loopContinuously;
};

struct AudioBufferData
{
    std::vector<std::vector<float>> channels;
    double sampleRate = 44100.0;
};

struct RenderedVariation
{
    std::filesystem::path filePath;
    std::string displayName;
    std::string finalFilename;
    std::size_t sourceSampleId = 0;
    std::filesystem::path sourcePath;
    BenchSettings settings;
};

struct ExportOptions
{
    bool includeKeptBounces = true;
    bool includeOriginalSources = false;
    bool includeNotesFile = true;
};

struct PackExportResult
{
    bool success = false;
    std::filesystem::path folderPath;
    int exportedFiles = 0;
    int skippedFiles = 0;
    std::vector<std::string> errors;
};

struct Bucket
{
    BucketId id;
    std::string folderName;
    std::string displayName;
};

struct Sample
{
    std::size_t id = 0;
    std::filesystem::path sourcePath;
    std::string displayName;
    std::string shortName;
    BucketId bucket = BucketId::other;
    SampleType type = SampleType::oneShot;
    std::optional<double> sourceBpm;
    std::optional<double> musicalBpm;
    std::optional<int> bars;
    std::string key;
    BenchSettings bench;
    std::vector<RenderedVariation> variations;
};

[[nodiscard]] std::string buildFinalFilename (const BenchSettings& settings);
[[nodiscard]] std::string safeExportFolderName (std::string displayName);
[[nodiscard]] PackExportResult exportPackToFolder (const class Pack& pack,
                                                   const std::filesystem::path& destinationDirectory,
                                                   const ExportOptions& options,
                                                   std::string exportTimestamp = {});
[[nodiscard]] CaptureTiming calculateCaptureTiming (double musicalBpm, const CaptureSettings& settings);
[[nodiscard]] CaptureRegions calculateCaptureRegions (double musicalBpm, const CaptureSettings& settings);
[[nodiscard]] VisibleTimeRange sourcePreviewLoopRegion (const CaptureRegions& regions);
[[nodiscard]] VisibleTimeRange bouncePreviewLoopRegion (double bounceDurationSeconds);
[[nodiscard]] double calculateBedDurationSeconds (double musicalBpm, int bedLengthBars);
[[nodiscard]] std::vector<float> buildMonoAudioBed (const std::vector<float>& source,
                                                    const AudioBedRequest& request);
[[nodiscard]] FxModule makeBuiltInFxModule (BuiltInEffectId effect);
[[nodiscard]] FxModule makePluginFxModule (const CachedPluginDescription& plugin);
[[nodiscard]] std::vector<BuiltInEffectId> fixedEffectChainOrder();
[[nodiscard]] std::vector<BuiltInEffectId> activeEffectChain (const BenchSettings& settings);
[[nodiscard]] std::vector<FxModule> defaultFxModules();
[[nodiscard]] std::vector<FxModule> activeFxModules (const BenchSettings& settings);
[[nodiscard]] std::vector<FxModule> resolvePluginModuleAvailability (const std::vector<FxModule>& modules,
                                                                     const std::vector<CachedPluginDescription>& availablePlugins);
[[nodiscard]] std::vector<HostedPluginModule> activePluginModules (const BenchSettings& settings);
[[nodiscard]] std::vector<std::string> fxMenuTopLevelOrder();
[[nodiscard]] AudioBufferData applyEffectChain (AudioBufferData audio,
                                                const BenchSettings& settings,
                                                EffectProcessMode mode);
[[nodiscard]] bool effectChainAffectsPreview (const BenchSettings& settings);
[[nodiscard]] std::string effectChainSummary (const BenchSettings& settings);
[[nodiscard]] std::string toString (RenderFlavor flavor);
[[nodiscard]] std::string suggestExportNameFromFilename (const std::filesystem::path& sourcePath);
[[nodiscard]] std::optional<int> extractBpmFromFilename (const std::filesystem::path& sourcePath);
[[nodiscard]] SampleType detectSampleTypeFromFilename (const std::filesystem::path& sourcePath);
[[nodiscard]] std::string middleTruncatePreservingEnding (const std::string& text, std::size_t maxCharacters);
[[nodiscard]] PlaybackState startPlayback (PlaybackState state, PlaybackTarget target);
[[nodiscard]] PlaybackState startSelectedPlayback (PlaybackState state);
[[nodiscard]] PlaybackState stopPlayback (PlaybackState state);
[[nodiscard]] PlaybackState setPreviewTarget (PlaybackState state, PlaybackTarget target);
[[nodiscard]] PlaybackState setLoopEnabled (PlaybackState state, bool enabled);
[[nodiscard]] double wrappedLoopPosition (double positionSeconds, VisibleTimeRange loop);
[[nodiscard]] VisibleTimeRange makeInitialVisibleWindow (double durationSeconds,
                                                         double focusStartSeconds,
                                                         double focusDurationSeconds);
[[nodiscard]] VisibleTimeRange autoFollowVisibleWindow (VisibleTimeRange visibleRange,
                                                        double playheadSeconds,
                                                        double durationSeconds);
[[nodiscard]] WaveformLaneLayout calculateWaveformLaneLayout (float areaX,
                                                              float areaWidth,
                                                              float channelLabelWidth);
[[nodiscard]] FxDetailLayout calculateFxDetailLayout (BuiltInEffectId effect,
                                                      int x,
                                                      int y,
                                                      int width,
                                                      int height);

class Pack
{
public:
    static Pack create (std::string name);
    [[nodiscard]] static std::optional<Pack> loadSessionFromDisk (const std::filesystem::path& filePath);

    [[nodiscard]] std::string_view name() const noexcept;
    bool rename (std::string newName);
    [[nodiscard]] const std::vector<Bucket>& buckets() const noexcept;
    [[nodiscard]] bool hasSamples() const;

    Sample importSample (BucketId bucket, std::filesystem::path sourcePath);
    [[nodiscard]] const std::vector<Sample>& samplesInBucket (BucketId bucket) const;

    bool selectSample (std::size_t sampleId);
    void clearSelection();
    bool moveSampleToBucket (std::size_t sampleId, BucketId targetBucket);
    [[nodiscard]] std::optional<Sample> selectedSample() const;
    bool updateBenchSettings (std::size_t sampleId, const BenchSettings& settings);
    bool keepVariation (std::size_t sampleId, RenderedVariation variation);
    [[nodiscard]] bool saveSessionToDisk (const std::filesystem::path& filePath) const;

private:
    explicit Pack (std::string name);
    [[nodiscard]] std::vector<Sample>& mutableSamplesInBucket (BucketId bucket);
    [[nodiscard]] Sample* findSample (std::size_t sampleId);
    [[nodiscard]] const Sample* findSample (std::size_t sampleId) const;

    std::string packName;
    std::vector<Bucket> packBuckets;
    std::vector<Sample> drums;
    std::vector<Sample> bass;
    std::vector<Sample> melody;
    std::vector<Sample> other;
    std::optional<std::size_t> selectedSampleId;
    std::size_t nextSampleId = 1;
};
}
