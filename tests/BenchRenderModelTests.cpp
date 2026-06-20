#include "model/SampleBenchModel.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
bool nearlyEqual (double first, double second)
{
    return std::abs (first - second) < 0.0001;
}

bool nearlyEqualFloat (float first, float second)
{
    return std::abs (first - second) < 0.0001f;
}

void testFilenameBuilderNormalLoop()
{
    samplebench::BenchSettings settings;
    settings.type = samplebench::SampleType::loop;
    settings.name = "amen";
    settings.flavor = samplebench::RenderFlavor::wet;
    settings.version = 2;
    settings.musicalBpm = 92.0;

    assert (samplebench::buildFinalFilename (settings) == "amen_wet_v2_092BPM.wav");
}

void testFilenameBuilderSpeedTrickLoop()
{
    samplebench::BenchSettings settings;
    settings.type = samplebench::SampleType::loop;
    settings.name = "amen";
    settings.flavor = samplebench::RenderFlavor::wet;
    settings.version = 3;
    settings.musicalBpm = 92.0;
    settings.speedTrickEnabled = true;

    assert (samplebench::buildFinalFilename (settings) == "amen_wet_v3_spdup_092BPM.wav");
}

void testSuggestsShortExportNameFromCymaticsLoop()
{
    assert (samplebench::suggestExportNameFromFilename ("Cymatics - Blaze Drum Loop 1 - 120 BPM.wav") == "blaze");
}

void testSuggestsExportNameKeepsMeaningfulNumbers()
{
    assert (samplebench::suggestExportNameFromFilename ("Cymatics - 808 Mob Full Drum Loop 1 - 75 BPM.wav")
            == "808_mob_full");
}

void testExtractsBpmFromFilename()
{
    assert (samplebench::extractBpmFromFilename ("Cymatics - Blaze Drum Loop 1 - 120 BPM.wav") == 120);
    assert (samplebench::extractBpmFromFilename ("break_086BPM.wav") == 86);
}

void testDetectsLoopFromFilename()
{
    assert (samplebench::detectSampleTypeFromFilename ("Cymatics - Blaze Drum Loop 1 - 120 BPM.wav")
            == samplebench::SampleType::loop);
}

void testDetectsOneShotFromFilename()
{
    assert (samplebench::detectSampleTypeFromFilename ("Analog Snare One Shot.wav")
            == samplebench::SampleType::oneShot);
}

void testMiddleTruncationPreservesBpmEnding()
{
    const auto truncated = samplebench::middleTruncatePreservingEnding (
        "Cymatics_Blaze_Big_Drum_Loop_dry_v1_120BPM.wav",
        32);

    assert (truncated.size() <= 32);
    assert (truncated.starts_with ("Cymatics"));
    assert (truncated.ends_with ("_dry_v1_120BPM.wav"));
}

void testImportedCymaticsLoopGetsUsefulDefaults()
{
    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");
    const auto sample = pack.importSample (samplebench::BucketId::drums,
                                           "/samples/Cymatics - Blaze Drum Loop 1 - 120 BPM.wav");

    assert (sample.type == samplebench::SampleType::loop);
    assert (sample.bench.type == samplebench::SampleType::loop);
    assert (sample.bench.musicalBpm == 120.0);
    assert (sample.bench.bars == 4);
    assert (sample.bench.name == "blaze");
    assert (sample.bench.flavor == samplebench::RenderFlavor::dry);
    assert (sample.bench.version == 1);
    assert (samplebench::buildFinalFilename (sample.bench) == "blaze_dry_v1_120BPM.wav");
}

void testImportedCymatics808LoopGetsUsefulDefaults()
{
    auto pack = samplebench::Pack::create ("EP133_BREAKS_01");
    const auto sample = pack.importSample (samplebench::BucketId::drums,
                                           "/samples/Cymatics - 808 Mob Full Drum Loop 1 - 75 BPM.wav");

    assert (sample.bench.name == "808_mob_full");
    assert (samplebench::buildFinalFilename (sample.bench) == "808_mob_full_dry_v1_075BPM.wav");
}

void testFilenameBuilderOneShotOmitsBpm()
{
    samplebench::BenchSettings settings;
    settings.type = samplebench::SampleType::oneShot;
    settings.name = "kick";
    settings.flavor = samplebench::RenderFlavor::dry;
    settings.version = 1;
    settings.musicalBpm = 128.0;

    assert (samplebench::buildFinalFilename (settings) == "kick_dry_v1.wav");
}

