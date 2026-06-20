#include "app/WaveformOverview.h"

#include <algorithm>

namespace
{
juce::Colour colour (samplebench::Rgb value)
{
    return juce::Colour::fromRGB (value.red, value.green, value.blue);
}

void drawRegion (juce::Graphics& g,
                 juce::Rectangle<float> area,
                 double startSeconds,
                 double durationSeconds,
                 samplebench::VisibleTimeRange visible,
                 juce::Colour fill,
                 const juce::String& label)
{
    const auto visibleDuration = visible.endSeconds - visible.startSeconds;
    if (durationSeconds <= 0.0 || visibleDuration <= 0.0)
        return;

    const auto endSeconds = startSeconds + durationSeconds;
    const auto clippedStart = std::max (startSeconds, visible.startSeconds);
    const auto clippedEnd = std::min (endSeconds, visible.endSeconds);
    if (clippedEnd <= clippedStart)
        return;

    const auto x = area.getX() + area.getWidth() * static_cast<float> ((clippedStart - visible.startSeconds) / visibleDuration);
    const auto width = area.getWidth() * static_cast<float> ((clippedEnd - clippedStart) / visibleDuration);
    auto region = juce::Rectangle<float> { x, area.getY(), width, area.getHeight() };

    g.setColour (fill);
    g.fillRect (region);
    g.setColour (colour (samplebench::palette::inverseText));
    g.setFont (juce::Font { juce::FontOptions { 13.0f, juce::Font::bold } });
    g.drawText (label, region.reduced (6.0f, 4.0f), juce::Justification::topLeft);
}
}

void WaveformOverview::setPeaks (std::vector<float> newPeaks)
{
    channelPeaks = { std::move (newPeaks) };
    channelCount = channelPeaks.front().empty() ? 0 : 1;
    repaint();
}

void WaveformOverview::setChannelPeaks (std::vector<std::vector<float>> newPeaks,
                                        double durationSeconds,
                                        int channels)
{
    channelPeaks = std::move (newPeaks);
    duration = std::max (0.0, durationSeconds);
    channelCount = std::max (0, channels);
    repaint();
}

void WaveformOverview::setBenchSettings (samplebench::BenchSettings newSettings)
{
    settings = std::move (newSettings);
    repaint();
}

void WaveformOverview::setSourceDescription (juce::String description)
{
    sourceDescription = std::move (description);
    repaint();
}

void WaveformOverview::setPlaybackView (samplebench::VisibleTimeRange visibleRange,
                                        double playheadSeconds,
                                        bool showPlayhead,
                                        samplebench::PlaybackTarget target)
{
    visible = visibleRange;
    playhead = std::max (0.0, playheadSeconds);
    playheadVisible = showPlayhead;
    previewTarget = target;
    repaint();
}

void WaveformOverview::clear()
{
    channelPeaks.clear();
    channelCount = 0;
    duration = 0.0;
    visible = {};
    playhead = 0.0;
    playheadVisible = false;
    sourceDescription.clear();
    repaint();
}

