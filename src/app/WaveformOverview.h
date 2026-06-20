#pragma once

#include "app/BenchPalette.h"
#include "model/SampleBenchModel.h"

#include <vector>

#include <juce_gui_extra/juce_gui_extra.h>

class WaveformOverview final : public juce::Component
{
public:
    void setPeaks (std::vector<float> newPeaks);
    void setChannelPeaks (std::vector<std::vector<float>> newPeaks,
                          double durationSeconds,
                          int channels);
    void setBenchSettings (samplebench::BenchSettings newSettings);
    void setSourceDescription (juce::String description);
    void setPlaybackView (samplebench::VisibleTimeRange visibleRange,
                          double playheadSeconds,
                          bool showPlayhead,
                          samplebench::PlaybackTarget target);
    void clear();

    void paint (juce::Graphics& g) override;

private:
    std::vector<std::vector<float>> channelPeaks;
    samplebench::BenchSettings settings;
    samplebench::VisibleTimeRange visible;
    double duration = 0.0;
    double playhead = 0.0;
    bool playheadVisible = false;
    int channelCount = 0;
    samplebench::PlaybackTarget previewTarget = samplebench::PlaybackTarget::source;
    juce::String sourceDescription;
};