void testBpmIsLastForLoops()
{
    samplebench::BenchSettings settings;
    settings.type = samplebench::SampleType::loop;
    settings.name = "break";
    settings.flavor = samplebench::RenderFlavor::wet;
    settings.version = 1;
    settings.musicalBpm = 86.0;
    settings.speedTrickEnabled = true;

    const auto filename = samplebench::buildFinalFilename (settings);
    assert (filename.ends_with ("086BPM.wav"));
    assert (filename.find ("spdup") < filename.find ("086BPM"));
}

void testCaptureSettingsConvertsBarsToSeconds()
{
    samplebench::CaptureSettings capture;
    capture.captureStartBar = 3;
    capture.warmupBars = 2;
    capture.keepBars = 4;
    capture.tailBars = 1;

    const auto timing = samplebench::calculateCaptureTiming (120.0, capture);

    assert (nearlyEqual (timing.secondsPerBar, 2.0));
    assert (nearlyEqual (timing.internalStartSeconds, 0.0));
    assert (nearlyEqual (timing.keepStartSeconds, 4.0));
    assert (nearlyEqual (timing.internalDurationSeconds, 14.0));
    assert (nearlyEqual (timing.exportedDurationSeconds, 10.0));
}

void testWarmupIsExcludedFromExportedKeepRegion()
{
    samplebench::CaptureSettings capture;
    capture.captureStartBar = 1;
    capture.warmupBars = 8;
    capture.keepBars = 4;
    capture.tailBars = 0;

    const auto timing = samplebench::calculateCaptureTiming (92.0, capture);
    const auto secondsPerBar = 60.0 / 92.0 * 4.0;

    assert (nearlyEqual (timing.internalStartSeconds, 0.0));
    assert (nearlyEqual (timing.exportOffsetInInternalSeconds, 8.0 * secondsPerBar));
    assert (nearlyEqual (timing.exportedDurationSeconds, 4.0 * secondsPerBar));
}

void testStartBarMeansKeepStart()
{
    samplebench::CaptureSettings capture;
    capture.captureStartBar = 5;
    capture.warmupBars = 4;
    capture.keepBars = 4;
    capture.tailBars = 0;

    const auto regions = samplebench::calculateCaptureRegions (120.0, capture);

    assert (nearlyEqual (regions.keep.startSeconds, 8.0));
    assert (nearlyEqual (regions.keep.endSeconds, 16.0));
    assert (nearlyEqual (regions.warmup.startSeconds, 0.0));
    assert (nearlyEqual (regions.warmup.endSeconds, 8.0));
}

void testSourceLoopRegionEqualsKeepRegion()
{
    samplebench::CaptureSettings capture;
    capture.captureStartBar = 5;
    capture.warmupBars = 4;
    capture.keepBars = 4;
    capture.tailBars = 0;

    const auto regions = samplebench::calculateCaptureRegions (120.0, capture);
    const auto loop = samplebench::sourcePreviewLoopRegion (regions);

    assert (nearlyEqual (loop.startSeconds, regions.keep.startSeconds));
    assert (nearlyEqual (loop.endSeconds, regions.keep.endSeconds));
}

void testBounceLoopRegionEqualsBounceDuration()
{
    const auto loop = samplebench::bouncePreviewLoopRegion (7.059);

    assert (nearlyEqual (loop.startSeconds, 0.0));
    assert (nearlyEqual (loop.endSeconds, 7.059));
}

void testTailIncludedOnlyWhenTailIsEnabled()
{
    samplebench::CaptureSettings withoutTail;
    withoutTail.keepBars = 4;
    withoutTail.tailBars = 0;

    samplebench::CaptureSettings withTail;
    withTail.keepBars = 4;
    withTail.tailBars = 2;

    const auto noTail = samplebench::calculateCaptureRegions (120.0, withoutTail);
    const auto tail = samplebench::calculateCaptureRegions (120.0, withTail);

    assert (nearlyEqual (noTail.bounce.endSeconds - noTail.bounce.startSeconds, 8.0));
    assert (nearlyEqual (tail.bounce.endSeconds - tail.bounce.startSeconds, 12.0));
    assert (nearlyEqual (noTail.tail.startSeconds, noTail.tail.endSeconds));
}

void testBedDurationFromBpmAndBarCount()
{
    assert (nearlyEqual (samplebench::calculateBedDurationSeconds (120.0, 8), 16.0));
}

void testLoopContinuouslyRepeatsMonoSourceToFillBed()
{
    samplebench::AudioBedRequest request;
    request.sampleRate = 2.0;
    request.musicalBpm = 60.0;
    request.bedLengthBars = 1;
    request.triggerMode = samplebench::BedTriggerMode::loopContinuously;

    const auto bed = samplebench::buildMonoAudioBed ({ 1.0f, -1.0f }, request);
    const std::vector<float> expected { 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f };

    assert (bed == expected);
}