void WaveformOverview::paint (juce::Graphics& g)
{
    g.fillAll (colour (samplebench::palette::editor));

    auto area = getLocalBounds().toFloat().reduced (12.0f);
    g.setColour (colour (samplebench::palette::border));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);

    auto displayVisible = visible;
    if (displayVisible.endSeconds <= displayVisible.startSeconds)
        displayVisible = { 0.0, duration };
    if (displayVisible.endSeconds <= displayVisible.startSeconds)
        displayVisible = { 0.0, 1.0 };

    const auto regions = samplebench::calculateCaptureRegions (settings.musicalBpm, settings.capture);

    drawRegion (g, area, regions.warmup.startSeconds, regions.warmup.endSeconds - regions.warmup.startSeconds, displayVisible,
                colour (samplebench::palette::warmupRegion).withAlpha (0.72f),
                "WARM-UP " + juce::String (regions.warmupBars) + " BARS");
    drawRegion (g, area, regions.keep.startSeconds, regions.keep.endSeconds - regions.keep.startSeconds, displayVisible,
                colour (samplebench::palette::keepRegion).withAlpha (0.88f),
                "KEEP " + juce::String (regions.keepBars) + " BARS");
    drawRegion (g, area, regions.tail.startSeconds, regions.tail.endSeconds - regions.tail.startSeconds, displayVisible,
                colour (samplebench::palette::tailRegion).withAlpha (0.72f),
                "TAIL " + juce::String (regions.tailBars) + " BARS");

    g.setColour (colour (samplebench::palette::border).withAlpha (0.7f));
    const auto firstBar = static_cast<int> (std::floor (displayVisible.startSeconds / regions.secondsPerBar));
    const auto lastBar = static_cast<int> (std::ceil (displayVisible.endSeconds / regions.secondsPerBar));
    for (int bar = firstBar; bar <= lastBar; ++bar)
    {
        const auto barSeconds = static_cast<double> (bar) * regions.secondsPerBar;
        const auto x = area.getX() + area.getWidth() * static_cast<float> ((barSeconds - displayVisible.startSeconds)
                                                                            / (displayVisible.endSeconds - displayVisible.startSeconds));
        g.drawVerticalLine (static_cast<int> (x), area.getY(), area.getBottom());
    }

    g.setColour (colour (samplebench::palette::accentBlue));
    if (channelPeaks.empty() || channelPeaks.front().empty())
    {
        g.setColour (colour (samplebench::palette::mutedText));
        g.drawText ("Drop/import audio to draw an amplitude overview",
                    area.toNearestInt(),
                    juce::Justification::centred);
        return;
    }

    const auto laneCount = channelCount >= 2 && channelPeaks.size() >= 2 ? 2 : 1;
    const auto laneGap = laneCount == 2 ? 8.0f : 0.0f;
    const auto laneHeight = (area.getHeight() - laneGap) / static_cast<float> (laneCount);
    const auto visibleDuration = std::max (0.001, displayVisible.endSeconds - displayVisible.startSeconds);

    for (int lane = 0; lane < laneCount; ++lane)
    {
        auto laneArea = area.withY (area.getY() + static_cast<float> (lane) * (laneHeight + laneGap))
                            .withHeight (laneHeight);
        const auto laneLayout = samplebench::calculateWaveformLaneLayout (laneArea.getX(), laneArea.getWidth(), 42.0f);
        const auto timelineArea = laneArea.withX (laneLayout.timelineX).withWidth (laneLayout.timelineWidth);
        const auto labelArea = laneArea.withX (laneLayout.labelX).withWidth (laneLayout.labelWidth);
        const auto& peaks = channelPeaks[static_cast<std::size_t> (lane)];
        const auto centerY = timelineArea.getCentreY();

        g.setColour (colour (samplebench::palette::mutedText));
        g.setFont (juce::Font { juce::FontOptions { 12.0f, juce::Font::bold } });
        g.drawText (laneCount == 2 ? (lane == 0 ? "L" : "R") : "MONO",
                    labelArea.toNearestInt(),
                    juce::Justification::centredLeft);

        g.setColour (colour (samplebench::palette::accentBlue));
        const auto width = std::max (1.0f, timelineArea.getWidth());
        for (int xIndex = 0; xIndex < static_cast<int> (width); ++xIndex)
        {
            const auto seconds = displayVisible.startSeconds + (static_cast<double> (xIndex) / width) * visibleDuration;
            const auto normalized = duration > 0.0 ? seconds / duration : 0.0;
            const auto peakIndex = static_cast<std::size_t> (
                juce::jlimit (0, static_cast<int> (peaks.size()) - 1,
                              static_cast<int> (normalized * static_cast<double> (peaks.size()))));
            const auto peak = juce::jlimit (0.0f, 1.0f, peaks[peakIndex]);
            const auto x = timelineArea.getX() + static_cast<float> (xIndex);
            const auto y = peak * timelineArea.getHeight() * 0.42f;
            g.drawLine (x, centerY - y, x, centerY + y, 1.2f);
        }
    }

    if (playheadVisible && playhead >= displayVisible.startSeconds && playhead <= displayVisible.endSeconds)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float> ((playhead - displayVisible.startSeconds) / visibleDuration);
        g.setColour (colour (samplebench::palette::inverseText));
        g.drawLine (x, area.getY(), x, area.getBottom(), 2.0f);
        g.setColour (colour (samplebench::palette::accentPink));
        g.fillEllipse (x - 4.0f, area.getY() - 2.0f, 8.0f, 8.0f);
        g.setColour (colour (samplebench::palette::mutedText));
        g.setFont (juce::Font { juce::FontOptions { 12.0f } });
        g.drawText (previewTarget == samplebench::PlaybackTarget::bounce ? "Previewing: Bounce" : "Previewing: Source",
                    area.toNearestInt().removeFromBottom (22),
                    juce::Justification::bottomRight);
    }

    if (sourceDescription.isNotEmpty())
    {
        g.setColour (colour (samplebench::palette::text).withAlpha (0.82f));
        g.setFont (juce::Font { juce::FontOptions { 12.0f, juce::Font::bold } });
        g.drawText (sourceDescription, area.toNearestInt().reduced (6, 4), juce::Justification::topRight);
    }
}
