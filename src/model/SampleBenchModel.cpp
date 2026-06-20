#include "model/SampleBenchModel.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace samplebench
{
namespace
{
std::vector<Bucket> makeDefaultBuckets()
{
    return {
        { BucketId::drums, "A_DRUMS", "Drums" },
        { BucketId::bass, "B_BASS", "Bass" },
        { BucketId::melody, "C_MELODY", "Melody" },
        { BucketId::other, "D_OTHER", "Other" }
    };
}

std::string shortNameFromPath (const std::filesystem::path& sourcePath)
{
    const auto stem = sourcePath.stem().string();
    return stem.empty() ? sourcePath.filename().string() : stem;
}

std::string lowercase (std::string text)
{
    std::transform (text.begin(), text.end(), text.begin(), [] (unsigned char character)
    {
        return static_cast<char> (std::tolower (character));
    });
    return text;
}

std::string trimSeparators (std::string text)
{
    const auto isSeparator = [] (char character)
    {
        return character == '_' || character == '-' || character == ' ';
    };

    while (! text.empty() && isSeparator (text.front()))
        text.erase (text.begin());

    while (! text.empty() && isSeparator (text.back()))
        text.pop_back();

    return text;
}

std::string trimWhitespace (std::string text)
{
    const auto isWhitespace = [] (unsigned char character)
    {
        return std::isspace (character) != 0;
    };

    while (! text.empty() && isWhitespace (static_cast<unsigned char> (text.front())))
        text.erase (text.begin());

    while (! text.empty() && isWhitespace (static_cast<unsigned char> (text.back())))
        text.pop_back();

    return text;
}

std::string safeName (std::string name)
{
    if (name.empty())
        return "sample";

    std::replace (name.begin(), name.end(), ' ', '_');
    return name;
}

std::string sampleTypeText (SampleType type)
{
    switch (type)
    {
        case SampleType::oneShot: return "One-shot";
        case SampleType::loop: return "Loop";
        case SampleType::texture: return "Texture";
    }

    return "One-shot";
}

std::string sourceBedText (SourceBedMode mode)
{
    switch (mode)
    {
        case SourceBedMode::asIs: return "As-is";
        case SourceBedMode::extendForFx: return "Extend for FX";
    }

    return "As-is";
}

std::string triggerText (BedTriggerMode mode)
{
    switch (mode)
    {
        case BedTriggerMode::loopContinuously: return "Loop";
        case BedTriggerMode::oncePerBar: return "Once per bar";
    }

    return "Loop";
}

std::string barsText (int bars)
{
    return bars <= 0 ? "Off" : std::to_string (bars);
}

std::string duplicateFilenameForIndex (const std::string& filename, int duplicateIndex)
{
    if (duplicateIndex <= 1)
        return filename;

    const auto extensionStart = filename.find_last_of ('.');
    const auto stem = extensionStart == std::string::npos ? filename : filename.substr (0, extensionStart);
    const auto extension = extensionStart == std::string::npos ? std::string {} : filename.substr (extensionStart);

    std::ostringstream suffix;
    suffix << '_' << std::setw (2) << std::setfill ('0') << duplicateIndex;

    // Loop filenames intentionally keep BPM as the final semantic token so exported packs
    // stay easy to scan on hardware and in file browsers.
    static const std::regex bpmSuffix { R"((.*)_([0-9]{3}BPM)$)" };
    std::smatch match;
    if (std::regex_match (stem, match, bpmSuffix) && match.size() == 3)
        return match[1].str() + suffix.str() + "_" + match[2].str() + extension;

    return stem + suffix.str() + extension;
}

std::string paddedBpm (double bpm)
{
    std::ostringstream stream;
    stream << std::setw (3) << std::setfill ('0') << static_cast<int> (std::lround (bpm));
    return stream.str();
}

float dbToGain (float decibels)
{
    return std::pow (10.0f, decibels / 20.0f);
}

float clampSample (float sample)
{
    return std::clamp (sample, -1.0f, 1.0f);
}

float onePoleCoefficient (double frequencyHz, double sampleRate)
{
    const auto cutoff = std::clamp (frequencyHz, 20.0, std::max (20.0, sampleRate * 0.5 - 100.0));
    return static_cast<float> (std::exp (-2.0 * 3.14159265358979323846 * cutoff / sampleRate));
}

std::size_t samplesFromSeconds (double seconds, double sampleRate)
{
    return std::max<std::size_t> (1, static_cast<std::size_t> (std::round (std::max (0.0, seconds) * sampleRate)));
}

std::string comparablePath (const std::filesystem::path& path)
{
    auto text = path.lexically_normal().string();
    while (text.size() > 1 && (text.back() == '/' || text.back() == '\\'))
        text.pop_back();
    return text;
}

bool pathListsEqual (const std::vector<std::filesystem::path>& left,
                     const std::vector<std::filesystem::path>& right)
{
    if (left.size() != right.size())
        return false;

    for (std::size_t index = 0; index < left.size(); ++index)
        if (comparablePath (left[index]) != comparablePath (right[index]))
            return false;

    return true;
}

std::string escapeField (std::string text)
{
    // The app session/cache files are deliberately line-oriented: small, diffable,
    // and tolerant of older versions ignoring fields they do not understand.
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

std::vector<std::string> splitCacheLine (const std::string& line)
{
    std::vector<std::string> fields;
    std::string current;
    for (const auto character : line)
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

void writeCacheLine (std::ostream& stream, const std::vector<std::string>& fields)
{
    for (std::size_t index = 0; index < fields.size(); ++index)
    {
        if (index > 0)
            stream << '\t';
        stream << escapeField (fields[index]);
    }
    stream << '\n';
}

std::string toString (long long value)
{
    return std::to_string (value);
}

long long parseLongLong (const std::string& text)
{
    try
    {
        return std::stoll (text);
    }
    catch (...)
    {
        return 0;
    }
}

int parseInt (const std::string& text, int fallback)
{
    try
    {
        return std::stoi (text);
    }
    catch (...)
    {
        return fallback;
    }
}

double parseDouble (const std::string& text, double fallback)
{
    try
    {
        return std::stod (text);
    }
    catch (...)
    {
        return fallback;
    }
}

float parseFloat (const std::string& text, float fallback)
{
    return static_cast<float> (parseDouble (text, static_cast<double> (fallback)));
}

bool parseBool (const std::string& text, bool fallback = false)
{
    if (text == "1" || text == "true")
        return true;
    if (text == "0" || text == "false")
        return false;
    return fallback;
}

std::string boolString (bool value)
{
    return value ? "1" : "0";
}

template <typename Enum>
Enum enumFromIndex (const std::string& text, Enum fallback)
{
    return static_cast<Enum> (parseInt (text, static_cast<int> (fallback)));
}

std::string enumIndex (auto value)
{
    return std::to_string (static_cast<int> (value));
}

std::string optionalDoubleString (std::optional<double> value)
{
    return value.has_value() ? std::to_string (*value) : "";
}

std::string optionalIntString (std::optional<int> value)
{
    return value.has_value() ? std::to_string (*value) : "";
}

std::optional<double> parseOptionalDouble (const std::string& text)
{
    if (text.empty())
        return std::nullopt;
    return parseDouble (text, 0.0);
}

std::optional<int> parseOptionalInt (const std::string& text)
{
    if (text.empty())
        return std::nullopt;
    return parseInt (text, 0);
}

std::string bytesToHex (const std::vector<std::uint8_t>& bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve (bytes.size() * 2);
    for (const auto byte : bytes)
    {
        text += digits[(byte >> 4) & 0x0f];
        text += digits[byte & 0x0f];
    }
    return text;
}

std::vector<std::uint8_t> hexToBytes (const std::string& text)
{
    const auto hexValue = [] (char character) -> int
    {
        if (character >= '0' && character <= '9')
            return character - '0';
        if (character >= 'a' && character <= 'f')
            return character - 'a' + 10;
        if (character >= 'A' && character <= 'F')
            return character - 'A' + 10;
        return -1;
    };

    std::vector<std::uint8_t> bytes;
    if (text.size() % 2 != 0)
        return bytes;

    bytes.reserve (text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2)
    {
        const auto high = hexValue (text[index]);
        const auto low = hexValue (text[index + 1]);
        if (high < 0 || low < 0)
            return {};
        bytes.push_back (static_cast<std::uint8_t> ((high << 4) | low));
    }
    return bytes;
}

void appendBenchFields (std::vector<std::string>& fields, const BenchSettings& settings)
{
    fields.insert (fields.end(),
                   { enumIndex (settings.type),
                     std::to_string (settings.musicalBpm),
                     std::to_string (settings.bars),
                     settings.key,
                     std::to_string (settings.capture.captureStartBar),
                     std::to_string (settings.capture.warmupBars),
                     std::to_string (settings.capture.keepBars),
                     std::to_string (settings.capture.tailBars),
                     settings.name,
                     enumIndex (settings.flavor),
                     std::to_string (settings.version),
                     boolString (settings.speedTrickEnabled),
                     boolString (settings.gainEnabled),
                     std::to_string (settings.gainDecibels),
                     boolString (settings.normalizeEnabled),
                     std::to_string (settings.normalizeTargetDecibels),
                     boolString (settings.monoEnabled),
                     boolString (settings.limitEnabled),
                     std::to_string (settings.limitCeilingDecibels),
                     std::to_string (settings.limitInputDecibels),
                     std::to_string (settings.limitReleaseMs),
                     boolString (settings.compressorEnabled),
                     std::to_string (settings.compressorThresholdDecibels),
                     std::to_string (settings.compressorRatio),
                     std::to_string (settings.compressorAttackMs),
                     std::to_string (settings.compressorReleaseMs),
                     std::to_string (settings.compressorMakeupDecibels),
                     std::to_string (settings.compressorMix),
                     boolString (settings.crushEnabled),
                     std::to_string (settings.crushBits),
                     std::to_string (settings.crushSampleRate),
                     std::to_string (settings.crushMix),
                     std::to_string (settings.crushOutputDecibels),
                     boolString (settings.filterEnabled),
                     enumIndex (settings.filterMode),
                     std::to_string (settings.filterCutoffHz),
                     std::to_string (settings.filterResonance),
                     boolString (settings.driveEnabled),
                     std::to_string (settings.driveAmount),
                     std::to_string (settings.driveTone),
                     std::to_string (settings.driveMix),
                     std::to_string (settings.driveOutputDecibels),
                     boolString (settings.eqEnabled),
                     std::to_string (settings.eqLowDecibels),
                     std::to_string (settings.eqMidDecibels),
                     std::to_string (settings.eqHighDecibels),
                     boolString (settings.delayEnabled),
                     std::to_string (settings.delayDivision),
                     std::to_string (settings.delayFeedback),
                     std::to_string (settings.delayMix),
                     std::to_string (settings.delayTone),
                     boolString (settings.reverbEnabled),
                     std::to_string (settings.reverbSize),
                     std::to_string (settings.reverbDecaySeconds),
                     std::to_string (settings.reverbMix),
                     std::to_string (settings.reverbTone),
                     boolString (settings.tapeEnabled),
                     std::to_string (settings.tapeDrive),
                     std::to_string (settings.tapeWobble),
                     std::to_string (settings.tapeTone),
                     std::to_string (settings.tapeNoise),
                     std::to_string (settings.tapeMix),
                     boolString (settings.customEffectChain),
                     boolString (settings.customFxModules),
                     enumIndex (settings.sourceBedMode),
                     std::to_string (settings.bedLengthBars),
                     enumIndex (settings.bedTriggerMode) });
}

BenchSettings benchFields (const std::vector<std::string>& fields, std::size_t start, BenchSettings settings = {})
{
    auto next = [&] () -> std::string
    {
        if (start >= fields.size())
            return {};
        return fields[start++];
    };

    settings.type = enumFromIndex (next(), settings.type);
    settings.musicalBpm = parseDouble (next(), settings.musicalBpm);
    settings.bars = parseInt (next(), settings.bars);
    settings.key = next();
    settings.capture.captureStartBar = parseInt (next(), settings.capture.captureStartBar);
    settings.capture.warmupBars = parseInt (next(), settings.capture.warmupBars);
    settings.capture.keepBars = parseInt (next(), settings.capture.keepBars);
    settings.capture.tailBars = parseInt (next(), settings.capture.tailBars);
    settings.name = next();
    settings.flavor = enumFromIndex (next(), settings.flavor);
    settings.version = parseInt (next(), settings.version);
    settings.speedTrickEnabled = parseBool (next(), settings.speedTrickEnabled);
    settings.gainEnabled = parseBool (next(), settings.gainEnabled);
    settings.gainDecibels = parseFloat (next(), settings.gainDecibels);
    settings.normalizeEnabled = parseBool (next(), settings.normalizeEnabled);
    settings.normalizeTargetDecibels = parseFloat (next(), settings.normalizeTargetDecibels);
    settings.monoEnabled = parseBool (next(), settings.monoEnabled);
    settings.limitEnabled = parseBool (next(), settings.limitEnabled);
    settings.limitCeilingDecibels = parseFloat (next(), settings.limitCeilingDecibels);
    settings.limitInputDecibels = parseFloat (next(), settings.limitInputDecibels);
    settings.limitReleaseMs = parseFloat (next(), settings.limitReleaseMs);
    settings.compressorEnabled = parseBool (next(), settings.compressorEnabled);
    settings.compressorThresholdDecibels = parseFloat (next(), settings.compressorThresholdDecibels);
    settings.compressorRatio = parseFloat (next(), settings.compressorRatio);
    settings.compressorAttackMs = parseFloat (next(), settings.compressorAttackMs);
    settings.compressorReleaseMs = parseFloat (next(), settings.compressorReleaseMs);
    settings.compressorMakeupDecibels = parseFloat (next(), settings.compressorMakeupDecibels);
    settings.compressorMix = parseFloat (next(), settings.compressorMix);
    settings.crushEnabled = parseBool (next(), settings.crushEnabled);
    settings.crushBits = parseInt (next(), settings.crushBits);
    settings.crushSampleRate = parseDouble (next(), settings.crushSampleRate);
    settings.crushMix = parseFloat (next(), settings.crushMix);
    settings.crushOutputDecibels = parseFloat (next(), settings.crushOutputDecibels);
    settings.filterEnabled = parseBool (next(), settings.filterEnabled);
    settings.filterMode = enumFromIndex (next(), settings.filterMode);
    settings.filterCutoffHz = parseFloat (next(), settings.filterCutoffHz);
    settings.filterResonance = parseFloat (next(), settings.filterResonance);
    settings.driveEnabled = parseBool (next(), settings.driveEnabled);
    settings.driveAmount = parseFloat (next(), settings.driveAmount);
    settings.driveTone = parseFloat (next(), settings.driveTone);
    settings.driveMix = parseFloat (next(), settings.driveMix);
    settings.driveOutputDecibels = parseFloat (next(), settings.driveOutputDecibels);
    settings.eqEnabled = parseBool (next(), settings.eqEnabled);
    settings.eqLowDecibels = parseFloat (next(), settings.eqLowDecibels);
    settings.eqMidDecibels = parseFloat (next(), settings.eqMidDecibels);
    settings.eqHighDecibels = parseFloat (next(), settings.eqHighDecibels);
    settings.delayEnabled = parseBool (next(), settings.delayEnabled);
    settings.delayDivision = parseInt (next(), settings.delayDivision);
    settings.delayFeedback = parseFloat (next(), settings.delayFeedback);
    settings.delayMix = parseFloat (next(), settings.delayMix);
    settings.delayTone = parseFloat (next(), settings.delayTone);
    settings.reverbEnabled = parseBool (next(), settings.reverbEnabled);
    settings.reverbSize = parseFloat (next(), settings.reverbSize);
    settings.reverbDecaySeconds = parseFloat (next(), settings.reverbDecaySeconds);
    settings.reverbMix = parseFloat (next(), settings.reverbMix);
    settings.reverbTone = parseFloat (next(), settings.reverbTone);
    settings.tapeEnabled = parseBool (next(), settings.tapeEnabled);
    settings.tapeDrive = parseFloat (next(), settings.tapeDrive);
    settings.tapeWobble = parseFloat (next(), settings.tapeWobble);
    settings.tapeTone = parseFloat (next(), settings.tapeTone);
    settings.tapeNoise = parseFloat (next(), settings.tapeNoise);
    settings.tapeMix = parseFloat (next(), settings.tapeMix);
    settings.customEffectChain = parseBool (next(), settings.customEffectChain);
    settings.customFxModules = parseBool (next(), settings.customFxModules);
    settings.sourceBedMode = enumFromIndex (next(), settings.sourceBedMode);
    settings.bedLengthBars = parseInt (next(), settings.bedLengthBars);
    settings.bedTriggerMode = enumFromIndex (next(), settings.bedTriggerMode);
    settings.effectChain.clear();
    settings.fxModules.clear();
    return settings;
}
}

std::vector<std::filesystem::path> defaultVst3ScanPaths (PluginScanPlatform platform,
                                                         const std::filesystem::path& homeDirectory)
{
    if (platform == PluginScanPlatform::macOS)
    {
        std::vector<std::filesystem::path> paths { "/Library/Audio/Plug-Ins/VST3" };
        if (! homeDirectory.empty())
            paths.push_back (homeDirectory / "Library/Audio/Plug-Ins/VST3");
        return paths;
    }

    if (platform == PluginScanPlatform::windows)
        return { std::filesystem::path { "C:\\Program Files\\Common Files\\VST3" } };

    return {};
}

PluginScanSettings PluginScanSettings::withPlatformDefaults (PluginScanPlatform platform,
                                                             const std::filesystem::path& homeDirectory)
{
    PluginScanSettings settings;
    settings.scanPaths = defaultVst3ScanPaths (platform, homeDirectory);
    return settings;
}

bool PluginScanSettings::addPath (std::filesystem::path path)
{
    if (path.empty())
        return false;

    path = path.lexically_normal();
    const auto comparable = comparablePath (path);
    const auto duplicate = std::any_of (scanPaths.begin(), scanPaths.end(), [&] (const auto& existing)
    {
        return comparablePath (existing) == comparable;
    });
    if (duplicate)
        return false;

    scanPaths.push_back (std::move (path));
    return true;
}

bool PluginScanSettings::removePath (const std::filesystem::path& path)
{
    const auto comparable = comparablePath (path);
    const auto oldSize = scanPaths.size();
    scanPaths.erase (std::remove_if (scanPaths.begin(), scanPaths.end(), [&] (const auto& existing)
    {
        return comparablePath (existing) == comparable;
    }), scanPaths.end());
    return scanPaths.size() != oldSize;
}

void PluginScanSettings::resetToPlatformDefaults (PluginScanPlatform platform,
                                                  const std::filesystem::path& homeDirectory)
{
    scanPaths = defaultVst3ScanPaths (platform, homeDirectory);
}

PluginRegistry PluginRegistry::loadFromDisk (const std::filesystem::path& filePath)
{
    PluginRegistry registry;
    std::ifstream stream { filePath };
    if (! stream)
        return registry;

    std::string line;
    std::getline (stream, line);
    if (line != "SFB_PLUGIN_CACHE_V1")
        return registry;

    while (std::getline (stream, line))
    {
        const auto fields = splitCacheLine (line);
        if (fields.empty())
            continue;

        if (fields[0] == "CACHE_VERSION" && fields.size() >= 2)
            registry.cacheVersion = parseInt (fields[1], currentCacheVersion);
        else if (fields[0] == "APP_VERSION" && fields.size() >= 2)
            registry.scanAppVersion = fields[1];
        else if (fields[0] == "LAST_SCAN" && fields.size() >= 2)
            registry.lastScanTime = fields[1];
        else if (fields[0] == "SCAN_PATH" && fields.size() >= 2)
            registry.settings.addPath (fields[1]);
        else if (fields[0] == "CACHED_PATH" && fields.size() >= 2)
            registry.cachedScanPaths.push_back (fields[1]);
        else if (fields[0] == "FOUND" && fields.size() >= 8)
            registry.foundPlugins.push_back ({ fields[1], fields[2], fields[3], fields[4], fields[5], fields[6], parseLongLong (fields[7]) });
        else if (fields[0] == "FAILED" && fields.size() >= 7)
            registry.failedPlugins.push_back ({ fields[1], fields[2], fields[3], fields[4], fields[5], parseLongLong (fields[6]) });
        else if (fields[0] == "BLOCKED" && fields.size() >= 7)
            registry.blockedPlugins.push_back ({ fields[1], fields[2], fields[3], fields[4], fields[5], parseLongLong (fields[6]) });
    }

    registry.scanHasRunThisSession = false;
    return registry;
}

bool PluginRegistry::saveToDisk (const std::filesystem::path& filePath) const
{
    const auto parent = filePath.parent_path();
    if (! parent.empty())
        std::filesystem::create_directories (parent);

    std::ofstream stream { filePath, std::ios::trunc };
    if (! stream)
        return false;

    stream << "SFB_PLUGIN_CACHE_V1\n";
    writeCacheLine (stream, { "CACHE_VERSION", std::to_string (cacheVersion) });
    writeCacheLine (stream, { "APP_VERSION", scanAppVersion });
    writeCacheLine (stream, { "LAST_SCAN", lastScanTime });

    for (const auto& path : settings.scanPaths)
        writeCacheLine (stream, { "SCAN_PATH", path.string() });

    for (const auto& path : cachedScanPaths)
        writeCacheLine (stream, { "CACHED_PATH", path.string() });

    for (const auto& plugin : foundPlugins)
        writeCacheLine (stream, { "FOUND",
                                  plugin.name,
                                  plugin.manufacturer,
                                  plugin.format,
                                  plugin.category,
                                  plugin.filePath.string(),
                                  plugin.uniqueId,
                                  toString (plugin.modifiedTimeMillis) });

    for (const auto& plugin : failedPlugins)
        writeCacheLine (stream, { "FAILED",
                                  plugin.name,
                                  plugin.manufacturer,
                                  plugin.format,
                                  plugin.filePath.string(),
                                  plugin.reason,
                                  toString (plugin.modifiedTimeMillis) });

    for (const auto& plugin : blockedPlugins)
        writeCacheLine (stream, { "BLOCKED",
                                  plugin.name,
                                  plugin.manufacturer,
                                  plugin.format,
                                  plugin.filePath.string(),
                                  plugin.reason,
                                  toString (plugin.modifiedTimeMillis) });

    return static_cast<bool> (stream);
}

bool PluginRegistry::rescanRecommended() const
{
    return ! pathListsEqual (settings.scanPaths, cachedScanPaths);
}

void PluginRegistry::clearCachePreservingPaths()
{
    foundPlugins.clear();
    failedPlugins.clear();
    blockedPlugins.clear();
    cachedScanPaths.clear();
    lastScanTime.clear();
}

void PluginRegistry::clearFailedPlugins()
{
    failedPlugins.clear();
    blockedPlugins.clear();
}

void PluginRegistry::resetPathsToDefaults (PluginScanPlatform platform,
                                           const std::filesystem::path& homeDirectory)
{
    settings.resetToPlatformDefaults (platform, homeDirectory);
}

std::string toString (RenderFlavor flavor)
{
    switch (flavor)
    {
        case RenderFlavor::dry: return "dry";
        case RenderFlavor::wet: return "wet";
    }

    return "dry";
}

std::string buildFinalFilename (const BenchSettings& settings)
{
    const auto name = safeName (settings.name);
    const auto flavor = toString (settings.flavor);
    const auto version = std::max (1, settings.version);

    if (settings.type == SampleType::oneShot)
        return name + "_" + flavor + "_v" + std::to_string (version) + ".wav";

    auto filename = name + "_" + flavor + "_v" + std::to_string (version);
    if (settings.speedTrickEnabled)
        filename += "_spdup";

    filename += "_" + paddedBpm (settings.musicalBpm) + "BPM.wav";
    return filename;
}

std::string safeExportFolderName (std::string displayName)
{
    // Pack names can be friendly in the UI, but export folders have to survive
    // Finder, Windows Explorer, and sampler SD-card workflows.
    displayName = trimWhitespace (std::move (displayName));
    std::string safe;
    safe.reserve (displayName.size());
    bool lastWasSeparator = false;

    for (const auto character : displayName)
    {
        const auto unsignedCharacter = static_cast<unsigned char> (character);
        const auto isSafe = std::isalnum (unsignedCharacter) != 0 || character == '_' || character == '-';
        const auto isSeparator = std::isspace (unsignedCharacter) != 0
                              || character == '/'
                              || character == '\\'
                              || character == ':'
                              || character == '*'
                              || character == '?'
                              || character == '"'
                              || character == '<'
                              || character == '>'
                              || character == '|';

        if (isSafe)
        {
            safe += character;
            lastWasSeparator = false;
        }
        else if (isSeparator && ! lastWasSeparator && ! safe.empty())
        {
            safe += '_';
            lastWasSeparator = true;
        }
    }

    while (! safe.empty() && (safe.back() == '_' || safe.back() == '-'))
        safe.pop_back();

    return safe.empty() ? "UNTITLED_PACK" : safe;
}

PackExportResult exportPackToFolder (const Pack& pack,
                                     const std::filesystem::path& destinationDirectory,
                                     const ExportOptions& options,
                                     std::string exportTimestamp)
{
    PackExportResult result;
    if (destinationDirectory.empty())
    {
        result.errors.push_back ("No export destination selected.");
        return result;
    }

    try
    {
        std::filesystem::create_directories (destinationDirectory);
        const auto baseName = safeExportFolderName (std::string (pack.name()));
        auto exportFolder = destinationDirectory / baseName;

        // Do not merge into an existing export. A numbered folder is boring, but it
        // keeps old exports intact and avoids surprising overwrites.
        for (int index = 2; std::filesystem::exists (exportFolder); ++index)
        {
            std::ostringstream suffix;
            suffix << '_' << std::setw (2) << std::setfill ('0') << index;
            exportFolder = destinationDirectory / (baseName + suffix.str());
        }

        std::filesystem::create_directories (exportFolder);
        result.folderPath = exportFolder;

        std::map<BucketId, std::map<std::string, int>> bucketNameCounts;
        std::ostringstream notes;
        notes << "Bench Sampler Export\n";
        notes << "Pack: " << pack.name() << "\n";
        if (! exportTimestamp.empty())
            notes << "Exported: " << exportTimestamp << "\n";
        notes << "\n";
        notes << "This app exports WAV files and pack folders only.\n";
        notes << "No sampler upload or device control.\n\n";

        for (const auto& bucket : pack.buckets())
        {
            const auto bucketFolder = exportFolder / bucket.folderName;
            std::filesystem::create_directories (bucketFolder);
            notes << "[" << bucket.folderName << "]\n\n";

            for (const auto& sample : pack.samplesInBucket (bucket.id))
            {
                if (options.includeKeptBounces)
                {
                    // A render preview is just audition material until the user keeps it.
                    // Export only variations, which are the explicit "this belongs in the pack" list.
                    for (const auto& variation : sample.variations)
                    {
                        if (variation.filePath.empty() || ! std::filesystem::is_regular_file (variation.filePath))
                        {
                            ++result.skippedFiles;
                            result.errors.push_back ("Skipped missing bounce: " + variation.finalFilename);
                            continue;
                        }

                        const auto requestedName = variation.finalFilename.empty()
                                                 ? variation.filePath.filename().string()
                                                 : variation.finalFilename;
                        const auto duplicateIndex = ++bucketNameCounts[bucket.id][requestedName];
                        const auto exportName = duplicateFilenameForIndex (requestedName, duplicateIndex);
                        const auto destination = bucketFolder / exportName;

                        try
                        {
                            std::filesystem::copy_file (variation.filePath,
                                                        destination,
                                                        std::filesystem::copy_options::overwrite_existing);
                            ++result.exportedFiles;

                            const auto& settings = variation.settings;
                            notes << exportName << "\n";
                            notes << "- Source: " << (variation.sourcePath.empty() ? sample.sourcePath.string()
                                                                                   : variation.sourcePath.string()) << "\n";
                            notes << "- Type: " << sampleTypeText (settings.type) << "\n";
                            if (settings.musicalBpm > 0.0)
                                notes << "- Musical BPM: " << settings.musicalBpm << "\n";
                            if (settings.bars > 0)
                                notes << "- Bars: " << settings.bars << "\n";
                            notes << "- Bucket: " << bucket.folderName << "\n";
                            notes << "- Capture: Start " << settings.capture.captureStartBar
                                  << ", Warm-up " << barsText (settings.capture.warmupBars)
                                  << ", Keep " << settings.capture.keepBars
                                  << ", Tail " << barsText (settings.capture.tailBars) << "\n";
                            notes << "- Source Bed: " << sourceBedText (settings.sourceBedMode) << "\n";
                            notes << "- Bed Length: " << settings.bedLengthBars << "\n";
                            notes << "- Trigger: " << triggerText (settings.bedTriggerMode) << "\n";
                            notes << "- FX Chain: " << effectChainSummary (settings) << "\n";
                            notes << "- Speed Trick: " << (settings.speedTrickEnabled ? "On" : "Off") << "\n";
                            notes << "- Render file bytes: " << std::filesystem::file_size (destination) << "\n\n";
                        }
                        catch (const std::exception& exception)
                        {
                            ++result.skippedFiles;
                            result.errors.push_back ("Could not export " + requestedName + ": " + exception.what());
                        }
                    }
                }

                if (options.includeOriginalSources)
                {
                    if (! std::filesystem::is_regular_file (sample.sourcePath))
                    {
                        ++result.skippedFiles;
                        result.errors.push_back ("Skipped missing source: " + sample.sourcePath.string());
                        continue;
                    }

                    const auto requestedName = sample.sourcePath.filename().string();
                    const auto duplicateIndex = ++bucketNameCounts[bucket.id][requestedName];
                    const auto exportName = duplicateFilenameForIndex (requestedName, duplicateIndex);
                    try
                    {
                        std::filesystem::copy_file (sample.sourcePath,
                                                    bucketFolder / exportName,
                                                    std::filesystem::copy_options::overwrite_existing);
                        ++result.exportedFiles;
                    }
                    catch (const std::exception& exception)
                    {
                        ++result.skippedFiles;
                        result.errors.push_back ("Could not export source " + requestedName + ": " + exception.what());
                    }
                }
            }
        }

        if (options.includeNotesFile)
        {
            std::ofstream notesFile { exportFolder / (baseName + "_notes.txt"), std::ios::trunc };
            notesFile << notes.str();
            if (! notesFile)
            {
                result.errors.push_back ("Could not write notes file.");
                return result;
            }
        }

        result.success = true;
    }
    catch (const std::exception& exception)
    {
        result.errors.push_back (exception.what());
    }

    return result;
}

CaptureTiming calculateCaptureTiming (double musicalBpm, const CaptureSettings& settings)
{
    const auto bpm = musicalBpm > 0.0 ? musicalBpm : 120.0;
    const auto startBar = std::max (1, settings.captureStartBar);
    const auto warmupBars = std::max (0, settings.warmupBars);
    const auto keepBars = std::max (1, settings.keepBars);
    const auto tailBars = std::max (0, settings.tailBars);

    CaptureTiming timing;
    timing.secondsPerBar = 60.0 / bpm * 4.0;
    timing.keepStartSeconds = static_cast<double> (startBar - 1) * timing.secondsPerBar;
    timing.exportOffsetInInternalSeconds = static_cast<double> (warmupBars) * timing.secondsPerBar;
    timing.internalStartSeconds = std::max (0.0, timing.keepStartSeconds - timing.exportOffsetInInternalSeconds);
    timing.exportedDurationSeconds = static_cast<double> (keepBars + tailBars) * timing.secondsPerBar;
    timing.internalDurationSeconds = timing.exportOffsetInInternalSeconds + timing.exportedDurationSeconds;
    return timing;
}

CaptureRegions calculateCaptureRegions (double musicalBpm, const CaptureSettings& settings)
{
    const auto bpm = musicalBpm > 0.0 ? musicalBpm : 120.0;
    const auto startBar = std::max (1, settings.captureStartBar);
    const auto warmupBars = std::max (0, settings.warmupBars);
    const auto keepBars = std::max (1, settings.keepBars);
    const auto tailBars = std::max (0, settings.tailBars);
    const auto secondsPerBar = 60.0 / bpm * 4.0;

    const auto keepStart = static_cast<double> (startBar - 1) * secondsPerBar;
    const auto keepEnd = keepStart + static_cast<double> (keepBars) * secondsPerBar;
    const auto tailEnd = keepEnd + static_cast<double> (tailBars) * secondsPerBar;
    const auto warmupStart = std::max (0.0, keepStart - static_cast<double> (warmupBars) * secondsPerBar);

    CaptureRegions regions;
    regions.secondsPerBar = secondsPerBar;
    regions.startBar = startBar;
    regions.warmupBars = warmupBars;
    regions.keepBars = keepBars;
    regions.tailBars = tailBars;
    regions.warmup = warmupBars > 0 ? VisibleTimeRange { warmupStart, keepStart }
                                    : VisibleTimeRange { keepStart, keepStart };
    regions.keep = { keepStart, keepEnd };
    regions.tail = tailBars > 0 ? VisibleTimeRange { keepEnd, tailEnd }
                                : VisibleTimeRange { keepEnd, keepEnd };
    regions.bounce = { keepStart, tailEnd };
    regions.render = { warmupStart, tailEnd };
    return regions;
}

VisibleTimeRange sourcePreviewLoopRegion (const CaptureRegions& regions)
{
    return regions.keep;
}

VisibleTimeRange bouncePreviewLoopRegion (double bounceDurationSeconds)
{
    return { 0.0, std::max (0.0, bounceDurationSeconds) };
}

double calculateBedDurationSeconds (double musicalBpm, int bedLengthBars)
{
    const auto bpm = musicalBpm > 0.0 ? musicalBpm : 120.0;
    return static_cast<double> (std::max (1, bedLengthBars)) * (60.0 / bpm * 4.0);
}

FxModule makeBuiltInFxModule (BuiltInEffectId effect)
{
    FxModule module;
    module.kind = FxModuleKind::builtIn;
    module.builtIn = effect;
    return module;
}

FxModule makePluginFxModule (const CachedPluginDescription& plugin)
{
    FxModule module;
    module.kind = FxModuleKind::plugin;
    module.plugin.name = plugin.name;
    module.plugin.manufacturer = plugin.manufacturer;
    module.plugin.format = plugin.format.empty() ? "VST3" : plugin.format;
    module.plugin.category = plugin.category;
    module.plugin.filePath = plugin.filePath;
    module.plugin.uniqueId = plugin.uniqueId;
    module.plugin.modifiedTimeMillis = plugin.modifiedTimeMillis;
    module.plugin.enabled = true;
    module.plugin.status = PluginModuleStatus::loaded;
    return module;
}

std::vector<BuiltInEffectId> fixedEffectChainOrder()
{
    return { BuiltInEffectId::gain,
             BuiltInEffectId::mono,
             BuiltInEffectId::normalize,
             BuiltInEffectId::limit,
             BuiltInEffectId::compressor,
             BuiltInEffectId::crush,
             BuiltInEffectId::filter,
             BuiltInEffectId::drive,
             BuiltInEffectId::eq,
             BuiltInEffectId::delay,
             BuiltInEffectId::reverb,
             BuiltInEffectId::tape };
}

std::vector<FxModule> defaultFxModules()
{
    std::vector<FxModule> modules;
    for (const auto effect : fixedEffectChainOrder())
        modules.push_back (makeBuiltInFxModule (effect));
    return modules;
}

std::vector<BuiltInEffectId> activeEffectChain (const BenchSettings& settings)
{
    if (settings.customFxModules)
    {
        std::vector<BuiltInEffectId> effects;
        for (const auto& module : settings.fxModules)
            if (module.kind == FxModuleKind::builtIn)
                effects.push_back (module.builtIn);
        return effects;
    }

    if (settings.customEffectChain)
        return settings.effectChain;

    return fixedEffectChainOrder();
}

std::vector<FxModule> activeFxModules (const BenchSettings& settings)
{
    if (settings.customFxModules)
        return settings.fxModules;

    if (settings.customEffectChain)
    {
        std::vector<FxModule> modules;
        for (const auto effect : settings.effectChain)
            modules.push_back (makeBuiltInFxModule (effect));
        return modules;
    }

    return defaultFxModules();
}

std::vector<FxModule> resolvePluginModuleAvailability (const std::vector<FxModule>& modules,
                                                       const std::vector<CachedPluginDescription>& availablePlugins)
{
    auto resolved = modules;
    for (auto& module : resolved)
    {
        if (module.kind != FxModuleKind::plugin)
            continue;

        const auto found = std::any_of (availablePlugins.begin(), availablePlugins.end(), [&] (const auto& plugin)
        {
            const auto uniqueMatches = ! module.plugin.uniqueId.empty() && module.plugin.uniqueId == plugin.uniqueId;
            const auto pathMatches = ! module.plugin.filePath.empty() && module.plugin.filePath == plugin.filePath;
            return uniqueMatches || pathMatches;
        });

        if (found)
        {
            if (module.plugin.status == PluginModuleStatus::missing)
                module.plugin.status = PluginModuleStatus::loaded;
        }
        else
        {
            module.plugin.status = PluginModuleStatus::missing;
            module.plugin.enabled = false;
        }
    }
    return resolved;
}

std::vector<HostedPluginModule> activePluginModules (const BenchSettings& settings)
{
    std::vector<HostedPluginModule> plugins;
    for (const auto& module : activeFxModules (settings))
    {
        if (module.kind == FxModuleKind::plugin
            && module.plugin.enabled
            && module.plugin.status == PluginModuleStatus::loaded)
        {
            plugins.push_back (module.plugin);
        }
    }
    return plugins;
}

std::vector<std::string> fxMenuTopLevelOrder()
{
    return { "Plugins", "UTILITY", "DYNAMICS", "COLOR", "SPACE" };
}

AudioBufferData applyEffectChain (AudioBufferData audio,
                                  const BenchSettings& settings,
                                  EffectProcessMode mode)
{
    if (audio.channels.empty())
        return audio;

    const auto sampleCount = audio.channels.front().size();
    if (sampleCount == 0)
        return audio;

    for (auto& channel : audio.channels)
        channel.resize (sampleCount, 0.0f);

    const auto applyGain = [&audio, &settings]
    {
        const auto gain = dbToGain (settings.gainDecibels);
        for (auto& channel : audio.channels)
            for (auto& sample : channel)
                sample = clampSample (sample * gain);
    };

    const auto applyMono = [&audio, sampleCount]
    {
        for (std::size_t index = 0; index < sampleCount; ++index)
        {
            float sum = 0.0f;
            for (const auto& channel : audio.channels)
                sum += channel[index];

            const auto mono = sum / static_cast<float> (audio.channels.size());
            for (auto& channel : audio.channels)
                channel[index] = mono;
        }
    };

    const auto applyNormalize = [&audio, &settings]
    {
        float peak = 0.0f;
        for (const auto& channel : audio.channels)
            for (auto sample : channel)
                peak = std::max (peak, std::abs (sample));

        if (peak > 0.000001f)
        {
            const auto target = dbToGain (settings.normalizeTargetDecibels);
            const auto gain = target / peak;
            for (auto& channel : audio.channels)
                for (auto& sample : channel)
                    sample = clampSample (sample * gain);
        }
    };

    const auto applyLimit = [&audio, &settings]
    {
        const auto ceiling = dbToGain (std::clamp (settings.limitCeilingDecibels, -12.0f, 0.0f));
        const auto inputGain = dbToGain (std::clamp (settings.limitInputDecibels, 0.0f, 24.0f));
        for (auto& channel : audio.channels)
            for (auto& sample : channel)
                sample = std::clamp (sample * inputGain, -ceiling, ceiling);
    };

    const auto applyCompressor = [&audio, &settings]
    {
        const auto threshold = dbToGain (std::clamp (settings.compressorThresholdDecibels, -48.0f, 0.0f));
        const auto ratio = std::clamp (settings.compressorRatio, 1.0f, 20.0f);
        const auto makeup = dbToGain (std::clamp (settings.compressorMakeupDecibels, 0.0f, 24.0f));
        const auto mix = std::clamp (settings.compressorMix, 0.0f, 1.0f);

        for (auto& channel : audio.channels)
        {
            for (auto& sample : channel)
            {
                const auto dry = sample;
                const auto sign = sample < 0.0f ? -1.0f : 1.0f;
                const auto absolute = std::abs (sample);
                auto compressed = absolute;
                if (absolute > threshold)
                    compressed = threshold + (absolute - threshold) / ratio;

                const auto wet = clampSample (sign * compressed * makeup);
                sample = clampSample (dry * (1.0f - mix) + wet * mix);
            }
        }
    };

    const auto applyCrush = [&audio, &settings, sampleCount]
    {
        const auto bits = std::clamp (settings.crushBits, 1, 24);
        const auto colorBits = std::clamp (bits - 4, 3, 16);
        const auto levels = static_cast<float> ((1 << std::min (colorBits, 16)) - 1);
        auto holdSamples = std::size_t { 1 };
        if (settings.crushSampleRate > 0.0 && audio.sampleRate > settings.crushSampleRate)
        {
            const auto baseHold = static_cast<std::size_t> (std::round (audio.sampleRate / settings.crushSampleRate));
            holdSamples = settings.crushSampleRate >= 40000.0 ? std::size_t { 1 }
                                                              : std::max<std::size_t> (1, baseHold * 4);
        }
        const auto mix = std::clamp (settings.crushMix, 0.0f, 1.0f);
        const auto output = dbToGain (std::clamp (settings.crushOutputDecibels, -12.0f, 12.0f));

        for (auto& channel : audio.channels)
        {
            auto held = 0.0f;
            for (std::size_t index = 0; index < sampleCount; ++index)
            {
                if (index % holdSamples == 0)
                    held = std::round (std::clamp (channel[index], -1.0f, 1.0f) * levels) / levels;

                channel[index] = clampSample ((channel[index] * (1.0f - mix) + held * mix) * output);
            }
        }
    };

    const auto applyFilter = [&audio, &settings]
    {
        const auto sampleRate = audio.sampleRate > 0.0 ? audio.sampleRate : 44100.0;
        const auto nyquist = sampleRate * 0.5;
        const auto cutoff = std::clamp (static_cast<double> (settings.filterCutoffHz), 20.0, nyquist - 100.0);
        const auto coefficient = onePoleCoefficient (cutoff, sampleRate);

        for (auto& channel : audio.channels)
        {
            auto low = 0.0f;
            for (auto& sample : channel)
            {
                low = (1.0f - coefficient) * sample + coefficient * low;
                sample = settings.filterMode == FilterMode::highPass ? sample - low : low;
            }
        }
    };

    const auto applyDrive = [&audio, &settings]
    {
        const auto amount = std::clamp (settings.driveAmount, 0.0f, 1.0f);
        const auto tone = std::clamp (settings.driveTone, 0.0f, 1.0f);
        const auto mix = std::clamp (settings.driveMix, 0.0f, 1.0f);
        const auto output = dbToGain (std::clamp (settings.driveOutputDecibels, -12.0f, 12.0f));
        const auto drive = 1.0f + amount * 24.0f;
        const auto lowCoefficient = onePoleCoefficient (800.0 + tone * 6000.0, audio.sampleRate > 0.0 ? audio.sampleRate : 44100.0);

        for (auto& channel : audio.channels)
        {
            auto low = 0.0f;
            for (auto& sample : channel)
            {
                const auto dry = sample;
                auto wet = std::tanh (sample * drive) / std::tanh (drive);
                low = (1.0f - lowCoefficient) * wet + lowCoefficient * low;
                wet = tone < 0.5f ? low * (1.0f - tone * 2.0f) + wet * tone * 2.0f
                                  : wet + (wet - low) * (tone - 0.5f);
                sample = clampSample ((dry * (1.0f - mix) + wet * mix) * output);
            }
        }
    };

    const auto applyEq = [&audio, &settings]
    {
        const auto sampleRate = audio.sampleRate > 0.0 ? audio.sampleRate : 44100.0;
        const auto lowGain = dbToGain (std::clamp (settings.eqLowDecibels, -12.0f, 12.0f));
        const auto midGain = dbToGain (std::clamp (settings.eqMidDecibels, -12.0f, 12.0f));
        const auto highGain = dbToGain (std::clamp (settings.eqHighDecibels, -12.0f, 12.0f));
        const auto lowCoefficient = onePoleCoefficient (250.0, sampleRate);
        const auto highCoefficient = onePoleCoefficient (4000.0, sampleRate);

        for (auto& channel : audio.channels)
        {
            auto low = 0.0f;
            auto highLow = 0.0f;
            for (auto& sample : channel)
            {
                low = (1.0f - lowCoefficient) * sample + lowCoefficient * low;
                highLow = (1.0f - highCoefficient) * sample + highCoefficient * highLow;
                const auto high = sample - highLow;
                const auto mid = sample - low - high;
                sample = clampSample (low * lowGain + mid * midGain + high * highGain);
            }
        }
    };

    const auto delaySecondsForDivision = [&settings]
    {
        if (settings.musicalBpm <= 0.0)
            return 0.0;

        const auto beat = 60.0 / settings.musicalBpm;
        switch (std::clamp (settings.delayDivision, 0, 4))
        {
            case 0: return beat * 0.25;
            case 1: return beat * 0.5;
            case 2: return beat;
            case 3: return beat * 2.0;
            default: return beat * 4.0;
        }
    };

    const auto applyDelay = [&audio, &settings, &delaySecondsForDivision, sampleCount]
    {
        const auto sampleRate = audio.sampleRate > 0.0 ? audio.sampleRate : 44100.0;
        const auto delaySeconds = delaySecondsForDivision();
        if (delaySeconds <= 0.0)
            return;

        const auto delaySamples = std::min<std::size_t> (sampleCount > 1 ? sampleCount - 1 : 1,
                                                         samplesFromSeconds (delaySeconds, sampleRate));
        const auto feedback = std::clamp (settings.delayFeedback, 0.0f, 0.95f);
        const auto mix = std::clamp (settings.delayMix, 0.0f, 1.0f);
        const auto tone = std::clamp (settings.delayTone, 0.0f, 1.0f);
        const auto damping = 0.25f + tone * 0.7f;

        for (auto& channel : audio.channels)
        {
            auto delayed = std::vector<float> (sampleCount, 0.0f);
            for (std::size_t index = delaySamples; index < sampleCount; ++index)
            {
                const auto repeat = channel[index - delaySamples] + delayed[index - delaySamples] * feedback;
                delayed[index] = repeat * damping;
                channel[index] = clampSample (channel[index] * (1.0f - mix) + delayed[index] * mix);
            }
        }
    };

    const auto applyReverb = [&audio, &settings, sampleCount]
    {
        const auto sampleRate = audio.sampleRate > 0.0 ? audio.sampleRate : 44100.0;
        const auto size = std::clamp (settings.reverbSize, 0.0f, 1.0f);
        const auto decay = std::clamp (settings.reverbDecaySeconds, 0.1f, 8.0f);
        const auto mix = std::clamp (settings.reverbMix, 0.0f, 1.0f);
        const auto tone = std::clamp (settings.reverbTone, 0.0f, 1.0f);
        const std::array<double, 3> baseDelays { 0.05, 0.07, 0.11 };
        const auto feedback = std::clamp (0.65f + decay * 0.04f, 0.2f, 0.92f);
        const auto damping = 0.75f + tone * 0.2f;

        for (auto& channel : audio.channels)
        {
            auto wet = std::vector<float> (sampleCount, 0.0f);
            for (const auto baseDelay : baseDelays)
            {
                const auto delaySamples = std::min<std::size_t> (sampleCount > 1 ? sampleCount - 1 : 1,
                                                                 samplesFromSeconds (baseDelay * (0.75 + size), sampleRate));
                for (std::size_t index = delaySamples; index < sampleCount; ++index)
                    wet[index] += (channel[index - delaySamples] + wet[index - delaySamples] * feedback) * damping * 0.7f;
            }

            for (std::size_t index = 0; index < sampleCount; ++index)
                channel[index] = clampSample (channel[index] * (1.0f - mix) + wet[index] * mix);
        }
    };

    const auto applyTape = [&audio, &settings, sampleCount]
    {
        const auto sampleRate = audio.sampleRate > 0.0 ? audio.sampleRate : 44100.0;
        const auto drive = std::clamp (settings.tapeDrive, 0.0f, 1.0f);
        const auto wobble = std::clamp (settings.tapeWobble, 0.0f, 1.0f);
        const auto tone = std::clamp (settings.tapeTone, 0.0f, 1.0f);
        const auto noise = std::clamp (settings.tapeNoise, 0.0f, 1.0f);
        const auto mix = std::clamp (settings.tapeMix, 0.0f, 1.0f);
        const auto lowCoefficient = onePoleCoefficient (1200.0 + tone * 7000.0, sampleRate);

        for (auto& channel : audio.channels)
        {
            auto low = 0.0f;
            for (std::size_t index = 0; index < sampleCount; ++index)
            {
                const auto dry = channel[index];
                const auto phase = static_cast<float> (2.0 * 3.14159265358979323846 * static_cast<double> (index) / std::max (1.0, sampleRate * 0.8));
                const auto wobbleGain = 1.0f + std::sin (phase) * wobble * 0.035f;
                const auto pseudoNoise = std::sin (static_cast<float> (index * 12.9898 + 78.233)) * noise * 0.015f;
                auto wet = std::tanh ((dry * wobbleGain + pseudoNoise) * (1.0f + drive * 5.0f));
                low = (1.0f - lowCoefficient) * wet + lowCoefficient * low;
                wet = low * (1.0f - tone * 0.6f) + wet * (tone * 0.6f);
                channel[index] = clampSample (dry * (1.0f - mix) + wet * mix);
            }
        }
    };

    for (const auto effect : activeEffectChain (settings))
    {
        if (effect == BuiltInEffectId::gain && settings.gainEnabled && settings.gainDecibels != 0.0f)
            applyGain();
        else if (effect == BuiltInEffectId::mono && settings.monoEnabled && audio.channels.size() > 1)
            applyMono();
        else if (effect == BuiltInEffectId::normalize && mode == EffectProcessMode::bounce && settings.normalizeEnabled)
            applyNormalize();
        else if (effect == BuiltInEffectId::limit && settings.limitEnabled)
            applyLimit();
        else if (effect == BuiltInEffectId::compressor && settings.compressorEnabled)
            applyCompressor();
        else if (effect == BuiltInEffectId::crush && settings.crushEnabled)
            applyCrush();
        else if (effect == BuiltInEffectId::filter && settings.filterEnabled)
            applyFilter();
        else if (effect == BuiltInEffectId::drive && settings.driveEnabled)
            applyDrive();
        else if (effect == BuiltInEffectId::eq && settings.eqEnabled)
            applyEq();
        else if (effect == BuiltInEffectId::delay && settings.delayEnabled)
            applyDelay();
        else if (effect == BuiltInEffectId::reverb && settings.reverbEnabled)
            applyReverb();
        else if (effect == BuiltInEffectId::tape && settings.tapeEnabled)
            applyTape();
    }

    return audio;
}

bool effectChainAffectsPreview (const BenchSettings& settings)
{
    if (! activePluginModules (settings).empty())
        return true;

    for (const auto effect : activeEffectChain (settings))
    {
        if (effect == BuiltInEffectId::gain && settings.gainEnabled && settings.gainDecibels != 0.0f)
            return true;
        if (effect == BuiltInEffectId::mono && settings.monoEnabled)
            return true;
        if (effect == BuiltInEffectId::limit && settings.limitEnabled)
            return true;
        if (effect == BuiltInEffectId::compressor && settings.compressorEnabled)
            return true;
        if (effect == BuiltInEffectId::crush && settings.crushEnabled)
            return true;
        if (effect == BuiltInEffectId::filter && settings.filterEnabled)
            return true;
        if (effect == BuiltInEffectId::drive && settings.driveEnabled)
            return true;
        if (effect == BuiltInEffectId::eq && settings.eqEnabled)
            return true;
        if (effect == BuiltInEffectId::delay && settings.delayEnabled)
            return true;
        if (effect == BuiltInEffectId::reverb && settings.reverbEnabled)
            return true;
        if (effect == BuiltInEffectId::tape && settings.tapeEnabled)
            return true;
    }

    return false;
}

std::string effectChainSummary (const BenchSettings& settings)
{
    std::vector<std::string> parts;
    for (const auto& module : activeFxModules (settings))
    {
        if (module.kind == FxModuleKind::plugin)
        {
            if (module.plugin.enabled && module.plugin.status == PluginModuleStatus::loaded)
                parts.push_back (module.plugin.name.empty() ? "VST3" : module.plugin.name);
            else if (module.plugin.status == PluginModuleStatus::missing)
                parts.push_back ((module.plugin.name.empty() ? "VST3" : module.plugin.name) + " missing");
            continue;
        }

        const auto effect = module.builtIn;
        if (effect == BuiltInEffectId::gain && settings.gainEnabled && settings.gainDecibels != 0.0f)
        {
            std::ostringstream stream;
            stream << "Gain " << std::showpos << std::fixed << std::setprecision (1) << settings.gainDecibels << " dB";
            parts.push_back (stream.str());
        }
        else if (effect == BuiltInEffectId::mono && settings.monoEnabled)
        {
            parts.push_back ("Mono ON");
        }
        else if (effect == BuiltInEffectId::normalize && settings.normalizeEnabled)
        {
            std::ostringstream stream;
            stream << "Normalize " << std::fixed << std::setprecision (1) << settings.normalizeTargetDecibels << " dBFS";
            parts.push_back (stream.str());
        }
        else if (effect == BuiltInEffectId::limit && settings.limitEnabled)
        {
            std::ostringstream stream;
            stream << "Limit " << std::fixed << std::setprecision (1) << settings.limitCeilingDecibels << " dB";
            parts.push_back (stream.str());
        }
        else if (effect == BuiltInEffectId::compressor && settings.compressorEnabled)
        {
            std::ostringstream stream;
            stream << "Comp " << std::fixed << std::setprecision (1) << settings.compressorRatio
                   << ":1 / " << settings.compressorThresholdDecibels << " dB";
            parts.push_back (stream.str());
        }
        else if (effect == BuiltInEffectId::crush && settings.crushEnabled)
        {
            std::ostringstream stream;
            stream << "Crush " << settings.crushBits << "-bit";
            if (settings.crushSampleRate > 0.0)
                stream << " / " << static_cast<int> (std::round (settings.crushSampleRate / 1000.0)) << "k";
            parts.push_back (stream.str());
        }
        else if (effect == BuiltInEffectId::filter && settings.filterEnabled)
        {
            std::ostringstream stream;
            stream << "Filter " << (settings.filterMode == FilterMode::highPass ? "HP " : "LP ")
                   << std::fixed << std::setprecision (1) << (settings.filterCutoffHz / 1000.0f) << "k";
            parts.push_back (stream.str());
        }
        else if (effect == BuiltInEffectId::drive && settings.driveEnabled)
        {
            std::ostringstream stream;
            stream << "Drive " << static_cast<int> (std::round (settings.driveAmount * 100.0f)) << "%";
            parts.push_back (stream.str());
        }
        else if (effect == BuiltInEffectId::eq && settings.eqEnabled)
        {
            std::ostringstream stream;
            stream << "EQ L" << std::showpos << std::fixed << std::setprecision (0) << settings.eqLowDecibels
                   << " M" << settings.eqMidDecibels
                   << " H" << settings.eqHighDecibels;
            parts.push_back (stream.str());
        }
        else if (effect == BuiltInEffectId::delay && settings.delayEnabled)
        {
            std::ostringstream stream;
            stream << "Delay " << static_cast<int> (std::round (settings.delayFeedback * 100.0f)) << "%";
            parts.push_back (stream.str());
        }
        else if (effect == BuiltInEffectId::reverb && settings.reverbEnabled)
        {
            std::ostringstream stream;
            stream << "Reverb " << std::fixed << std::setprecision (1) << settings.reverbDecaySeconds
                   << "s " << static_cast<int> (std::round (settings.reverbMix * 100.0f)) << "%";
            parts.push_back (stream.str());
        }
        else if (effect == BuiltInEffectId::tape && settings.tapeEnabled)
        {
            std::ostringstream stream;
            stream << "Tape Drive " << static_cast<int> (std::round (settings.tapeDrive * 100.0f))
                   << " / Wob " << static_cast<int> (std::round (settings.tapeWobble * 100.0f));
            parts.push_back (stream.str());
        }
    }

    if (parts.empty())
        return "FX: none";

    std::ostringstream stream;
    stream << "FX: ";
    for (std::size_t index = 0; index < parts.size(); ++index)
    {
        if (index > 0)
            stream << "; ";
        stream << parts[index];
    }
    return stream.str();
}

std::vector<float> buildMonoAudioBed (const std::vector<float>& source,
                                      const AudioBedRequest& request)
{
    const auto sampleRate = request.sampleRate > 0.0 ? request.sampleRate : 44100.0;
    const auto durationSeconds = calculateBedDurationSeconds (request.musicalBpm, request.bedLengthBars);
    const auto totalSamples = static_cast<std::size_t> (std::max (0.0, std::round (durationSeconds * sampleRate)));
    std::vector<float> bed (totalSamples, 0.0f);

    if (source.empty() || bed.empty())
        return bed;

    if (request.triggerMode == BedTriggerMode::loopContinuously)
    {
        for (std::size_t index = 0; index < bed.size(); ++index)
            bed[index] = source[index % source.size()];

        return bed;
    }

    const auto secondsPerBar = calculateBedDurationSeconds (request.musicalBpm, 1);
    const auto samplesPerBar = static_cast<std::size_t> (std::max (1.0, std::round (secondsPerBar * sampleRate)));
    for (std::size_t barStart = 0; barStart < bed.size(); barStart += samplesPerBar)
    {
        for (std::size_t sourceIndex = 0; sourceIndex < source.size() && barStart + sourceIndex < bed.size(); ++sourceIndex)
        {
            auto mixed = bed[barStart + sourceIndex] + source[sourceIndex];
            mixed = std::clamp (mixed, -1.0f, 1.0f);
            bed[barStart + sourceIndex] = mixed;
        }
    }

    return bed;
}

std::optional<int> extractBpmFromFilename (const std::filesystem::path& sourcePath)
{
    const auto stem = lowercase (shortNameFromPath (sourcePath));
    const std::regex bpmPattern { R"((\d{2,3})\s*bpm\b)" };
    std::smatch match;

    if (std::regex_search (stem, match, bpmPattern))
        return std::stoi (match[1].str());

    return std::nullopt;
}

SampleType detectSampleTypeFromFilename (const std::filesystem::path& sourcePath)
{
    const auto name = lowercase (shortNameFromPath (sourcePath));

    if (name.find ("loop") != std::string::npos || extractBpmFromFilename (sourcePath).has_value())
        return SampleType::loop;

    for (const auto* token : { "kick", "snare", "hat", "clap", "one shot", "oneshot" })
    {
        if (name.find (token) != std::string::npos)
            return SampleType::oneShot;
    }

    return SampleType::oneShot;
}

std::string suggestExportNameFromFilename (const std::filesystem::path& sourcePath)
{
    auto name = lowercase (shortNameFromPath (sourcePath));
    name = std::regex_replace (name, std::regex { R"(\b\d{2,3}\s*bpm\b)" }, " ");
    name = std::regex_replace (name, std::regex { R"([^a-z0-9]+)" }, " ");

    std::istringstream stream { name };
    std::vector<std::string> kept;

    for (std::string token; stream >> token;)
    {
        const auto shouldDrop = token == "cymatics"
                             || token == "sample"
                             || token == "pack"
                             || token == "drum"
                             || token == "loop"
                             || token == "loops"
                             || token == "one"
                             || token == "shot"
                             || token == "wav";

        if (! shouldDrop)
            kept.push_back (token);
    }

    if (kept.size() > 1)
    {
        const auto& trailingToken = kept.back();
        const auto isTrailingIndex = trailingToken.size() <= 2
                                  && std::all_of (trailingToken.begin(), trailingToken.end(), [] (unsigned char character)
                                     {
                                         return std::isdigit (character);
                                     });
        if (isTrailingIndex)
            kept.pop_back();
    }

    if (kept.empty())
        return "sample";

    auto result = kept.front();
    for (std::size_t index = 1; index < kept.size() && result.size() < 16; ++index)
        result += "_" + kept[index];

    return trimSeparators (result);
}

std::string middleTruncatePreservingEnding (const std::string& text, std::size_t maxCharacters)
{
    if (text.size() <= maxCharacters || maxCharacters < 5)
        return text.substr (0, maxCharacters);

    const auto extensionStart = text.rfind ('.');
    const auto bpmStart = text.rfind ("BPM");
    std::size_t suffixStart = text.size() > maxCharacters / 2 ? text.size() - (maxCharacters / 2) : 0;

    if (bpmStart != std::string::npos)
    {
        const auto dryStart = text.rfind ("_dry_");
        const auto wetStart = text.rfind ("_wet_");
        if (dryStart != std::string::npos || wetStart != std::string::npos)
            suffixStart = std::max (dryStart == std::string::npos ? 0 : dryStart,
                                    wetStart == std::string::npos ? 0 : wetStart);
        else if (const auto precedingUnderscore = text.rfind ('_', bpmStart);
                 precedingUnderscore != std::string::npos)
            suffixStart = precedingUnderscore;
    }
    else if (extensionStart != std::string::npos)
    {
        suffixStart = extensionStart > maxCharacters / 2 ? extensionStart - (maxCharacters / 2) : 0;
    }

    const auto suffix = text.substr (suffixStart);
    const auto ellipsis = std::string { "..." };
    if (suffix.size() + ellipsis.size() >= maxCharacters)
        return ellipsis + text.substr (text.size() - (maxCharacters - ellipsis.size()));

    const auto prefixLength = maxCharacters - suffix.size() - ellipsis.size();
    return text.substr (0, prefixLength) + ellipsis + suffix;
}

PlaybackState startPlayback (PlaybackState state, PlaybackTarget target)
{
    state.target = target;
    state.isPlaying = target != PlaybackTarget::none;
    if (target != PlaybackTarget::none)
        state.previewTarget = target;
    return state;
}

PlaybackState startSelectedPlayback (PlaybackState state)
{
    return startPlayback (state, state.previewTarget);
}

PlaybackState stopPlayback (PlaybackState state)
{
    state.target = PlaybackTarget::none;
    state.isPlaying = false;
    return state;
}

PlaybackState setPreviewTarget (PlaybackState state, PlaybackTarget target)
{
    if (target != PlaybackTarget::none)
        state.previewTarget = target;

    return state;
}

PlaybackState setLoopEnabled (PlaybackState state, bool enabled)
{
    state.loopEnabled = enabled;
    return state;
}

double wrappedLoopPosition (double positionSeconds, VisibleTimeRange loop)
{
    if (loop.endSeconds <= loop.startSeconds)
        return std::max (0.0, positionSeconds);

    if (positionSeconds < loop.startSeconds)
        return loop.startSeconds;

    if (positionSeconds < loop.endSeconds)
        return positionSeconds;

    const auto duration = loop.endSeconds - loop.startSeconds;
    auto offset = std::fmod (positionSeconds - loop.startSeconds, duration);
    if (offset < 0.0)
        offset += duration;

    return loop.startSeconds + offset;
}

VisibleTimeRange makeInitialVisibleWindow (double durationSeconds,
                                           double focusStartSeconds,
                                           double focusDurationSeconds)
{
    const auto duration = std::max (0.0, durationSeconds);
    if (duration <= 12.0)
        return { 0.0, duration };

    const auto focusDuration = std::max (1.0, focusDurationSeconds);
    const auto windowDuration = std::min (duration, std::max (12.0, focusDuration * 1.75));
    const auto focusCenter = std::max (0.0, focusStartSeconds) + focusDuration * 0.5;
    auto start = focusCenter - windowDuration * 0.35;
    start = std::clamp (start, 0.0, std::max (0.0, duration - windowDuration));

    return { start, start + windowDuration };
}

VisibleTimeRange autoFollowVisibleWindow (VisibleTimeRange visibleRange,
                                          double playheadSeconds,
                                          double durationSeconds)
{
    const auto duration = std::max (0.0, durationSeconds);
    auto windowDuration = std::max (0.001, visibleRange.endSeconds - visibleRange.startSeconds);
    windowDuration = std::min (windowDuration, std::max (windowDuration, duration));

    if (duration <= 0.0 || windowDuration >= duration)
        return { 0.0, duration };

    const auto playhead = std::clamp (playheadSeconds, 0.0, duration);
    const auto leftGuard = visibleRange.startSeconds + windowDuration * 0.25;
    const auto rightGuard = visibleRange.startSeconds + windowDuration * 0.70;

    if (playhead >= leftGuard && playhead <= rightGuard)
        return visibleRange;

    auto start = playhead - windowDuration * 0.35;
    start = std::clamp (start, 0.0, duration - windowDuration);
    return { start, start + windowDuration };
}

WaveformLaneLayout calculateWaveformLaneLayout (float areaX,
                                                float areaWidth,
                                                float channelLabelWidth)
{
    const auto safeWidth = std::max (0.0f, areaWidth);
    const auto safeLabelWidth = std::clamp (channelLabelWidth, 0.0f, safeWidth);
    return { areaX, safeWidth, areaX, safeLabelWidth };
}

FxDetailLayout calculateFxDetailLayout (BuiltInEffectId effect, int x, int y, int width, int height)
{
    const auto safeWidth = std::max (0, width);
    const auto safeHeight = std::max (0, height);

    auto row = [&] (int rowY, int rowHeight)
    {
        const auto clampedY = std::clamp (rowY, y, y + safeHeight);
        const auto clampedHeight = std::max (0, std::min (rowHeight, y + safeHeight - clampedY));
        return UiRect { x, clampedY, safeWidth, clampedHeight };
    };

    auto slot = [&] (const UiRect& sourceRow, int index)
    {
        constexpr int slotWidth = 86;
        constexpr int slotGap = 8;
        const auto slotX = sourceRow.x + index * (slotWidth + slotGap);
        const auto availableWidth = std::max (0, sourceRow.x + sourceRow.width - slotX);
        return UiRect { slotX, sourceRow.y, std::min (slotWidth, availableWidth), sourceRow.height };
    };

    auto withSlots = [&] (FxDetailLayout layout)
    {
        layout.knobA = slot (layout.knobRow, 0);
        layout.knobB = slot (layout.knobRow, 1);
        layout.knobC = slot (layout.knobRow, 2);
        return layout;
    };

    if (effect == BuiltInEffectId::gain)
        return withSlots ({ row (y, 58), {}, {}, {}, {}, {}, {}, row (y + 60, 22) });

    if (effect == BuiltInEffectId::mono)
        return withSlots ({ row (y, 44), {}, {}, {}, {}, {}, {}, {} });

    if (effect == BuiltInEffectId::normalize)
    {
        const auto labelY = y + 30;
        const auto knobY = labelY + 18;
        return withSlots ({ row (y, 28), row (labelY, 18), row (knobY, 68), {}, {}, {}, {}, row (knobY + 70, 22) });
    }

    if (effect == BuiltInEffectId::filter)
    {
        const auto labelY = y + 30;
        const auto knobY = labelY + 18;
        return withSlots ({ {}, row (labelY, 18), row (knobY, 68), {}, {}, {}, row (y, 28), row (knobY + 70, 22) });
    }

    return withSlots ({
        {},
        row (y, 18),
        row (y + 18, 72),
        {},
        {},
        {},
        {},
        row (y + 92, 22)
    });
}

Pack Pack::create (std::string name)
{
    return Pack { std::move (name) };
}

std::optional<Pack> Pack::loadSessionFromDisk (const std::filesystem::path& filePath)
{
    std::ifstream stream { filePath };
    if (! stream)
        return std::nullopt;

    std::string line;
    std::getline (stream, line);
    if (line != "SFB_PACK_V1")
        return std::nullopt;

    auto pack = Pack::create ("UNTITLED_PACK");
    pack.drums.clear();
    pack.bass.clear();
    pack.melody.clear();
    pack.other.clear();
    pack.selectedSampleId = std::nullopt;
    pack.nextSampleId = 1;
    std::optional<std::size_t> savedSelectedId;

    while (std::getline (stream, line))
    {
        const auto fields = splitCacheLine (line);
        if (fields.empty())
            continue;

        if (fields[0] == "PACK" && fields.size() >= 4)
        {
            pack.packName = fields[1];
            pack.nextSampleId = static_cast<std::size_t> (std::max<long long> (1, parseLongLong (fields[2])));
            if (! fields[3].empty())
                savedSelectedId = static_cast<std::size_t> (std::max<long long> (1, parseLongLong (fields[3])));
        }
        else if (fields[0] == "SAMPLE" && fields.size() >= 12)
        {
            Sample sample;
            sample.id = static_cast<std::size_t> (std::max<long long> (1, parseLongLong (fields[1])));
            sample.bucket = enumFromIndex (fields[2], BucketId::other);
            sample.sourcePath = fields[3];
            sample.displayName = fields[4];
            sample.shortName = fields[5];
            sample.type = enumFromIndex (fields[6], SampleType::oneShot);
            sample.sourceBpm = parseOptionalDouble (fields[7]);
            sample.musicalBpm = parseOptionalDouble (fields[8]);
            sample.bars = parseOptionalInt (fields[9]);
            sample.key = fields[10];
            sample.bench.type = sample.type;
            sample.bench.name = fields[11];
            sample.bench.musicalBpm = sample.musicalBpm.value_or (0.0);
            sample.bench.bars = sample.bars.value_or (0);
            sample.bench.key = sample.key;
            sample.variations.clear();
            pack.nextSampleId = std::max (pack.nextSampleId, sample.id + 1);
            pack.mutableSamplesInBucket (sample.bucket).push_back (std::move (sample));
        }
        else if (fields[0] == "BENCH" && fields.size() >= 3)
        {
            if (auto* sample = pack.findSample (static_cast<std::size_t> (parseLongLong (fields[1]))))
            {
                sample->bench = benchFields (fields, 2, sample->bench);
                sample->type = sample->bench.type;
                sample->musicalBpm = sample->bench.musicalBpm > 0.0 ? std::optional<double> { sample->bench.musicalBpm } : std::nullopt;
                sample->bars = sample->bench.bars > 0 ? std::optional<int> { sample->bench.bars } : std::nullopt;
                sample->key = sample->bench.key;
            }
        }
        else if (fields[0] == "EFFECT" && fields.size() >= 3)
        {
            if (auto* sample = pack.findSample (static_cast<std::size_t> (parseLongLong (fields[1]))))
                sample->bench.effectChain.push_back (enumFromIndex (fields[2], BuiltInEffectId::gain));
        }
        else if (fields[0] == "FXMODULE" && fields.size() >= 4)
        {
            if (auto* sample = pack.findSample (static_cast<std::size_t> (parseLongLong (fields[1]))))
            {
                FxModule module;
                module.kind = enumFromIndex (fields[2], FxModuleKind::builtIn);
                if (module.kind == FxModuleKind::builtIn)
                {
                    module.builtIn = enumFromIndex (fields[3], BuiltInEffectId::gain);
                }
                else if (fields.size() >= 14)
                {
                    module.plugin.name = fields[3];
                    module.plugin.manufacturer = fields[4];
                    module.plugin.format = fields[5];
                    module.plugin.category = fields[6];
                    module.plugin.filePath = fields[7];
                    module.plugin.uniqueId = fields[8];
                    module.plugin.modifiedTimeMillis = parseLongLong (fields[9]);
                    module.plugin.enabled = parseBool (fields[10], true);
                    module.plugin.status = enumFromIndex (fields[11], PluginModuleStatus::loaded);
                    module.plugin.errorMessage = fields[12];
                    module.plugin.stateBlob = hexToBytes (fields[13]);
                }
                sample->bench.fxModules.push_back (std::move (module));
            }
        }
        else if (fields[0] == "VARIATION" && fields.size() >= 8)
        {
            if (auto* sample = pack.findSample (static_cast<std::size_t> (parseLongLong (fields[1]))))
            {
                RenderedVariation variation;
                variation.filePath = fields[2];
                variation.displayName = fields[3];
                variation.finalFilename = fields[4];
                variation.sourceSampleId = static_cast<std::size_t> (parseLongLong (fields[5]));
                variation.sourcePath = fields[6];
                variation.settings = benchFields (fields, 7, sample->bench);
                sample->variations.push_back (std::move (variation));
            }
        }
    }

    if (savedSelectedId.has_value())
        pack.selectSample (*savedSelectedId);

    return pack;
}

Pack::Pack (std::string name)
    : packName (std::move (name)),
      packBuckets (makeDefaultBuckets())
{
}

std::string_view Pack::name() const noexcept
{
    return packName;
}

bool Pack::rename (std::string newName)
{
    newName = trimWhitespace (std::move (newName));
    if (newName.empty())
        return false;

    packName = std::move (newName);
    return true;
}

const std::vector<Bucket>& Pack::buckets() const noexcept
{
    return packBuckets;
}

bool Pack::hasSamples() const
{
    return ! drums.empty()
        || ! bass.empty()
        || ! melody.empty()
        || ! other.empty();
}

Sample Pack::importSample (BucketId bucket, std::filesystem::path sourcePath)
{
    Sample sample;
    sample.id = nextSampleId++;
    sample.bucket = bucket;
    sample.sourcePath = std::move (sourcePath);
    sample.displayName = sample.sourcePath.filename().string();
    sample.shortName = shortNameFromPath (sample.sourcePath);
    sample.type = detectSampleTypeFromFilename (sample.sourcePath);
    sample.bench.name = suggestExportNameFromFilename (sample.sourcePath);
    sample.bench.type = sample.type;
    if (const auto bpm = extractBpmFromFilename (sample.sourcePath))
    {
        sample.sourceBpm = static_cast<double> (*bpm);
        sample.musicalBpm = static_cast<double> (*bpm);
        sample.bench.musicalBpm = static_cast<double> (*bpm);
    }
    if (sample.type == SampleType::loop)
    {
        sample.bars = 4;
        sample.bench.bars = 4;
    }

    auto& samples = mutableSamplesInBucket (bucket);
    samples.push_back (sample);
    return sample;
}

std::vector<Sample>& Pack::mutableSamplesInBucket (BucketId bucket)
{
    switch (bucket)
    {
        case BucketId::drums: return drums;
        case BucketId::bass: return bass;
        case BucketId::melody: return melody;
        case BucketId::other: return other;
    }

    throw std::logic_error { "Unknown bucket" };
}

const std::vector<Sample>& Pack::samplesInBucket (BucketId bucket) const
{
    switch (bucket)
    {
        case BucketId::drums: return drums;
        case BucketId::bass: return bass;
        case BucketId::melody: return melody;
        case BucketId::other: return other;
    }

    throw std::logic_error { "Unknown bucket" };
}

bool Pack::selectSample (std::size_t sampleId)
{
    if (findSample (sampleId) == nullptr)
        return false;

    selectedSampleId = sampleId;
    return true;
}

void Pack::clearSelection()
{
    selectedSampleId = std::nullopt;
}

bool Pack::moveSampleToBucket (std::size_t sampleId, BucketId targetBucket)
{
    for (auto* samples : { &drums, &bass, &melody, &other })
    {
        const auto found = std::find_if (samples->begin(), samples->end(), [sampleId] (const Sample& sample)
        {
            return sample.id == sampleId;
        });

        if (found == samples->end())
            continue;

        if (found->bucket == targetBucket)
            return true;

        auto moved = std::move (*found);
        samples->erase (found);
        moved.bucket = targetBucket;
        mutableSamplesInBucket (targetBucket).push_back (std::move (moved));
        return true;
    }

    return false;
}

std::optional<Sample> Pack::selectedSample() const
{
    if (! selectedSampleId.has_value())
        return std::nullopt;

    if (const auto* sample = findSample (*selectedSampleId))
        return *sample;

    return std::nullopt;
}

bool Pack::updateBenchSettings (std::size_t sampleId, const BenchSettings& settings)
{
    auto* sample = findSample (sampleId);
    if (sample == nullptr)
        return false;

    sample->bench = settings;
    sample->type = settings.type;
    sample->musicalBpm = settings.musicalBpm > 0.0 ? std::optional<double> { settings.musicalBpm } : std::nullopt;
    sample->bars = settings.bars > 0 ? std::optional<int> { settings.bars } : std::nullopt;
    sample->key = settings.key;
    return true;
}

bool Pack::keepVariation (std::size_t sampleId, RenderedVariation variation)
{
    auto* sample = findSample (sampleId);
    if (sample == nullptr)
        return false;

    variation.sourceSampleId = sampleId;
    variation.sourcePath = sample->sourcePath;
    sample->variations.push_back (std::move (variation));
    return true;
}

bool Pack::saveSessionToDisk (const std::filesystem::path& filePath) const
{
    const auto parent = filePath.parent_path();
    if (! parent.empty())
        std::filesystem::create_directories (parent);

    std::ofstream stream { filePath, std::ios::trunc };
    if (! stream)
        return false;

    stream << "SFB_PACK_V1\n";
    writeCacheLine (stream, { "PACK",
                              packName,
                              std::to_string (nextSampleId),
                              selectedSampleId.has_value() ? std::to_string (*selectedSampleId) : "" });

    const auto writeSample = [&] (const Sample& sample)
    {
        writeCacheLine (stream,
                        { "SAMPLE",
                          std::to_string (sample.id),
                          enumIndex (sample.bucket),
                          sample.sourcePath.string(),
                          sample.displayName,
                          sample.shortName,
                          enumIndex (sample.type),
                          optionalDoubleString (sample.sourceBpm),
                          optionalDoubleString (sample.musicalBpm),
                          optionalIntString (sample.bars),
                          sample.key,
                          sample.bench.name });

        std::vector<std::string> benchFieldsLine { "BENCH", std::to_string (sample.id) };
        appendBenchFields (benchFieldsLine, sample.bench);
        writeCacheLine (stream, benchFieldsLine);

        for (const auto effect : sample.bench.effectChain)
            writeCacheLine (stream, { "EFFECT", std::to_string (sample.id), enumIndex (effect) });

        for (const auto& module : sample.bench.fxModules)
        {
            if (module.kind == FxModuleKind::builtIn)
            {
                writeCacheLine (stream, { "FXMODULE",
                                          std::to_string (sample.id),
                                          enumIndex (module.kind),
                                          enumIndex (module.builtIn) });
            }
            else
            {
                writeCacheLine (stream, { "FXMODULE",
                                          std::to_string (sample.id),
                                          enumIndex (module.kind),
                                          module.plugin.name,
                                          module.plugin.manufacturer,
                                          module.plugin.format,
                                          module.plugin.category,
                                          module.plugin.filePath.string(),
                                          module.plugin.uniqueId,
                                          std::to_string (module.plugin.modifiedTimeMillis),
                                          boolString (module.plugin.enabled),
                                          enumIndex (module.plugin.status),
                                          module.plugin.errorMessage,
                                          bytesToHex (module.plugin.stateBlob) });
            }
        }

        for (const auto& variation : sample.variations)
        {
            std::vector<std::string> variationLine { "VARIATION",
                                                     std::to_string (sample.id),
                                                     variation.filePath.string(),
                                                     variation.displayName,
                                                     variation.finalFilename,
                                                     std::to_string (variation.sourceSampleId),
                                                     variation.sourcePath.string() };
            appendBenchFields (variationLine, variation.settings);
            writeCacheLine (stream, variationLine);
        }
    };

    for (const auto* samples : { &drums, &bass, &melody, &other })
        for (const auto& sample : *samples)
            writeSample (sample);

    return static_cast<bool> (stream);
}

Sample* Pack::findSample (std::size_t sampleId)
{
    for (auto* samples : { &drums, &bass, &melody, &other })
    {
        const auto found = std::find_if (samples->begin(), samples->end(), [sampleId] (const Sample& sample)
        {
            return sample.id == sampleId;
        });

        if (found != samples->end())
            return &*found;
    }

    return nullptr;
}

const Sample* Pack::findSample (std::size_t sampleId) const
{
    for (const auto* samples : { &drums, &bass, &melody, &other })
    {
        const auto found = std::find_if (samples->begin(), samples->end(), [sampleId] (const Sample& sample)
        {
            return sample.id == sampleId;
        });

        if (found != samples->end())
            return &*found;
    }

    return nullptr;
}
}