void testOncePerBarPlacesMonoSourceAtEachBarStart()
{
    samplebench::AudioBedRequest request;
    request.sampleRate = 4.0;
    request.musicalBpm = 60.0;
    request.bedLengthBars = 2;
    request.triggerMode = samplebench::BedTriggerMode::oncePerBar;

    const auto bed = samplebench::buildMonoAudioBed ({ 1.0f, 0.5f }, request);

    assert (bed.size() == 32);
    assert (bed[0] == 1.0f);
    assert (bed[1] == 0.5f);
    assert (bed[16] == 1.0f);
    assert (bed[17] == 0.5f);
    assert (bed[15] == 0.0f);
    assert (bed[18] == 0.0f);
}

void testCaptureRegionCanSelectKeepFromGeneratedBed()
{
    samplebench::CaptureSettings capture;
    capture.captureStartBar = 5;
    capture.warmupBars = 4;
    capture.keepBars = 4;
    capture.tailBars = 0;

    const auto regions = samplebench::calculateCaptureRegions (120.0, capture);
    const auto bedDuration = samplebench::calculateBedDurationSeconds (120.0, 8);

    assert (nearlyEqual (bedDuration, 16.0));
    assert (nearlyEqual (regions.keep.startSeconds, 8.0));
    assert (nearlyEqual (regions.keep.endSeconds, bedDuration));
}

void testFourBucketModelStillWorks()
{
    const auto pack = samplebench::Pack::create ("EP133_BREAKS_01");
    const auto& buckets = pack.buckets();

    assert (buckets.size() == 4);
    assert (buckets[0].folderName == "A_DRUMS");
    assert (buckets[1].folderName == "B_BASS");
    assert (buckets[2].folderName == "C_MELODY");
    assert (buckets[3].folderName == "D_OTHER");
}

void testPlaybackStartsOneTargetAtATime()
{
    samplebench::PlaybackState state;
    state = samplebench::startPlayback (state, samplebench::PlaybackTarget::source);

    assert (state.target == samplebench::PlaybackTarget::source);
    assert (state.isPlaying);

    state = samplebench::startPlayback (state, samplebench::PlaybackTarget::bounce);

    assert (state.target == samplebench::PlaybackTarget::bounce);
    assert (state.isPlaying);
}

void testStopPlaybackClearsTargetButKeepsLoopPreference()
{
    samplebench::PlaybackState state;
    state.loopEnabled = true;
    state = samplebench::startPlayback (state, samplebench::PlaybackTarget::source);
    state = samplebench::stopPlayback (state);

    assert (state.target == samplebench::PlaybackTarget::none);
    assert (! state.isPlaying);
    assert (state.loopEnabled);
}

void testLoopPreferenceDoesNotStartPlayback()
{
    samplebench::PlaybackState state;
    state = samplebench::setLoopEnabled (state, true);

    assert (state.loopEnabled);
    assert (! state.isPlaying);
    assert (state.target == samplebench::PlaybackTarget::none);
}

void testPreviewTargetDefaultsToSource()
{
    const samplebench::PlaybackState state;

    assert (state.previewTarget == samplebench::PlaybackTarget::source);
}

void testPreviewTargetSelectionDoesNotStartPlayback()
{
    samplebench::PlaybackState state;
    state = samplebench::setPreviewTarget (state, samplebench::PlaybackTarget::bounce);

    assert (state.previewTarget == samplebench::PlaybackTarget::bounce);
    assert (state.target == samplebench::PlaybackTarget::none);
    assert (! state.isPlaying);
}

void testPlaybackUsesSelectedPreviewTarget()
{
    samplebench::PlaybackState state;
    state = samplebench::setPreviewTarget (state, samplebench::PlaybackTarget::bounce);
    state = samplebench::startSelectedPlayback (state);

    assert (state.target == samplebench::PlaybackTarget::bounce);
    assert (state.isPlaying);
}

void testWaveformWindowShowsWholeShortFile()
{
    const auto window = samplebench::makeInitialVisibleWindow (6.0, 2.0, 4.0);

    assert (nearlyEqual (window.startSeconds, 0.0));
    assert (nearlyEqual (window.endSeconds, 6.0));
}

void testWaveformWindowCentersCaptureForLongFile()
{
    const auto window = samplebench::makeInitialVisibleWindow (60.0, 20.0, 8.0);

    assert (window.startSeconds > 0.0);
    assert (window.startSeconds < 20.0);
    assert (window.endSeconds > 28.0);
    assert (window.endSeconds <= 60.0);
}

void testAutoFollowKeepsPlayheadVisible()
{
    samplebench::VisibleTimeRange window;
    window.startSeconds = 0.0;
    window.endSeconds = 10.0;

    const auto followed = samplebench::autoFollowVisibleWindow (window, 14.0, 60.0);

    assert (followed.startSeconds > 0.0);
    assert (followed.endSeconds > 14.0);
    assert (followed.startSeconds <= 14.0);
}

void testAutoFollowDoesNotScrollWhenPlayheadVisible()
{
    samplebench::VisibleTimeRange window;
    window.startSeconds = 10.0;
    window.endSeconds = 20.0;

    const auto followed = samplebench::autoFollowVisibleWindow (window, 13.0, 60.0);

    assert (nearlyEqual (followed.startSeconds, 10.0));
    assert (nearlyEqual (followed.endSeconds, 20.0));
}

void testWaveformTimelineIsNotShiftedByChannelLabel()
{
    const auto layout = samplebench::calculateWaveformLaneLayout (12.0f, 900.0f, 42.0f);

    assert (nearlyEqual (layout.timelineX, 12.0));
    assert (nearlyEqual (layout.timelineWidth, 900.0));
    assert (nearlyEqual (layout.labelX, 12.0));
    assert (nearlyEqual (layout.labelWidth, 42.0));
}

void testGainEffectChangesAmplitude()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { { 0.25f, -0.25f } };

    samplebench::BenchSettings settings;
    settings.gainDecibels = 6.0f;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    assert (processed.channels.size() == 1);
    assert (processed.channels[0][0] > audio.channels[0][0]);
    assert (processed.channels[0][1] < audio.channels[0][1]);
}

void testMonoEffectAveragesStereoChannels()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { { 1.0f, -1.0f }, { -1.0f, 1.0f } };

    samplebench::BenchSettings settings;
    settings.monoEnabled = true;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    assert (processed.channels.size() == 2);
    assert (nearlyEqualFloat (processed.channels[0][0], 0.0f));
    assert (nearlyEqualFloat (processed.channels[1][0], 0.0f));
    assert (nearlyEqualFloat (processed.channels[0][1], 0.0f));
    assert (nearlyEqualFloat (processed.channels[1][1], 0.0f));
}

void testCrushEffectQuantizesAndHoldsSamples()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { { 0.0f, 0.1f, 0.2f, 0.3f } };

    samplebench::BenchSettings settings;
    settings.crushEnabled = true;
    settings.crushBits = 8;
    settings.crushSampleRate = 11025.0;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    assert (processed.channels[0][0] == processed.channels[0][1]);
    assert (processed.channels[0][1] == processed.channels[0][2]);
    assert (processed.channels[0][2] == processed.channels[0][3]);
    assert (processed.channels[0][0] != audio.channels[0][1]);
}

void testCrushFullBlastMakesObviousAudioChange()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { {} };
    for (int index = 0; index < 256; ++index)
        audio.channels[0].push_back (-1.0f + static_cast<float> (index) / 127.5f);

    samplebench::BenchSettings settings;
    settings.crushEnabled = true;
    settings.crushBits = 8;
    settings.crushSampleRate = 11025.0;
    settings.crushMix = 1.0f;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::bounce);

    double absoluteDifference = 0.0;
    int heldSampleCount = 0;
    for (std::size_t index = 0; index < audio.channels[0].size(); ++index)
    {
        absoluteDifference += std::abs (processed.channels[0][index] - audio.channels[0][index]);
        if (index > 0 && nearlyEqualFloat (processed.channels[0][index], processed.channels[0][index - 1]))
            ++heldSampleCount;
    }

    assert (absoluteDifference > 8.0);
    assert (heldSampleCount > 120);
}

void testFilterEffectProcessesAndChangesOutput()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { { 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f } };

    samplebench::BenchSettings settings;
    settings.filterEnabled = true;
    settings.filterMode = samplebench::FilterMode::lowPass;
    settings.filterCutoffHz = 2000.0f;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    assert (processed.channels[0].size() == audio.channels[0].size());
    assert (! nearlyEqualFloat (processed.channels[0][1], audio.channels[0][1]));
}

void testLimiterPreventsClipping()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { { 0.7f, -0.8f, 0.4f } };

    samplebench::BenchSettings settings;
    settings.customEffectChain = true;
    settings.effectChain = { samplebench::BuiltInEffectId::limit };
    settings.limitEnabled = true;
    settings.limitCeilingDecibels = -6.0f;
    settings.limitInputDecibels = 12.0f;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);
    const auto ceiling = static_cast<float> (std::pow (10.0, -6.0 / 20.0));

    for (const auto sample : processed.channels[0])
        assert (std::abs (sample) <= ceiling + 0.0001f);
}

void testCompressorReducesLevelAboveThreshold()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { { 0.05f, 0.9f } };

    samplebench::BenchSettings settings;
    settings.customEffectChain = true;
    settings.effectChain = { samplebench::BuiltInEffectId::compressor };
    settings.compressorEnabled = true;
    settings.compressorThresholdDecibels = -18.0f;
    settings.compressorRatio = 8.0f;
    settings.compressorMix = 1.0f;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    assert (nearlyEqualFloat (processed.channels[0][0], audio.channels[0][0]));
    assert (std::abs (processed.channels[0][1]) < std::abs (audio.channels[0][1]));
}

void testDriveChangesWaveformAndClampsSafely()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { { -0.8f, -0.2f, 0.2f, 0.8f } };

    samplebench::BenchSettings settings;
    settings.customEffectChain = true;
    settings.effectChain = { samplebench::BuiltInEffectId::drive };
    settings.driveEnabled = true;
    settings.driveAmount = 1.0f;
    settings.driveMix = 1.0f;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    double difference = 0.0;
    for (std::size_t index = 0; index < audio.channels[0].size(); ++index)
    {
        difference += std::abs (processed.channels[0][index] - audio.channels[0][index]);
        assert (std::abs (processed.channels[0][index]) <= 1.0f);
    }
    assert (difference > 0.05);
}

void testEqLowMidHighAffectSignal()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { {} };
    for (int index = 0; index < 128; ++index)
        audio.channels[0].push_back (index % 2 == 0 ? 0.5f : -0.5f);

    samplebench::BenchSettings settings;
    settings.customEffectChain = true;
    settings.effectChain = { samplebench::BuiltInEffectId::eq };
    settings.eqEnabled = true;
    settings.eqLowDecibels = 6.0f;
    settings.eqMidDecibels = -3.0f;
    settings.eqHighDecibels = 6.0f;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    double difference = 0.0;
    for (std::size_t index = 0; index < audio.channels[0].size(); ++index)
        difference += std::abs (processed.channels[0][index] - audio.channels[0][index]);

    assert (difference > 0.1);
}

void testDelayProducesSyncedRepeatsFromBpm()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 16.0;
    audio.channels = { std::vector<float> (32, 0.0f) };
    audio.channels[0][0] = 1.0f;

    samplebench::BenchSettings settings;
    settings.customEffectChain = true;
    settings.effectChain = { samplebench::BuiltInEffectId::delay };
    settings.musicalBpm = 60.0;
    settings.delayEnabled = true;
    settings.delayDivision = 2;
    settings.delayFeedback = 0.5f;
    settings.delayMix = 0.5f;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    assert (processed.channels[0][16] > 0.2f);
}

void testDelayWithoutBpmDoesNotInventTiming()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 16.0;
    audio.channels = { std::vector<float> (32, 0.0f) };
    audio.channels[0][0] = 1.0f;

    samplebench::BenchSettings settings;
    settings.customEffectChain = true;
    settings.effectChain = { samplebench::BuiltInEffectId::delay };
    settings.musicalBpm = 0.0;
    settings.delayEnabled = true;
    settings.delayDivision = 2;
    settings.delayFeedback = 0.5f;
    settings.delayMix = 0.5f;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    assert (processed.channels == audio.channels);
}

void testReverbProducesTail()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 100.0;
    audio.channels = { std::vector<float> (120, 0.0f) };
    audio.channels[0][0] = 1.0f;

    samplebench::BenchSettings settings;
    settings.customEffectChain = true;
    settings.effectChain = { samplebench::BuiltInEffectId::reverb };
    settings.reverbEnabled = true;
    settings.reverbMix = 0.5f;
    settings.reverbDecaySeconds = 2.0f;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    double tailEnergy = 0.0;
    for (std::size_t index = 10; index < processed.channels[0].size(); ++index)
        tailEnergy += std::abs (processed.channels[0][index]);

    assert (tailEnergy > 0.1);
}

void testTapeChangesSignalWithoutClipping()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { {} };
    for (int index = 0; index < 128; ++index)
        audio.channels[0].push_back (0.25f);

    samplebench::BenchSettings settings;
    settings.customEffectChain = true;
    settings.effectChain = { samplebench::BuiltInEffectId::tape };
    settings.tapeEnabled = true;
    settings.tapeDrive = 0.8f;
    settings.tapeWobble = 0.5f;
    settings.tapeNoise = 0.1f;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    double difference = 0.0;
    for (std::size_t index = 0; index < processed.channels[0].size(); ++index)
    {
        difference += std::abs (processed.channels[0][index] - audio.channels[0][index]);
        assert (std::abs (processed.channels[0][index]) <= 1.0f);
    }

    assert (difference > 0.1);
}

void testDisabledEffectsDoNotChangeAudio()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { { 0.2f, -0.4f, 0.6f } };

    samplebench::BenchSettings settings;
    settings.gainEnabled = false;
    settings.monoEnabled = false;
    settings.normalizeEnabled = false;
    settings.crushEnabled = false;
    settings.filterEnabled = false;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::bounce);

    assert (processed.channels == audio.channels);
}

void testEffectChainOrderIsStable()
{
    const auto order = samplebench::fixedEffectChainOrder();

    assert (order.size() == 12);
    assert (order[0] == samplebench::BuiltInEffectId::gain);
    assert (order[1] == samplebench::BuiltInEffectId::mono);
    assert (order[2] == samplebench::BuiltInEffectId::normalize);
    assert (order[3] == samplebench::BuiltInEffectId::limit);
    assert (order[4] == samplebench::BuiltInEffectId::compressor);
    assert (order[5] == samplebench::BuiltInEffectId::crush);
    assert (order[6] == samplebench::BuiltInEffectId::filter);
    assert (order[7] == samplebench::BuiltInEffectId::drive);
    assert (order[8] == samplebench::BuiltInEffectId::eq);
    assert (order[9] == samplebench::BuiltInEffectId::delay);
    assert (order[10] == samplebench::BuiltInEffectId::reverb);
    assert (order[11] == samplebench::BuiltInEffectId::tape);
}

void testPluginModuleCanBeRepresentedInFxChain()
{
    samplebench::BenchSettings settings;
    settings.customFxModules = true;
    settings.fxModules.push_back (samplebench::makeBuiltInFxModule (samplebench::BuiltInEffectId::gain));
    settings.fxModules.push_back (samplebench::makePluginFxModule ({ "Space Verb",
                                                                     "Acme Audio",
                                                                     "VST3",
                                                                     "Reverb",
                                                                     "/Library/Audio/Plug-Ins/VST3/Space Verb.vst3",
                                                                     "acme.spaceverb",
                                                                     123456 }));

    const auto modules = samplebench::activeFxModules (settings);

    assert (modules.size() == 2);
    assert (modules[0].kind == samplebench::FxModuleKind::builtIn);
    assert (modules[0].builtIn == samplebench::BuiltInEffectId::gain);
    assert (modules[1].kind == samplebench::FxModuleKind::plugin);
    assert (modules[1].plugin.name == "Space Verb");
    assert (modules[1].plugin.manufacturer == "Acme Audio");
    assert (modules[1].plugin.format == "VST3");
    assert (modules[1].plugin.enabled);
    assert (modules[1].plugin.status == samplebench::PluginModuleStatus::loaded);
}

void testPluginModuleStateBlobIsPreservedInBenchSettings()
{
    samplebench::BenchSettings settings;
    settings.customFxModules = true;
    auto plugin = samplebench::makePluginFxModule ({ "Color Box", "Acme Audio", "VST3", "Distortion", "/tmp/Color Box.vst3", "color.box", 42 });
    plugin.plugin.stateBlob = { 1, 2, 3, 5, 8 };
    settings.fxModules.push_back (plugin);

    auto pack = samplebench::Pack::create ("STATE_TEST");
    const auto sample = pack.importSample (samplebench::BucketId::drums, "/samples/kick.wav");

    assert (pack.updateBenchSettings (sample.id, settings));
    assert (pack.selectedSample() == std::nullopt);
    assert (pack.selectSample (sample.id));

    const auto restored = pack.selectedSample()->bench.fxModules;
    assert (restored.size() == 1);
    assert (restored[0].plugin.stateBlob == plugin.plugin.stateBlob);
    assert (restored[0].plugin.uniqueId == "color.box");
}

void testMissingPluginBecomesDisabledPlaceholder()
{
    samplebench::BenchSettings settings;
    settings.customFxModules = true;
    settings.fxModules.push_back (samplebench::makePluginFxModule ({ "Missing Verb", "Acme", "VST3", "Reverb", "/missing/Missing Verb.vst3", "missing.verb", 1 }));

    const std::vector<samplebench::CachedPluginDescription> availablePlugins;
    const auto resolved = samplebench::resolvePluginModuleAvailability (settings.fxModules, availablePlugins);

    assert (resolved.size() == 1);
    assert (resolved[0].plugin.status == samplebench::PluginModuleStatus::missing);
    assert (! resolved[0].plugin.enabled);
    assert (resolved[0].plugin.name == "Missing Verb");
}

void testBypassedPluginModuleIsSkippedByActivePluginList()
{
    samplebench::BenchSettings settings;
    settings.customFxModules = true;
    auto plugin = samplebench::makePluginFxModule ({ "Bypass Me", "Acme", "VST3", "Filter", "/tmp/Bypass Me.vst3", "bypass.me", 1 });
    plugin.plugin.enabled = false;
    settings.fxModules.push_back (plugin);

    const auto plugins = samplebench::activePluginModules (settings);

    assert (plugins.empty());
}

void testFxMenuPutsPluginsInSubmenuBeforeBuiltIns()
{
    const auto items = samplebench::fxMenuTopLevelOrder();

    assert (items.size() >= 2);
    assert (items[0] == "Plugins");
    assert (items[1] == "UTILITY");
}

void testCustomEffectChainOrderChangesProcessingOrder()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { {} };
    for (int index = 0; index < 128; ++index)
        audio.channels[0].push_back (index % 2 == 0 ? 0.8f : -0.8f);

    samplebench::BenchSettings crushThenFilter;
    crushThenFilter.gainEnabled = false;
    crushThenFilter.crushEnabled = true;
    crushThenFilter.crushBits = 8;
    crushThenFilter.crushSampleRate = 11025.0;
    crushThenFilter.filterEnabled = true;
    crushThenFilter.filterCutoffHz = 900.0f;
    crushThenFilter.customEffectChain = true;
    crushThenFilter.effectChain = { samplebench::BuiltInEffectId::crush,
                                    samplebench::BuiltInEffectId::filter };

    auto filterThenCrush = crushThenFilter;
    filterThenCrush.effectChain = { samplebench::BuiltInEffectId::filter,
                                    samplebench::BuiltInEffectId::crush };

    const auto first = samplebench::applyEffectChain (audio, crushThenFilter, samplebench::EffectProcessMode::preview);
    const auto second = samplebench::applyEffectChain (audio, filterThenCrush, samplebench::EffectProcessMode::preview);

    double difference = 0.0;
    for (std::size_t index = 0; index < first.channels[0].size(); ++index)
        difference += std::abs (first.channels[0][index] - second.channels[0][index]);

    assert (difference > 0.1);
}

void testCustomEmptyEffectChainDisablesProcessing()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { { 0.25f, -0.25f } };

    samplebench::BenchSettings settings;
    settings.customEffectChain = true;
    settings.effectChain = {};
    settings.gainEnabled = true;
    settings.gainDecibels = 12.0f;
    settings.crushEnabled = true;

    const auto processed = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);

    assert (processed.channels == audio.channels);
}

void testBounceProcessingUsesNormalizeWhenEnabled()
{
    samplebench::AudioBufferData audio;
    audio.sampleRate = 44100.0;
    audio.channels = { { 0.25f, -0.5f } };

    samplebench::BenchSettings settings;
    settings.gainEnabled = false;
    settings.normalizeEnabled = true;
    settings.normalizeTargetDecibels = -12.0f;

    const auto preview = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::preview);
    const auto bounce = samplebench::applyEffectChain (audio, settings, samplebench::EffectProcessMode::bounce);

    assert (preview.channels == audio.channels);
    assert (std::abs (bounce.channels[0][1]) < std::abs (audio.channels[0][1]));
    assert (nearlyEqualFloat (std::abs (bounce.channels[0][1]), 0.251189f));
}

void testPreviewFxDetectionIgnoresRenderOnlyNormalize()
{
    samplebench::BenchSettings settings;
    settings.normalizeEnabled = true;

    assert (! samplebench::effectChainAffectsPreview (settings));

    settings.crushEnabled = true;
    assert (samplebench::effectChainAffectsPreview (settings));
}

void testFxDetailLayoutGivesKnobsVisibleSpace()
{
    const auto layout = samplebench::calculateFxDetailLayout (samplebench::BuiltInEffectId::crush, 0, 0, 420, 112);

    assert (layout.knobLabelRow.height > 0);
    assert (layout.knobRow.height >= 64);
    assert (layout.knobRow.y < 24);
    assert (layout.knobRow.y + layout.knobRow.height <= 112);
}

void testFilterDetailLayoutStartsKnobsAtLeft()
{
    const auto layout = samplebench::calculateFxDetailLayout (samplebench::BuiltInEffectId::filter, 20, 30, 420, 112);

    assert (layout.knobA.x == 20);
    assert (layout.knobB.x > layout.knobA.x);
    assert (layout.knobA.y == layout.knobRow.y);
    assert (layout.knobB.y == layout.knobRow.y);
}

void testLoopPositionWrapsAfterFxReload()
{
    const samplebench::VisibleTimeRange loop { 2.0, 6.0 };

    assert (nearlyEqual (samplebench::wrappedLoopPosition (6.0, loop), 2.0));
    assert (nearlyEqual (samplebench::wrappedLoopPosition (7.5, loop), 3.5));
    assert (nearlyEqual (samplebench::wrappedLoopPosition (1.0, loop), 2.0));
    assert (nearlyEqual (samplebench::wrappedLoopPosition (4.0, loop), 4.0));
}
}

int main()
{
    testFilenameBuilderNormalLoop();
    testFilenameBuilderSpeedTrickLoop();
    testSuggestsShortExportNameFromCymaticsLoop();
    testSuggestsExportNameKeepsMeaningfulNumbers();
    testExtractsBpmFromFilename();
    testDetectsLoopFromFilename();
    testDetectsOneShotFromFilename();
    testMiddleTruncationPreservesBpmEnding();
    testImportedCymaticsLoopGetsUsefulDefaults();
    testImportedCymatics808LoopGetsUsefulDefaults();
    testFilenameBuilderOneShotOmitsBpm();
    testBpmIsLastForLoops();
    testCaptureSettingsConvertsBarsToSeconds();
    testWarmupIsExcludedFromExportedKeepRegion();
    testStartBarMeansKeepStart();
    testSourceLoopRegionEqualsKeepRegion();
    testBounceLoopRegionEqualsBounceDuration();
    testTailIncludedOnlyWhenTailIsEnabled();
    testBedDurationFromBpmAndBarCount();
    testLoopContinuouslyRepeatsMonoSourceToFillBed();
    testOncePerBarPlacesMonoSourceAtEachBarStart();
    testCaptureRegionCanSelectKeepFromGeneratedBed();
    testFourBucketModelStillWorks();
    testPlaybackStartsOneTargetAtATime();
    testStopPlaybackClearsTargetButKeepsLoopPreference();
    testLoopPreferenceDoesNotStartPlayback();
    testPreviewTargetDefaultsToSource();
    testPreviewTargetSelectionDoesNotStartPlayback();
    testPlaybackUsesSelectedPreviewTarget();
    testWaveformWindowShowsWholeShortFile();
    testWaveformWindowCentersCaptureForLongFile();
    testAutoFollowKeepsPlayheadVisible();
    testAutoFollowDoesNotScrollWhenPlayheadVisible();
    testWaveformTimelineIsNotShiftedByChannelLabel();
    testGainEffectChangesAmplitude();
    testMonoEffectAveragesStereoChannels();
    testCrushEffectQuantizesAndHoldsSamples();
    testCrushFullBlastMakesObviousAudioChange();
    testFilterEffectProcessesAndChangesOutput();
    testLimiterPreventsClipping();
    testCompressorReducesLevelAboveThreshold();
    testDriveChangesWaveformAndClampsSafely();
    testEqLowMidHighAffectSignal();
    testDelayProducesSyncedRepeatsFromBpm();
    testDelayWithoutBpmDoesNotInventTiming();
    testReverbProducesTail();
    testTapeChangesSignalWithoutClipping();
    testDisabledEffectsDoNotChangeAudio();
    testEffectChainOrderIsStable();
    testPluginModuleCanBeRepresentedInFxChain();
    testPluginModuleStateBlobIsPreservedInBenchSettings();
    testMissingPluginBecomesDisabledPlaceholder();
    testBypassedPluginModuleIsSkippedByActivePluginList();
    testFxMenuPutsPluginsInSubmenuBeforeBuiltIns();
    testCustomEffectChainOrderChangesProcessingOrder();
    testCustomEmptyEffectChainDisablesProcessing();
    testBounceProcessingUsesNormalizeWhenEnabled();
    testPreviewFxDetectionIgnoresRenderOnlyNormalize();
    testFxDetailLayoutGivesKnobsVisibleSpace();
    testFilterDetailLayoutStartsKnobsAtLeft();
    testLoopPositionWrapsAfterFxReload();

    std::cout << "BenchRenderModelTests passed\n";
    return 0;
}
