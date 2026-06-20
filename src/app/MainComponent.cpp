#include "MainComponent.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr int margin = 20;
constexpr int gap = 12;
constexpr const char* sampleDragPrefix = "samplebench-sample:";

juce::Colour colour (samplebench::Rgb value)
{
    return juce::Colour::fromRGB (value.red, value.green, value.blue);
}

void addLabel (juce::Component& parent, juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, colour (samplebench::palette::text));
    parent.addAndMakeVisible (label);
}

void styleTransportLabel (juce::Label& label)
{
    label.setColour (juce::Label::textColourId, colour (samplebench::palette::text));
    label.setColour (juce::Label::backgroundColourId, colour (samplebench::palette::editor));
    label.setJustificationType (juce::Justification::centredLeft);
}

void configureEditor (juce::Component& parent, juce::TextEditor& editor, const juce::String& tooltip)
{
    editor.setTooltip (tooltip);
    editor.setSelectAllWhenFocused (true);
    editor.setColour (juce::TextEditor::textColourId, colour (samplebench::palette::text));
    editor.setColour (juce::TextEditor::backgroundColourId, colour (samplebench::palette::editor));
    editor.setColour (juce::TextEditor::outlineColourId, colour (samplebench::palette::border));
    editor.setColour (juce::TextEditor::focusedOutlineColourId, colour (samplebench::palette::accent));
    editor.setColour (juce::TextEditor::highlightColourId, colour (samplebench::palette::accentBlue).withAlpha (0.6f));
    parent.addAndMakeVisible (editor);
}

void configureButton (juce::TextButton& button, bool dark = false)
{
    button.setColour (juce::TextButton::buttonColourId,
                      dark ? colour (samplebench::palette::darkButton) : colour (samplebench::palette::button));
    button.setColour (juce::TextButton::textColourOffId,
                      dark ? colour (samplebench::palette::inverseText) : colour (samplebench::palette::text));
    button.setColour (juce::TextButton::textColourOnId,
                      dark ? colour (samplebench::palette::inverseText) : colour (samplebench::palette::text));
}

juce::String sampleDragDescription (std::size_t sampleId)
{
    return juce::String { sampleDragPrefix } + juce::String (static_cast<juce::int64> (sampleId));
}

std::optional<std::size_t> sampleIdFromDragDescription (const juce::var& description)
{
    if (! description.isString())
        return std::nullopt;

    const auto text = description.toString();
    if (! text.startsWith (sampleDragPrefix))
        return std::nullopt;

    const auto id = text.fromFirstOccurrenceOf (sampleDragPrefix, false, false).getLargeIntValue();
    if (id <= 0)
        return std::nullopt;

    return static_cast<std::size_t> (id);
}

void configureChip (juce::TextButton& button, bool active)
{
    button.setColour (juce::TextButton::buttonColourId,
                      active ? colour (samplebench::palette::accent)
                             : colour (samplebench::palette::darkButton));
    button.setColour (juce::TextButton::textColourOffId,
                      active ? colour (samplebench::palette::inverseText)
                             : colour (samplebench::palette::text));
    button.setColour (juce::TextButton::textColourOnId, colour (samplebench::palette::inverseText));
}

int intFromEditor (const juce::TextEditor& editor, int fallback)
{
    const auto value = editor.getText().getIntValue();
    return value == 0 && editor.getText().trim() != "0" ? fallback : value;
}

double doubleFromEditor (const juce::TextEditor& editor, double fallback)
{
    const auto text = editor.getText().trim();
    if (text.isEmpty())
        return fallback;

    return text.getDoubleValue();
}

juce::String effectLabel (samplebench::BuiltInEffectId effect);

juce::String pluginName (const samplebench::HostedPluginModule& plugin)
{
    return plugin.name.empty() ? juce::File { juce::String::fromUTF8 (plugin.filePath.u8string().c_str()) }.getFileNameWithoutExtension()
                               : juce::String::fromUTF8 (plugin.name.c_str());
}

juce::String moduleLabel (const samplebench::FxModule& module)
{
    return module.kind == samplebench::FxModuleKind::builtIn ? effectLabel (module.builtIn)
                                                             : pluginName (module.plugin);
}

bool isPluginAvailableForMenu (const samplebench::CachedPluginDescription& plugin)
{
    if (plugin.format != "VST3" || plugin.filePath.empty())
        return false;

    const auto category = juce::String::fromUTF8 (plugin.category.c_str()).toLowerCase();
    return ! category.contains ("instrument");
}

juce::PluginDescription toPluginDescription (const samplebench::HostedPluginModule& plugin)
{
    juce::PluginDescription description;
    description.name = juce::String::fromUTF8 (plugin.name.c_str());
    description.descriptiveName = description.name;
    description.manufacturerName = juce::String::fromUTF8 (plugin.manufacturer.c_str());
    description.pluginFormatName = plugin.format.empty() ? "VST3" : juce::String::fromUTF8 (plugin.format.c_str());
    description.category = juce::String::fromUTF8 (plugin.category.c_str());
    description.fileOrIdentifier = juce::String::fromUTF8 (plugin.filePath.u8string().c_str());
    description.uniqueId = juce::String::fromUTF8 (plugin.uniqueId.c_str()).getIntValue();
    description.deprecatedUid = description.uniqueId;
    description.numInputChannels = 2;
    description.numOutputChannels = 2;
    description.lastFileModTime = juce::Time (plugin.modifiedTimeMillis);
    description.lastInfoUpdateTime = juce::Time::getCurrentTime();
    return description;
}

bool moduleIsPowered (const samplebench::FxModule& module,
                      bool gainEnabled,
                      bool monoOn,
                      bool normalizeOn,
                      bool limitEnabled,
                      bool compressorEnabled,
                      bool crushEnabled,
                      bool filterEnabled,
                      bool driveEnabled,
                      bool eqEnabled,
                      bool delayEnabled,
                      bool reverbEnabled,
                      bool tapeEnabled)
{
    if (module.kind == samplebench::FxModuleKind::plugin)
        return module.plugin.enabled && module.plugin.status == samplebench::PluginModuleStatus::loaded;

    if (module.builtIn == samplebench::BuiltInEffectId::gain)
        return gainEnabled;
    if (module.builtIn == samplebench::BuiltInEffectId::mono)
        return monoOn;
    if (module.builtIn == samplebench::BuiltInEffectId::normalize)
        return normalizeOn;
    if (module.builtIn == samplebench::BuiltInEffectId::limit)
        return limitEnabled;
    if (module.builtIn == samplebench::BuiltInEffectId::compressor)
        return compressorEnabled;
    if (module.builtIn == samplebench::BuiltInEffectId::crush)
        return crushEnabled;
    if (module.builtIn == samplebench::BuiltInEffectId::filter)
        return filterEnabled;
    if (module.builtIn == samplebench::BuiltInEffectId::drive)
        return driveEnabled;
    if (module.builtIn == samplebench::BuiltInEffectId::eq)
        return eqEnabled;
    if (module.builtIn == samplebench::BuiltInEffectId::delay)
        return delayEnabled;
    if (module.builtIn == samplebench::BuiltInEffectId::reverb)
        return reverbEnabled;
    return tapeEnabled;
}

class PluginEditorWindow final : public juce::DocumentWindow
{
public:
    PluginEditorWindow (juce::String title,
                        std::unique_ptr<juce::AudioPluginInstance> instanceToOwn,
                        int slotIndex,
                        std::function<void (int, const juce::MemoryBlock&)> onStateChanged)
        : juce::DocumentWindow { std::move (title),
                                 colour (samplebench::palette::panel),
                                 juce::DocumentWindow::closeButton },
          instance { std::move (instanceToOwn) },
          slot { slotIndex },
          stateCallback { std::move (onStateChanged) }
    {
        setUsingNativeTitleBar (true);
        if (auto* editor = instance != nullptr ? instance->createEditorIfNeeded() : nullptr)
        {
            setContentOwned (editor, true);
            centreWithSize (std::max (360, editor->getWidth()), std::max (260, editor->getHeight()));
        }
        else
        {
            auto* label = new juce::Label {};
            label->setText ("This plugin did not provide an editor.", juce::dontSendNotification);
            label->setJustificationType (juce::Justification::centred);
            label->setColour (juce::Label::textColourId, colour (samplebench::palette::text));
            setContentOwned (label, true);
            centreWithSize (360, 160);
        }
    }

    ~PluginEditorWindow() override
    {
        releasePluginEditor();
    }

    void closeButtonPressed() override
    {
        releasePluginEditor();
        setVisible (false);
    }

private:
    void releasePluginEditor()
    {
        storeState();
        clearContentComponent();
        instance.reset();
    }

    void storeState()
    {
        if (stored || instance == nullptr || stateCallback == nullptr)
            return;

        juce::MemoryBlock state;
        instance->getStateInformation (state);
        stateCallback (slot, state);
        stored = true;
    }

    std::unique_ptr<juce::AudioPluginInstance> instance;
    int slot = -1;
    std::function<void (int, const juce::MemoryBlock&)> stateCallback;
    bool stored = false;
};

class BenchPluginPlayHead final : public juce::AudioPlayHead
{
public:
    BenchPluginPlayHead (double bpmToUse, double sampleRateToUse, bool loopingToUse, juce::int64 totalSamplesToUse)
        : bpm { bpmToUse > 0.0 ? bpmToUse : 120.0 },
          sampleRate { sampleRateToUse > 0.0 ? sampleRateToUse : 44100.0 },
          looping { loopingToUse },
          totalSamples { totalSamplesToUse }
    {
    }

    void setTimeInSamples (juce::int64 nextPosition) noexcept
    {
        positionSamples = std::max<juce::int64> (0, nextPosition);
    }

    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        juce::AudioPlayHead::PositionInfo info;
        const auto seconds = static_cast<double> (positionSamples) / sampleRate;
        const auto ppq = seconds * bpm / 60.0;
        info.setTimeInSamples (positionSamples);
        info.setTimeInSeconds (seconds);
        info.setBpm (bpm);
        info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        info.setPpqPosition (ppq);
        info.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
        info.setIsPlaying (true);
        info.setIsRecording (false);
        info.setIsLooping (looping);
        if (looping && totalSamples > 0)
        {
            const auto endSeconds = static_cast<double> (totalSamples) / sampleRate;
            info.setLoopPoints (juce::AudioPlayHead::LoopPoints { 0.0, endSeconds * bpm / 60.0 });
        }
        return info;
    }

private:
    double bpm = 120.0;
    double sampleRate = 44100.0;
    bool looping = false;
    juce::int64 totalSamples = 0;
    juce::int64 positionSamples = 0;
};

juce::String previewFxKey (const juce::File& inputFile, const samplebench::BenchSettings& settings)
{
    juce::String chainKey;
    for (const auto& module : samplebench::activeFxModules (settings))
    {
        chainKey += moduleLabel (module) + ",";
        if (module.kind == samplebench::FxModuleKind::plugin)
            chainKey += juce::String (static_cast<int> (module.plugin.enabled)) + ":"
                     + juce::String::fromUTF8 (module.plugin.uniqueId.c_str()) + ":"
                     + juce::String (static_cast<int> (module.plugin.stateBlob.size())) + ",";
    }

    return inputFile.getFullPathName()
         + "|" + juce::String (inputFile.getLastModificationTime().toMilliseconds())
         + "|" + juce::String (settings.customEffectChain ? 1 : 0)
         + "|" + chainKey
         + "|" + juce::String (settings.gainEnabled ? 1 : 0)
         + "|" + juce::String (settings.gainDecibels, 3)
         + "|" + juce::String (settings.monoEnabled ? 1 : 0)
         + "|" + juce::String (settings.limitEnabled ? 1 : 0)
         + "|" + juce::String (settings.limitCeilingDecibels, 3)
         + "|" + juce::String (settings.limitInputDecibels, 3)
         + "|" + juce::String (settings.limitReleaseMs, 3)
         + "|" + juce::String (settings.compressorEnabled ? 1 : 0)
         + "|" + juce::String (settings.compressorThresholdDecibels, 3)
         + "|" + juce::String (settings.compressorRatio, 3)
         + "|" + juce::String (settings.compressorAttackMs, 3)
         + "|" + juce::String (settings.compressorReleaseMs, 3)
         + "|" + juce::String (settings.compressorMakeupDecibels, 3)
         + "|" + juce::String (settings.compressorMix, 3)
         + "|" + juce::String (settings.crushEnabled ? 1 : 0)
         + "|" + juce::String (settings.crushBits)
         + "|" + juce::String (settings.crushSampleRate, 2)
         + "|" + juce::String (settings.crushMix, 3)
         + "|" + juce::String (settings.crushOutputDecibels, 3)
         + "|" + juce::String (settings.filterEnabled ? 1 : 0)
         + "|" + juce::String (settings.filterMode == samplebench::FilterMode::highPass ? "HP" : "LP")
         + "|" + juce::String (settings.filterCutoffHz, 2)
         + "|" + juce::String (settings.filterResonance, 3)
         + "|" + juce::String (settings.driveEnabled ? 1 : 0)
         + "|" + juce::String (settings.driveAmount, 3)
         + "|" + juce::String (settings.driveTone, 3)
         + "|" + juce::String (settings.driveMix, 3)
         + "|" + juce::String (settings.driveOutputDecibels, 3)
         + "|" + juce::String (settings.eqEnabled ? 1 : 0)
         + "|" + juce::String (settings.eqLowDecibels, 3)
         + "|" + juce::String (settings.eqMidDecibels, 3)
         + "|" + juce::String (settings.eqHighDecibels, 3)
         + "|" + juce::String (settings.delayEnabled ? 1 : 0)
         + "|" + juce::String (settings.delayDivision)
         + "|" + juce::String (settings.delayFeedback, 3)
         + "|" + juce::String (settings.delayMix, 3)
         + "|" + juce::String (settings.delayTone, 3)
         + "|" + juce::String (settings.reverbEnabled ? 1 : 0)
         + "|" + juce::String (settings.reverbSize, 3)
         + "|" + juce::String (settings.reverbDecaySeconds, 3)
         + "|" + juce::String (settings.reverbMix, 3)
         + "|" + juce::String (settings.reverbTone, 3)
         + "|" + juce::String (settings.tapeEnabled ? 1 : 0)
         + "|" + juce::String (settings.tapeDrive, 3)
         + "|" + juce::String (settings.tapeWobble, 3)
         + "|" + juce::String (settings.tapeTone, 3)
         + "|" + juce::String (settings.tapeNoise, 3)
         + "|" + juce::String (settings.tapeMix, 3);
}

juce::String formatSignedDb (float decibels)
{
    return (decibels >= 0.0f ? "+" : "") + juce::String (decibels, 1) + " dB";
}

juce::String formatTime (double seconds)
{
    const auto safeSeconds = std::max (0.0, seconds);
    const auto totalMilliseconds = static_cast<int> (std::llround (safeSeconds * 1000.0));
    const auto minutes = totalMilliseconds / 60000;
    const auto secondsPart = (totalMilliseconds / 1000) % 60;
    const auto milliseconds = totalMilliseconds % 1000;

    return juce::String (minutes) + ":"
         + juce::String (secondsPart).paddedLeft ('0', 2) + "."
         + juce::String (milliseconds).paddedLeft ('0', 3);
}

juce::String formatDurationMetadata (int channels, double sampleRate, double durationSeconds, juce::int64 fileSizeBytes)
{
    const auto channelText = channels == 1 ? juce::String { "Mono" }
                           : channels == 2 ? juce::String { "Stereo" }
                           : juce::String (channels) + " ch";
    const auto sampleRateText = sampleRate > 0.0
        ? juce::String (sampleRate / 1000.0, 1) + " kHz"
        : juce::String { "-- kHz" };
    const auto sizeText = fileSizeBytes > 0
        ? juce::String (static_cast<double> (fileSizeBytes) / 1024.0, 0) + " KB"
        : juce::String {};

    auto text = channelText + "  " + sampleRateText + "  " + formatTime (durationSeconds);
    if (sizeText.isNotEmpty())
        text += "  " + sizeText;
    return text;
}

juce::String effectLabel (samplebench::BuiltInEffectId effect)
{
    if (effect == samplebench::BuiltInEffectId::gain)
        return "GAIN";
    if (effect == samplebench::BuiltInEffectId::mono)
        return "MONO";
    if (effect == samplebench::BuiltInEffectId::normalize)
        return "NORM";
    if (effect == samplebench::BuiltInEffectId::limit)
        return "LIMIT";
    if (effect == samplebench::BuiltInEffectId::compressor)
        return "COMP";
    if (effect == samplebench::BuiltInEffectId::crush)
        return "CRUSH";
    if (effect == samplebench::BuiltInEffectId::drive)
        return "DRIVE";
    if (effect == samplebench::BuiltInEffectId::eq)
        return "EQ";
    if (effect == samplebench::BuiltInEffectId::delay)
        return "DELAY";
    if (effect == samplebench::BuiltInEffectId::reverb)
        return "REVERB";
    if (effect == samplebench::BuiltInEffectId::tape)
        return "TAPE";
    return "FILTER";
}

bool isNewFxDetailEffect (samplebench::BuiltInEffectId effect)
{
    return effect == samplebench::BuiltInEffectId::limit
        || effect == samplebench::BuiltInEffectId::compressor
        || effect == samplebench::BuiltInEffectId::drive
        || effect == samplebench::BuiltInEffectId::eq
        || effect == samplebench::BuiltInEffectId::delay
        || effect == samplebench::BuiltInEffectId::reverb
        || effect == samplebench::BuiltInEffectId::tape;
}
}

RotaryKnob::RotaryKnob()
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 20);
    setMouseDragSensitivity (180);
    setSliderSnapsToMousePosition (false);
    setVelocityBasedMode (false);
    setScrollWheelEnabled (true);
    setColour (juce::Slider::rotarySliderFillColourId, colour (samplebench::palette::accentBlue));
    setColour (juce::Slider::rotarySliderOutlineColourId, colour (samplebench::palette::border));
    setColour (juce::Slider::thumbColourId, colour (samplebench::palette::text));
    setColour (juce::Slider::textBoxTextColourId, colour (samplebench::palette::text));
    setColour (juce::Slider::textBoxBackgroundColourId, colour (samplebench::palette::editor));
    setColour (juce::Slider::textBoxOutlineColourId, colour (samplebench::palette::border));
}

void RotaryKnob::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        juce::Slider::mouseDown (event);
        return;
    }

    directDragActive = true;
    dragStartY = event.position.y;
    dragStartProportion = valueToProportionOfLength (getValue());
    if (onDragStart != nullptr)
        onDragStart();
}

void RotaryKnob::mouseDrag (const juce::MouseEvent& event)
{
    if (! directDragActive)
    {
        juce::Slider::mouseDrag (event);
        return;
    }

    const auto sensitivity = event.mods.isShiftDown() ? 420.0 : 160.0;
    const auto delta = static_cast<double> (dragStartY - event.position.y) / sensitivity;
    const auto nextProportion = std::clamp (dragStartProportion + delta, 0.0, 1.0);
    setValue (proportionOfLengthToValue (nextProportion), juce::sendNotificationSync);
}

void RotaryKnob::mouseUp (const juce::MouseEvent& event)
{
    if (! directDragActive)
    {
        juce::Slider::mouseUp (event);
        return;
    }

    directDragActive = false;
    if (onDragEnd != nullptr)
        onDragEnd();
}

void RotaryKnob::mouseDoubleClick (const juce::MouseEvent& event)
{
    juce::Slider::mouseDoubleClick (event);
}

FxModuleCard::FxModuleCard (juce::String moduleName)
    : juce::Button (moduleName),
      name (std::move (moduleName))
{
    setClickingTogglesState (false);
}

void FxModuleCard::setModuleName (juce::String moduleName)
{
    name = std::move (moduleName);
    repaint();
}

void FxModuleCard::setCardState (bool powerOn, bool isSelected, juce::String newSummary)
{
    powered = powerOn;
    selected = isSelected;
    summary = std::move (newSummary);
    repaint();
}

void FxModuleCard::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto area = getLocalBounds().toFloat().reduced (1.0f);
    const auto fill = selected ? colour (samplebench::palette::accent).withAlpha (0.92f)
                    : highlighted || down ? colour (samplebench::palette::button)
                    : colour (samplebench::palette::rack);
    const auto stroke = powered ? colour (samplebench::palette::accentBlue)
                                : colour (samplebench::palette::border);

    g.setColour (fill);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (stroke);
    g.drawRoundedRectangle (area, 5.0f, selected ? 2.0f : 1.0f);

    auto inner = getLocalBounds().reduced (8, 7);
    g.setColour (selected ? colour (samplebench::palette::inverseText)
                          : colour (samplebench::palette::text));
    g.setFont (juce::Font { juce::FontOptions { 12.0f, juce::Font::bold } });
    g.drawText (name, inner.removeFromTop (18), juce::Justification::centredLeft);

    g.setFont (juce::Font { juce::FontOptions { 10.5f } });
    g.drawText (powered ? "ON" : "OFF", inner.removeFromTop (16), juce::Justification::centredLeft);
    g.setColour (selected ? colour (samplebench::palette::inverseText).withAlpha (0.9f)
                          : colour (samplebench::palette::mutedText));
    g.drawText (summary, inner, juce::Justification::centredLeft);
}

void LevelMeterComponent::setLevels (float leftPeak, float rightPeak, bool stereo, bool clipping)
{
    left = juce::jlimit (0.0f, 1.0f, leftPeak);
    right = juce::jlimit (0.0f, 1.0f, rightPeak);
    isStereo = stereo;
    isClipping = clipping;
    repaint();
}

void LevelMeterComponent::paint (juce::Graphics& g)
{
    g.fillAll (colour (samplebench::palette::editor));
    auto area = getLocalBounds().reduced (6);

    g.setColour (colour (samplebench::palette::border));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);

    g.setColour (colour (samplebench::palette::mutedText));
    g.setFont (juce::Font { juce::FontOptions { 10.0f, juce::Font::bold } });
    g.drawText ("PEAK", area.removeFromTop (16), juce::Justification::centred);

    auto clip = area.removeFromTop (16).reduced (2, 2);
    g.setColour (isClipping ? colour (samplebench::palette::accentPink)
                            : colour (samplebench::palette::darkButton));
    g.fillRoundedRectangle (clip.toFloat(), 3.0f);
    g.setColour (colour (samplebench::palette::mutedText));
    g.drawText ("CLIP", clip, juce::Justification::centred);

    area.removeFromTop (4);
    auto meters = area;
    const auto meterGap = isStereo ? 4 : 0;
    const auto meterWidth = isStereo ? (meters.getWidth() - meterGap) / 2 : meters.getWidth();

    auto drawMeter = [&] (juce::Rectangle<int> meterArea, float peak, const juce::String& label)
    {
        g.setColour (colour (samplebench::palette::darkButton));
        g.fillRect (meterArea);
        const auto fillHeight = static_cast<int> (static_cast<float> (meterArea.getHeight()) * juce::jlimit (0.0f, 1.0f, peak));
        auto fill = meterArea.removeFromBottom (fillHeight);
        g.setColour (peak > 0.95f ? colour (samplebench::palette::accentPink)
                                  : colour (samplebench::palette::accentBlue));
        g.fillRect (fill);
        g.setColour (colour (samplebench::palette::border));
        g.drawRect (meterArea.expanded (0, fillHeight));
        g.setColour (colour (samplebench::palette::text));
        g.drawText (label, meterArea.removeFromBottom (14), juce::Justification::centred);
    };

    auto leftArea = meters.removeFromLeft (meterWidth);
    if (isStereo)
    {
        meters.removeFromLeft (meterGap);
        drawMeter (leftArea, left, "L");
        drawMeter (meters, right, "R");
    }
    else
    {
        drawMeter (leftArea, left, "M");
    }
}

bool BucketDropButton::isInterestedInDragSource (const SourceDetails& dragSourceDetails)
{
    return sampleIdFromDragDescription (dragSourceDetails.description).has_value();
}

void BucketDropButton::itemDragEnter (const SourceDetails& dragSourceDetails)
{
    if (! isInterestedInDragSource (dragSourceDetails))
        return;

    dropHighlighted = true;
    repaint();
}

void BucketDropButton::itemDragExit (const SourceDetails&)
{
    dropHighlighted = false;
    repaint();
}

void BucketDropButton::itemDropped (const SourceDetails& dragSourceDetails)
{
    dropHighlighted = false;
    repaint();

    if (auto sampleId = sampleIdFromDragDescription (dragSourceDetails.description))
        if (onSampleDropped != nullptr)
            onSampleDropped (*sampleId, bucket);
}

void BucketDropButton::paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::TextButton::paintButton (g, shouldDrawButtonAsHighlighted || dropHighlighted, shouldDrawButtonAsDown);

    if (! dropHighlighted)
        return;

    g.setColour (colour (samplebench::palette::accentBlue));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 2.0f);
}

void BenchContentComponent::paint (juce::Graphics& g)
{
    auto bench = getLocalBounds().reduced (22, 18);
    const auto panelColour = colour (samplebench::palette::rack);
    const auto outline = colour (samplebench::palette::border);

    auto drawSection = [&] (juce::Rectangle<int> section)
    {
        g.setColour (panelColour);
        g.fillRoundedRectangle (section.toFloat(), 6.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (section.toFloat().reduced (0.5f), 6.0f, 1.0f);
    };

    if (! sampleWorkbenchVisible)
    {
        drawSection (bench);
        return;
    }

    drawSection (bench.removeFromTop (104));
    bench.removeFromTop (gap);
    drawSection (bench.removeFromTop (406));
    bench.removeFromTop (gap);
    auto bottom = bench.removeFromTop (std::max (348, bench.getHeight() - 26));
    auto fx = bottom.removeFromLeft (bottom.getWidth() / 2 - gap / 2);
    bottom.removeFromLeft (gap);
    drawSection (fx);
    drawSection (bottom);
}

void BenchContentComponent::setSampleWorkbenchVisible (bool visible)
{
    if (sampleWorkbenchVisible == visible)
        return;

    sampleWorkbenchVisible = visible;
    repaint();
}

MainComponent::MainComponent()
    : pack (samplebench::Pack::create ("UNTITLED_PACK"))
{
    setSize (1280, 860);

    audioFormatManager.registerBasicFormats();
    mixerSource.addInputSource (&sourceTransport, false);
    mixerSource.addInputSource (&renderTransport, false);
    audioSourcePlayer.setSource (&mixerSource);
    audioDeviceManager.initialiseWithDefaultDevices (0, 2);
    audioDeviceManager.addAudioCallback (&audioSourcePlayer);
    pluginRegistryCache = samplebench::PluginRegistry::loadFromDisk (pluginRegistryCacheFile().getFullPathName().toStdString());

    topAppLabel.setFont (juce::Font { juce::FontOptions { 17.0f, juce::Font::bold } });
    addLabel (*this, topAppLabel, "BENCH SAMPLER");
    addLabel (*this, topPackLabel, "PACK  UNTITLED_PACK");
    for (auto* label : { &topAppLabel, &topPackLabel, &transportStatusLabel })
        styleTransportLabel (*label);

    benchViewport.setViewedComponent (&benchContent, false);
    benchViewport.setScrollBarsShown (true, false, true, false);
    benchViewport.setScrollBarThickness (14);
    addAndMakeVisible (benchViewport);

    settingsButton.onClick = [this] { openPluginSettings(); };
    configureButton (settingsButton);
    addAndMakeVisible (settingsButton);
    exportPackButton.onClick = [this] { requestExportPack(); };
    exportPackButton.setTooltip ("Export kept bounces into a sampler-ready pack folder.");
    exportPackButton.setEnabled (true);
    configureButton (exportPackButton, true);
    addAndMakeVisible (exportPackButton);

    titleLabel.setFont (juce::Font { juce::FontOptions { 18.0f, juce::Font::bold } });
    addLabel (*this, titleLabel, "Pack:");
    configureEditor (*this, packNameEditor, "Pack name");
    packNameEditor.onReturnKey = [this] { commitPackNameEditor(); };
    packNameEditor.onFocusLost = [this] { commitPackNameEditor(); };

    addLabel (*this, subtitleLabel, "Local pack session");
    subtitleLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::mutedText));

    newPackButton.onClick = [this] { requestNewPack(); };
    newPackButton.setTooltip ("Start a new pack. If this pack has imported samples, you will be asked before discarding it.");
    configureButton (newPackButton);
    addAndMakeVisible (newPackButton);

    openPackButton.onClick = [this] { requestOpenPack(); };
    openPackButton.setTooltip ("Open a saved Bench Sampler pack session.");
    configureButton (openPackButton);
    addAndMakeVisible (openPackButton);

    savePackButton.onClick = [this] { savePack(); };
    savePackButton.setTooltip ("Save this Bench Sampler pack session.");
    configureButton (savePackButton);
    addAndMakeVisible (savePackButton);

    importButton.onClick = [this] { chooseFilesToImport(); };
    configureButton (importButton, true);
    addAndMakeVisible (importButton);

    configureBucketButtons();
    sampleList.setRowHeight (34);
    sampleList.setMultipleSelectionEnabled (false);
    sampleList.setColour (juce::ListBox::backgroundColourId, colour (samplebench::palette::editor));
    sampleList.setColour (juce::ListBox::outlineColourId, colour (samplebench::palette::border));
    addAndMakeVisible (sampleList);

    benchTitleLabel.setFont (juce::Font { juce::FontOptions { 20.0f, juce::Font::bold } });
    addLabel (*this, benchTitleLabel, "SOUND");
    addLabel (*this, sourceNameLabel, "Source: -");
    addLabel (*this, sourceMetadataLabel, "");
    sourceMetadataLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::mutedText));
    addLabel (*this, sourceBucketLabel, "Bucket: -");
    sourceBucketLabel.setJustificationType (juce::Justification::centred);
    sourceBucketLabel.setColour (juce::Label::backgroundColourId, colour (samplebench::palette::accent));
    sourceBucketLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::inverseText));
    addLabel (*this, typeLabel, "Type");
    addLabel (*this, bpmLabel, "Musical BPM");
    addLabel (*this, barsFieldLabel, "Bars");
    addLabel (*this, keyFieldLabel, "Key");

    typeSelector.addItem ("One-shot", 1);
    typeSelector.addItem ("Loop", 2);
    typeSelector.addItem ("Texture", 3);
    typeSelector.onChange = [this] { refreshFilenamePreview(); };
    addAndMakeVisible (typeSelector);

    configureEditor (*this, musicalBpmEditor, "Musical BPM");
    musicalBpmEditor.setTextToShowWhenEmpty ("BPM", colour (samplebench::palette::mutedText));
    configureEditor (*this, barsEditor, "Bars");
    barsEditor.setTextToShowWhenEmpty ("Bars", colour (samplebench::palette::mutedText));
    configureEditor (*this, keyEditor, "Key");
    keyEditor.setTextToShowWhenEmpty ("Key", colour (samplebench::palette::mutedText));
    for (auto* editor : { &musicalBpmEditor, &barsEditor, &keyEditor })
        editor->onTextChange = [this] { refreshFilenamePreview(); };

    playSourceButton.onClick = [this] { playSource(); };
    configureButton (playSourceButton);
    addAndMakeVisible (playSourceButton);
    playSourceButton.setVisible (false);

    addAndMakeVisible (waveformOverview);
    addAndMakeVisible (levelMeter);

    captureTitleLabel.setFont (juce::Font { juce::FontOptions { 18.0f, juce::Font::bold } });
    addLabel (*this, captureTitleLabel, "CAPTURE");
    returnToStartButton.onClick = [this] { returnPreviewToStart(); };
    playPreviewButton.onClick = [this] { playSelectedPreview(); };
    stopPreviewButton.onClick = [this] { stopPlayback(); };
    loopPreviewToggle.onClick = [this]
    {
        playbackState = samplebench::setLoopEnabled (playbackState, loopPreviewToggle.getToggleState());
        sourceTransport.setLooping (playbackState.loopEnabled
                                    && playbackState.target == samplebench::PlaybackTarget::source
                                    && sourcePreviewScope == SourcePreviewScope::full);
        renderTransport.setLooping (playbackState.loopEnabled && playbackState.target == samplebench::PlaybackTarget::bounce);
        refreshTransportControls();
    };
    sourceTargetButton.onClick = [this] { selectPreviewTarget (samplebench::PlaybackTarget::source); };
    bounceTargetButton.onClick = [this] { selectPreviewTarget (samplebench::PlaybackTarget::bounce); };
    sourceBedAsIsButton.onClick = [this] { selectSourceBedMode (samplebench::SourceBedMode::asIs); };
    sourceBedExtendButton.onClick = [this] { selectSourceBedMode (samplebench::SourceBedMode::extendForFx); };
    bedTriggerSelector.addItem ("Loop", 1);
    bedTriggerSelector.addItem ("Once per bar", 2);
    bedTriggerSelector.onChange = [this] { refreshFilenamePreview(); };
    sourceKeepScopeButton.onClick = [this] { selectSourcePreviewScope (SourcePreviewScope::keep); };
    sourceFullScopeButton.onClick = [this] { selectSourcePreviewScope (SourcePreviewScope::full); };
    for (auto* button : { &returnToStartButton, &playPreviewButton, &stopPreviewButton,
                          &sourceTargetButton, &bounceTargetButton,
                          &sourceBedAsIsButton, &sourceBedExtendButton,
                          &sourceKeepScopeButton, &sourceFullScopeButton })
    {
        configureButton (*button);
        addAndMakeVisible (*button);
    }
    addAndMakeVisible (bedTriggerSelector);
    loopPreviewToggle.setColour (juce::ToggleButton::textColourId, colour (samplebench::palette::text));
    addAndMakeVisible (loopPreviewToggle);
    addLabel (*this, previewTimeLabel, "0:00.000");
    previewTimeLabel.setFont (juce::Font { juce::FontOptions { "Menlo", 15.0f, juce::Font::plain } });
    previewTimeLabel.setColour (juce::Label::backgroundColourId, colour (samplebench::palette::editor));
    previewTimeLabel.setColour (juce::Label::outlineColourId, colour (samplebench::palette::border));
    addLabel (*this, previewTargetLabel, "Target");
    addLabel (*this, sourceBedLabel, "Source Bed");
    addLabel (*this, bedTriggerLabel, "Trigger");
    addLabel (*this, bedLengthLabel, "Bed Length Bars");
    configureEditor (*this, bedLengthBarsEditor, "Bed length bars");
    bedLengthBarsEditor.onTextChange = [this] { refreshFilenamePreview(); };
    bedLengthMinusButton.onClick = [this] { nudgeBedLengthBars (-1); };
    bedLengthPlusButton.onClick = [this] { nudgeBedLengthBars (1); };
    for (auto* button : { &bedLengthMinusButton, &bedLengthPlusButton })
    {
        configureButton (*button);
        addAndMakeVisible (*button);
    }
    addLabel (*this, previewScopeLabel, "Scope");
    addLabel (*this, captureStartLabel, "Start Bar");
    addLabel (*this, warmupLabel, "Warm-up Bars");
    addLabel (*this, keepLabel, "Keep Bars");
    addLabel (*this, tailLabel, "Tail Bars");
    addLabel (*this, regionSummaryLabel, "Render path: -");
    regionSummaryLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::mutedText));
    configureEditor (*this, captureStartBarEditor, "Capture start bar");
    startMinusButton.onClick = [this] { nudgeStartBar (-1); };
    startPlusButton.onClick = [this] { nudgeStartBar (1); };
    warmupMinusButton.onClick = [this] { nudgeWarmupBars (-1); };
    warmupPlusButton.onClick = [this] { nudgeWarmupBars (1); };
    keepMinusButton.onClick = [this] { nudgeKeepBars (-1); };
    keepPlusButton.onClick = [this] { nudgeKeepBars (1); };
    tailMinusButton.onClick = [this] { nudgeTailBars (-1); };
    tailPlusButton.onClick = [this] { nudgeTailBars (1); };
    for (auto* button : { &startMinusButton, &startPlusButton,
                          &warmupMinusButton, &warmupPlusButton,
                          &keepMinusButton, &keepPlusButton,
                          &tailMinusButton, &tailPlusButton })
    {
        configureButton (*button);
        addAndMakeVisible (*button);
    }
    configureEditor (*this, warmupBarsEditor, "Warm-up bars");
    configureEditor (*this, keepBarsEditor, "Keep bars");
    configureEditor (*this, tailBarsEditor, "Tail bars");
    auto clampBarEditor = [this] (juce::TextEditor& editor, int minimum, int fallback, bool showOff)
    {
        auto commit = [this, &editor, minimum, fallback, showOff]
        {
            const auto value = std::max (minimum, intFromEditor (editor, fallback));
            editor.setText (showOff && value == 0 ? "Off" : juce::String (value), juce::dontSendNotification);
            refreshFilenamePreview();
        };
        editor.onReturnKey = commit;
        editor.onFocusLost = commit;
    };
    clampBarEditor (captureStartBarEditor, 1, 1, false);
    clampBarEditor (warmupBarsEditor, 0, 0, true);
    clampBarEditor (keepBarsEditor, 1, 4, false);
    clampBarEditor (tailBarsEditor, 0, 0, true);
    clampBarEditor (bedLengthBarsEditor, 1, 16, false);
    for (auto* editor : { &captureStartBarEditor, &warmupBarsEditor, &keepBarsEditor, &tailBarsEditor })
        editor->onTextChange = [this] { refreshFilenamePreview(); };

    addLabel (*this, keepQuickLabel, "Keep");
    addLabel (*this, warmupQuickLabel, "Warm-up");
    addLabel (*this, tailQuickLabel, "Tail");
    const std::array<int, 5> keepValues { 1, 2, 4, 8, 16 };
    for (std::size_t index = 0; index < keepQuickButtons.size(); ++index)
    {
        keepQuickButtons[index].onClick = [this, bars = keepValues[index]] { setKeepBars (bars); };
        configureButton (keepQuickButtons[index]);
        addAndMakeVisible (keepQuickButtons[index]);
        keepQuickButtons[index].setVisible (false);
    }
    const std::array<int, 5> warmupValues { 0, 1, 4, 8, 16 };
    for (std::size_t index = 0; index < warmupQuickButtons.size(); ++index)
    {
        warmupQuickButtons[index].onClick = [this, bars = warmupValues[index]] { setWarmupBars (bars); };
        configureButton (warmupQuickButtons[index]);
        addAndMakeVisible (warmupQuickButtons[index]);
        warmupQuickButtons[index].setVisible (false);
    }
    const std::array<int, 4> tailValues { 0, 1, 2, 4 };
    for (std::size_t index = 0; index < tailQuickButtons.size(); ++index)
    {
        tailQuickButtons[index].onClick = [this, bars = tailValues[index]] { setTailBars (bars); };
        configureButton (tailQuickButtons[index]);
        addAndMakeVisible (tailQuickButtons[index]);
        tailQuickButtons[index].setVisible (false);
    }
    keepQuickLabel.setVisible (false);
    warmupQuickLabel.setVisible (false);
    tailQuickLabel.setVisible (false);

    fxTitleLabel.setFont (juce::Font { juce::FontOptions { 18.0f, juce::Font::bold } });
    addLabel (*this, fxTitleLabel, "FX RACK");
    gainModuleCard.onClick = [this] { selectFxChainSlot (0); };
    monoModuleCard.onClick = [this] { selectFxChainSlot (1); };
    normalizeModuleCard.onClick = [this] { selectFxChainSlot (2); };
    limitModuleCard.onClick = [this] { selectFxChainSlot (3); };
    compressorModuleCard.onClick = [this] { selectFxChainSlot (4); };
    crushModuleCard.onClick = [this] { selectFxChainSlot (5); };
    filterModuleCard.onClick = [this] { selectFxChainSlot (6); };
    driveModuleCard.onClick = [this] { selectFxChainSlot (7); };
    eqModuleCard.onClick = [this] { selectFxChainSlot (8); };
    delayModuleCard.onClick = [this] { selectFxChainSlot (9); };
    reverbModuleCard.onClick = [this] { selectFxChainSlot (10); };
    tapeModuleCard.onClick = [this] { selectFxChainSlot (11); };
    for (auto* card : { &gainModuleCard, &monoModuleCard, &normalizeModuleCard, &limitModuleCard, &compressorModuleCard,
                        &crushModuleCard, &filterModuleCard, &driveModuleCard, &eqModuleCard,
                        &delayModuleCard, &reverbModuleCard, &tapeModuleCard })
        addAndMakeVisible (*card);
    addFxButton.onClick = [this] { showAddFxMenu(); };
    moveFxLeftButton.onClick = [this] { moveSelectedFxModule (-1); };
    moveFxRightButton.onClick = [this] { moveSelectedFxModule (1); };
    removeFxButton.onClick = [this] { removeSelectedFxModule(); };
    for (auto* button : { &addFxButton, &moveFxLeftButton, &moveFxRightButton, &removeFxButton })
    {
        configureButton (*button);
        addAndMakeVisible (*button);
    }

    fxDetailTitleLabel.setFont (juce::Font { juce::FontOptions { 15.0f, juce::Font::bold } });
    addLabel (*this, fxDetailTitleLabel, "GAIN");
    for (auto* label : { &fxParamLabelA, &fxParamLabelB, &fxParamLabelC, &fxParamLabelD, &fxParamLabelE, &fxParamLabelF })
    {
        label->setFont (juce::Font { juce::FontOptions { 12.0f, juce::Font::bold } });
        addLabel (*this, *label, "");
    }
    fxPowerToggle.onClick = [this]
    {
        const auto enabled = fxPowerToggle.getToggleState();
        if (auto* module = selectedFxModule(); module != nullptr && module->kind == samplebench::FxModuleKind::plugin)
        {
            module->plugin.enabled = enabled;
        }
        else if (selectedFx == samplebench::BuiltInEffectId::gain)
            gainEnabled = enabled;
        else if (selectedFx == samplebench::BuiltInEffectId::mono)
            monoToggle.setToggleState (enabled, juce::dontSendNotification);
        else if (selectedFx == samplebench::BuiltInEffectId::normalize)
            normalizeToggle.setToggleState (enabled, juce::dontSendNotification);
        else if (selectedFx == samplebench::BuiltInEffectId::limit)
            limitEnabled = enabled;
        else if (selectedFx == samplebench::BuiltInEffectId::compressor)
            compressorEnabled = enabled;
        else if (selectedFx == samplebench::BuiltInEffectId::crush)
            crushEnabled = enabled;
        else if (selectedFx == samplebench::BuiltInEffectId::filter)
            filterEnabled = enabled;
        else if (selectedFx == samplebench::BuiltInEffectId::drive)
            driveEnabled = enabled;
        else if (selectedFx == samplebench::BuiltInEffectId::eq)
            eqEnabled = enabled;
        else if (selectedFx == samplebench::BuiltInEffectId::delay)
            delayEnabled = enabled;
        else if (selectedFx == samplebench::BuiltInEffectId::reverb)
            reverbEnabled = enabled;
        else if (selectedFx == samplebench::BuiltInEffectId::tape)
            tapeEnabled = enabled;

        handleFxControlsChanged();
    };
    fxPowerToggle.setColour (juce::ToggleButton::textColourId, colour (samplebench::palette::text));
    addAndMakeVisible (fxPowerToggle);
    fxResetButton.onClick = [this] { resetSelectedFxModule(); };
    configureButton (fxResetButton);
    addAndMakeVisible (fxResetButton);
    openPluginEditorButton.onClick = [this] { openSelectedPluginEditor(); };
    configureButton (openPluginEditorButton);
    addAndMakeVisible (openPluginEditorButton);
    addLabel (*this, pluginStatusLabel, "");
    pluginStatusLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::mutedText));

    addLabel (*this, gainLabel, "Gain");
    gainSlider.setRange (-24.0, 24.0, 0.1);
    gainSlider.setValue (0.0);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
    gainSlider.setMouseDragSensitivity (220);
    gainSlider.onDragStart = [this] { beginFxParameterDrag(); };
    gainSlider.onDragEnd = [this] { endFxParameterDrag(); };
    gainSlider.onValueChange = [this] { handleFxParameterChanged(); };
    addAndMakeVisible (gainSlider);
    normalizeToggle.onClick = [this]
    {
        fxPowerToggle.setToggleState (normalizeToggle.getToggleState(), juce::dontSendNotification);
        handleFxControlsChanged();
    };
    normalizeToggle.setColour (juce::ToggleButton::textColourId, colour (samplebench::palette::text));
    addAndMakeVisible (normalizeToggle);
    monoToggle.setColour (juce::ToggleButton::textColourId, colour (samplebench::palette::text));
    monoToggle.onClick = [this]
    {
        monoToggle.setButtonText (monoToggle.getToggleState() ? "Mono On" : "Mono Off");
        fxPowerToggle.setToggleState (monoToggle.getToggleState(), juce::dontSendNotification);
        handleFxControlsChanged();
    };
    addAndMakeVisible (monoToggle);

    normalizeTargetKnob.setRange (-12.0, 0.0, 0.1);
    normalizeTargetKnob.setValue (-1.0);
    normalizeTargetKnob.setDoubleClickReturnValue (true, -1.0);
    normalizeTargetKnob.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " dBFS"; };
    crushBitsKnob.setRange (4.0, 16.0, 1.0);
    crushBitsKnob.setValue (12.0);
    crushBitsKnob.setDoubleClickReturnValue (true, 12.0);
    crushBitsKnob.textFromValueFunction = [] (double value) { return juce::String (static_cast<int> (std::round (value))) + "-bit"; };
    crushRateKnob.setRange (0.0, 5.0, 1.0);
    crushRateKnob.setValue (3.0);
    crushRateKnob.setDoubleClickReturnValue (true, 3.0);
    crushRateKnob.textFromValueFunction = [] (double value)
    {
        const auto index = static_cast<int> (std::round (value));
        if (index == 0)
            return juce::String { "Off" };
        if (index == 1)
            return juce::String { "44k" };
        if (index == 2)
            return juce::String { "32k" };
        if (index == 3)
            return juce::String { "22k" };
        if (index == 4)
            return juce::String { "16k" };
        return juce::String { "11k" };
    };
    crushMixKnob.setRange (0.0, 100.0, 1.0);
    crushMixKnob.setValue (100.0);
    crushMixKnob.setDoubleClickReturnValue (true, 100.0);
    crushMixKnob.textFromValueFunction = [] (double value) { return juce::String (static_cast<int> (std::round (value))) + "%"; };
    filterCutoffKnob.setRange (20.0, 20000.0, 1.0);
    filterCutoffKnob.setSkewFactorFromMidPoint (1200.0);
    filterCutoffKnob.setValue (12000.0);
    filterCutoffKnob.setDoubleClickReturnValue (true, 12000.0);
    filterCutoffKnob.textFromValueFunction = [] (double value)
    {
        return value >= 1000.0 ? juce::String (value / 1000.0, 1) + " kHz"
                               : juce::String (static_cast<int> (std::round (value))) + " Hz";
    };
    filterResonanceKnob.setRange (0.1, 1.0, 0.01);
    filterResonanceKnob.setValue (0.2);
    filterResonanceKnob.setDoubleClickReturnValue (true, 0.2);
    filterResonanceKnob.textFromValueFunction = [] (double value) { return juce::String (value, 2); };
    for (auto* knob : { &normalizeTargetKnob, &crushBitsKnob, &crushRateKnob, &crushMixKnob, &filterCutoffKnob, &filterResonanceKnob })
    {
        knob->onDragStart = [this] { beginFxParameterDrag(); };
        knob->onDragEnd = [this] { endFxParameterDrag(); };
        knob->onValueChange = [this] { handleFxParameterChanged(); };
        addAndMakeVisible (*knob);
    }
    for (auto* knob : { &genericFxKnobA, &genericFxKnobB, &genericFxKnobC, &genericFxKnobD, &genericFxKnobE, &genericFxKnobF })
    {
        knob->onDragStart = [this] { beginFxParameterDrag(); };
        knob->onDragEnd = [this] { endFxParameterDrag(); };
        knob->onValueChange = [this] { handleGenericFxKnobChanged(); };
        addAndMakeVisible (*knob);
    }
    filterLpButton.onClick = [this]
    {
        filterMode = samplebench::FilterMode::lowPass;
        configureChip (filterLpButton, true);
        configureChip (filterHpButton, false);
        handleFxControlsChanged();
    };
    filterHpButton.onClick = [this]
    {
        filterMode = samplebench::FilterMode::highPass;
        configureChip (filterLpButton, false);
        configureChip (filterHpButton, true);
        handleFxControlsChanged();
    };
    configureButton (filterLpButton);
    configureButton (filterHpButton);
    addAndMakeVisible (filterLpButton);
    addAndMakeVisible (filterHpButton);
    addLabel (*this, fxHintLabel, "Normalize is render-only. Source preview uses Gain, Mono, Crush, and Filter.");
    fxHintLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::mutedText));

    exportTitleLabel.setFont (juce::Font { juce::FontOptions { 18.0f, juce::Font::bold } });
    addLabel (*this, exportTitleLabel, "BOUNCE");
    addLabel (*this, nameLabel, "Name");
    addLabel (*this, flavorLabel, "Flavor");
    addLabel (*this, versionLabel, "Version");
    configureEditor (*this, nameEditor, "Short sample name");
    nameEditor.onTextChange = [this] { refreshFilenamePreview(); };

    flavorSelector.addItem ("dry", 1);
    flavorSelector.addItem ("wet", 2);
    flavorSelector.onChange = [this] { refreshFilenamePreview(); };
    addAndMakeVisible (flavorSelector);

    configureEditor (*this, versionEditor, "Version");
    versionEditor.onTextChange = [this] { refreshFilenamePreview(); };

    speedTrickToggle.onClick = [this] { refreshFilenamePreview(); };
    speedTrickToggle.setColour (juce::ToggleButton::textColourId, colour (samplebench::palette::text));
    addAndMakeVisible (speedTrickToggle);

    addLabel (*this, filenamePreviewLabel, "Filename: -");
    filenamePreviewLabel.setFont (juce::Font { juce::FontOptions { "Menlo", 15.0f, juce::Font::plain } });
    filenamePreviewLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::text));
    filenamePreviewLabel.setColour (juce::Label::backgroundColourId, colour (samplebench::palette::editor));
    filenamePreviewLabel.setColour (juce::Label::outlineColourId, colour (samplebench::palette::border));

    renderPreviewButton.onClick = [this] { renderPreview(); };
    keepVariationButton.onClick = [this] { keepRenderedVariation(); };
    trashRenderButton.onClick = [this] { trashRenderedPreview(); };
    for (auto* button : { &renderPreviewButton, &keepVariationButton, &trashRenderButton })
    {
        configureButton (*button);
        addAndMakeVisible (*button);
    }

    addLabel (*this, renderStatusLabel, "No Bounce");
    renderStatusLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::mutedText));

    addLabel (*this, boundaryLabel, "Exports WAV files and pack folders only. No sampler upload or device control.");
    boundaryLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::mutedText));

    emptyBenchTitleLabel.setFont (juce::Font { juce::FontOptions { 22.0f, juce::Font::bold } });
    addLabel (*this, emptyBenchTitleLabel, "No sample selected");
    addLabel (*this, emptyBenchLabel, "Import audio or select a sample from a bucket to open the workbench.");
    emptyBenchLabel.setColour (juce::Label::textColourId, colour (samplebench::palette::mutedText));
    emptyBenchImportButton.onClick = [this] { chooseFilesToImport(); };
    configureButton (emptyBenchImportButton, true);

    for (auto* component : { static_cast<juce::Component*> (&benchTitleLabel),
                             static_cast<juce::Component*> (&sourceNameLabel),
                             static_cast<juce::Component*> (&sourceMetadataLabel),
                             static_cast<juce::Component*> (&sourceBucketLabel),
                             static_cast<juce::Component*> (&typeLabel),
                             static_cast<juce::Component*> (&bpmLabel),
                             static_cast<juce::Component*> (&barsFieldLabel),
                             static_cast<juce::Component*> (&keyFieldLabel),
                             static_cast<juce::Component*> (&typeSelector),
                             static_cast<juce::Component*> (&musicalBpmEditor),
                             static_cast<juce::Component*> (&barsEditor),
                             static_cast<juce::Component*> (&keyEditor),
                             static_cast<juce::Component*> (&playSourceButton),
                             static_cast<juce::Component*> (&waveformOverview),
                             static_cast<juce::Component*> (&captureTitleLabel),
                             static_cast<juce::Component*> (&returnToStartButton),
                             static_cast<juce::Component*> (&playPreviewButton),
                             static_cast<juce::Component*> (&stopPreviewButton),
                             static_cast<juce::Component*> (&loopPreviewToggle),
                             static_cast<juce::Component*> (&previewTimeLabel),
                             static_cast<juce::Component*> (&previewTargetLabel),
                             static_cast<juce::Component*> (&sourceTargetButton),
                             static_cast<juce::Component*> (&bounceTargetButton),
                             static_cast<juce::Component*> (&sourceBedLabel),
                             static_cast<juce::Component*> (&sourceBedAsIsButton),
                             static_cast<juce::Component*> (&sourceBedExtendButton),
                             static_cast<juce::Component*> (&bedTriggerLabel),
                             static_cast<juce::Component*> (&bedTriggerSelector),
                             static_cast<juce::Component*> (&bedLengthLabel),
                             static_cast<juce::Component*> (&bedLengthBarsEditor),
                             static_cast<juce::Component*> (&bedLengthMinusButton),
                             static_cast<juce::Component*> (&bedLengthPlusButton),
                             static_cast<juce::Component*> (&previewScopeLabel),
                             static_cast<juce::Component*> (&sourceKeepScopeButton),
                             static_cast<juce::Component*> (&sourceFullScopeButton),
                             static_cast<juce::Component*> (&captureStartLabel),
                             static_cast<juce::Component*> (&warmupLabel),
                             static_cast<juce::Component*> (&keepLabel),
                             static_cast<juce::Component*> (&tailLabel),
                             static_cast<juce::Component*> (&regionSummaryLabel),
                             static_cast<juce::Component*> (&captureStartBarEditor),
                             static_cast<juce::Component*> (&startMinusButton),
                             static_cast<juce::Component*> (&startPlusButton),
                             static_cast<juce::Component*> (&warmupBarsEditor),
                             static_cast<juce::Component*> (&warmupMinusButton),
                             static_cast<juce::Component*> (&warmupPlusButton),
                             static_cast<juce::Component*> (&keepBarsEditor),
                             static_cast<juce::Component*> (&keepMinusButton),
                             static_cast<juce::Component*> (&keepPlusButton),
                             static_cast<juce::Component*> (&tailBarsEditor),
                             static_cast<juce::Component*> (&tailMinusButton),
                             static_cast<juce::Component*> (&tailPlusButton),
                             static_cast<juce::Component*> (&keepQuickLabel),
                             static_cast<juce::Component*> (&warmupQuickLabel),
                             static_cast<juce::Component*> (&tailQuickLabel),
                             static_cast<juce::Component*> (&fxTitleLabel),
                             static_cast<juce::Component*> (&gainModuleCard),
                             static_cast<juce::Component*> (&monoModuleCard),
                             static_cast<juce::Component*> (&normalizeModuleCard),
                             static_cast<juce::Component*> (&crushModuleCard),
                             static_cast<juce::Component*> (&filterModuleCard),
                             static_cast<juce::Component*> (&limitModuleCard),
                             static_cast<juce::Component*> (&compressorModuleCard),
                             static_cast<juce::Component*> (&driveModuleCard),
                             static_cast<juce::Component*> (&eqModuleCard),
                             static_cast<juce::Component*> (&delayModuleCard),
                             static_cast<juce::Component*> (&reverbModuleCard),
                             static_cast<juce::Component*> (&tapeModuleCard),
                             static_cast<juce::Component*> (&addFxButton),
                             static_cast<juce::Component*> (&moveFxLeftButton),
                             static_cast<juce::Component*> (&moveFxRightButton),
                             static_cast<juce::Component*> (&removeFxButton),
                             static_cast<juce::Component*> (&fxDetailTitleLabel),
                             static_cast<juce::Component*> (&fxParamLabelA),
                             static_cast<juce::Component*> (&fxParamLabelB),
                             static_cast<juce::Component*> (&fxParamLabelC),
                             static_cast<juce::Component*> (&fxParamLabelD),
                             static_cast<juce::Component*> (&fxParamLabelE),
                             static_cast<juce::Component*> (&fxParamLabelF),
                             static_cast<juce::Component*> (&fxPowerToggle),
                             static_cast<juce::Component*> (&fxResetButton),
                             static_cast<juce::Component*> (&openPluginEditorButton),
                             static_cast<juce::Component*> (&pluginStatusLabel),
                             static_cast<juce::Component*> (&gainSlider),
                             static_cast<juce::Component*> (&gainLabel),
                             static_cast<juce::Component*> (&normalizeToggle),
                             static_cast<juce::Component*> (&monoToggle),
                             static_cast<juce::Component*> (&normalizeTargetKnob),
                             static_cast<juce::Component*> (&crushBitsKnob),
                             static_cast<juce::Component*> (&crushRateKnob),
                             static_cast<juce::Component*> (&crushMixKnob),
                             static_cast<juce::Component*> (&filterCutoffKnob),
                             static_cast<juce::Component*> (&filterResonanceKnob),
                             static_cast<juce::Component*> (&genericFxKnobA),
                             static_cast<juce::Component*> (&genericFxKnobB),
                             static_cast<juce::Component*> (&genericFxKnobC),
                             static_cast<juce::Component*> (&genericFxKnobD),
                             static_cast<juce::Component*> (&genericFxKnobE),
                             static_cast<juce::Component*> (&genericFxKnobF),
                             static_cast<juce::Component*> (&filterLpButton),
                             static_cast<juce::Component*> (&filterHpButton),
                             static_cast<juce::Component*> (&fxHintLabel),
                             static_cast<juce::Component*> (&exportTitleLabel),
                             static_cast<juce::Component*> (&nameLabel),
                             static_cast<juce::Component*> (&flavorLabel),
                             static_cast<juce::Component*> (&versionLabel),
                             static_cast<juce::Component*> (&nameEditor),
                             static_cast<juce::Component*> (&flavorSelector),
                             static_cast<juce::Component*> (&versionEditor),
                             static_cast<juce::Component*> (&speedTrickToggle),
                             static_cast<juce::Component*> (&filenamePreviewLabel),
                             static_cast<juce::Component*> (&renderPreviewButton),
                             static_cast<juce::Component*> (&keepVariationButton),
                             static_cast<juce::Component*> (&trashRenderButton),
                             static_cast<juce::Component*> (&renderStatusLabel),
                             static_cast<juce::Component*> (&boundaryLabel),
                             static_cast<juce::Component*> (&emptyBenchTitleLabel),
                             static_cast<juce::Component*> (&emptyBenchLabel),
                             static_cast<juce::Component*> (&emptyBenchImportButton) })
    {
        benchContent.addAndMakeVisible (*component);
    }
    for (auto& button : keepQuickButtons)
        benchContent.addAndMakeVisible (button);
    for (auto& button : warmupQuickButtons)
        benchContent.addAndMakeVisible (button);
    for (auto& button : tailQuickButtons)
        benchContent.addAndMakeVisible (button);

    keyFieldLabel.setVisible (false);
    keyEditor.setVisible (false);
    playSourceButton.setVisible (false);
    emptyBenchTitleLabel.setVisible (false);
    emptyBenchLabel.setVisible (false);
    emptyBenchImportButton.setVisible (false);
    keepQuickLabel.setVisible (false);
    warmupQuickLabel.setVisible (false);
    tailQuickLabel.setVisible (false);
    for (auto& button : keepQuickButtons)
        button.setVisible (false);
    for (auto& button : warmupQuickButtons)
        button.setVisible (false);
    for (auto& button : tailQuickButtons)
        button.setVisible (false);

    refreshFxControls();
    setBenchControlsEnabled (false);
    setRenderActionsAvailable (false);
    refreshTransportControls();
    startTimerHz (20);
    refreshPackChrome();
    refreshBucketButtons();
    refreshBenchView();
}

MainComponent::~MainComponent()
{
    shuttingDown = true;
    pluginEditorWindows.clear();
    sourceTransport.stop();
    renderTransport.stop();
    sourceTransport.setSource (nullptr);
    renderTransport.setSource (nullptr);
    audioDeviceManager.removeAudioCallback (&audioSourcePlayer);
    audioSourcePlayer.setSource (nullptr);
    mixerSource.removeAllInputs();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colour (samplebench::palette::background));

    auto full = getLocalBounds();
    g.setColour (colour (samplebench::palette::editor));
    g.fillRect (full.removeFromTop (42));

    auto bounds = full.reduced (margin);
    const auto left = bounds.removeFromLeft (270);
    bounds.removeFromLeft (gap);
    bounds.removeFromRight (44);
    bounds.removeFromRight (gap);
    const auto benchPanel = bounds;

    g.setColour (colour (samplebench::palette::panel));
    g.fillRoundedRectangle (left.toFloat(), 6.0f);
    g.fillRoundedRectangle (benchPanel.toFloat(), 6.0f);
}

void MainComponent::resized()
{
    auto whole = getLocalBounds();
    auto transport = whole.removeFromTop (42).reduced (10, 6);
    topAppLabel.setBounds (transport.removeFromLeft (190));
    topPackLabel.setBounds (transport.removeFromLeft (150));
    transport.removeFromLeft (12);
    transportStatusLabel.setBounds (transport.removeFromLeft (210));
    exportPackButton.setBounds (transport.removeFromRight (118));
    transport.removeFromRight (8);
    settingsButton.setBounds (transport.removeFromRight (104));

    auto bounds = whole.reduced (margin);
    auto left = bounds.removeFromLeft (270);
    bounds.removeFromLeft (gap);
    auto meterArea = bounds.removeFromRight (44);
    bounds.removeFromRight (gap);
    levelMeter.setBounds (meterArea);
    const auto benchPanel = bounds;
    benchViewport.setBounds (benchPanel);
    const auto benchContentWidth = std::max (640, benchPanel.getWidth() - 14);
    const auto benchContentHeight = std::max (980, benchPanel.getHeight());
    benchContent.setSize (benchContentWidth, benchContentHeight);
    auto bench = benchContent.getLocalBounds().reduced (22, 18);

    auto packNameRow = left.removeFromTop (30);
    titleLabel.setBounds (packNameRow.removeFromLeft (48));
    packNameEditor.setBounds (packNameRow);
    subtitleLabel.setBounds (left.removeFromTop (20));

    auto packActions = left.removeFromTop (30);
    const auto actionWidth = (packActions.getWidth() - 8) / 3;
    newPackButton.setBounds (packActions.removeFromLeft (actionWidth));
    packActions.removeFromLeft (4);
    openPackButton.setBounds (packActions.removeFromLeft (actionWidth));
    packActions.removeFromLeft (4);
    savePackButton.setBounds (packActions);
    left.removeFromTop (6);

    importButton.setBounds (left.removeFromTop (30));
    left.removeFromTop (gap);

    for (auto& button : bucketButtons)
    {
        button.setBounds (left.removeFromTop (28));
        left.removeFromTop (4);
    }

    left.removeFromTop (gap);
    sampleList.setBounds (left);

    if (emptyBenchTitleLabel.isVisible())
    {
        benchTitleLabel.setBounds ({});
        sourceNameLabel.setBounds ({});
        sourceMetadataLabel.setBounds ({});
        sourceBucketLabel.setBounds ({});
        typeLabel.setBounds ({});
        bpmLabel.setBounds ({});
        barsFieldLabel.setBounds ({});
        typeSelector.setBounds ({});
        musicalBpmEditor.setBounds ({});
        barsEditor.setBounds ({});
        keyFieldLabel.setBounds ({});
        keyEditor.setBounds ({});
        emptyBenchLabel.setBounds ({});
        emptyBenchImportButton.setBounds ({});

        auto empty = bench.reduced (32);
        empty.removeFromTop (std::max (24, empty.getHeight() / 4));
        emptyBenchTitleLabel.setBounds (empty.removeFromTop (34));
        emptyBenchLabel.setBounds (empty.removeFromTop (64));
        empty.removeFromTop (12);
        emptyBenchImportButton.setBounds (empty.removeFromTop (34).removeFromLeft (140));
        return;
    }

    emptyBenchTitleLabel.setBounds ({});
    emptyBenchLabel.setBounds ({});
    emptyBenchImportButton.setBounds ({});

    auto header = bench.removeFromTop (94);
    sourceBucketLabel.setBounds (header.removeFromLeft (96).reduced (0, 18));
    header.removeFromLeft (gap);
    auto headerMain = header.removeFromLeft (std::max (260, header.getWidth() - 430));
    benchTitleLabel.setBounds (headerMain.removeFromTop (26));
    sourceNameLabel.setBounds (headerMain.removeFromTop (24));
    sourceMetadataLabel.setBounds (headerMain.removeFromTop (20));

    auto sourceLabelRow = header.removeFromTop (20);
    typeLabel.setBounds (sourceLabelRow.removeFromLeft (120));
    sourceLabelRow.removeFromLeft (gap);
    bpmLabel.setBounds (sourceLabelRow.removeFromLeft (95));
    sourceLabelRow.removeFromLeft (gap);
    barsFieldLabel.setBounds (sourceLabelRow.removeFromLeft (60));
    auto sourceControls = header.removeFromTop (34);
    typeSelector.setBounds (sourceControls.removeFromLeft (120));
    sourceControls.removeFromLeft (gap);
    musicalBpmEditor.setBounds (sourceControls.removeFromLeft (95));
    sourceControls.removeFromLeft (gap);
    barsEditor.setBounds (sourceControls.removeFromLeft (60));
    sourceControls.removeFromLeft (gap);
    playSourceButton.setBounds ({});
    keyFieldLabel.setVisible (false);
    keyEditor.setVisible (false);

    bench.removeFromTop (gap);
    captureTitleLabel.setBounds (bench.removeFromTop (26));
    waveformOverview.setBounds (bench.removeFromTop (170));

    bench.removeFromTop (gap);
    auto previewTransport = bench.removeFromTop (34);
    returnToStartButton.setBounds (previewTransport.removeFromLeft (42));
    previewTransport.removeFromLeft (6);
    playPreviewButton.setBounds (previewTransport.removeFromLeft (72));
    previewTransport.removeFromLeft (6);
    stopPreviewButton.setBounds (previewTransport.removeFromLeft (72));
    previewTransport.removeFromLeft (6);
    loopPreviewToggle.setBounds (previewTransport.removeFromLeft (74));
    previewTransport.removeFromLeft (12);
    previewTimeLabel.setBounds (previewTransport.removeFromLeft (156));

    auto targetRow = bench.removeFromTop (32);
    previewTargetLabel.setBounds (targetRow.removeFromLeft (56));
    targetRow.removeFromLeft (6);
    sourceTargetButton.setBounds (targetRow.removeFromLeft (86));
    targetRow.removeFromLeft (6);
    bounceTargetButton.setBounds (targetRow.removeFromLeft (86));
    targetRow.removeFromLeft (18);
    previewScopeLabel.setBounds (targetRow.removeFromLeft (54));
    targetRow.removeFromLeft (6);
    sourceKeepScopeButton.setBounds (targetRow.removeFromLeft (70));
    targetRow.removeFromLeft (6);
    sourceFullScopeButton.setBounds (targetRow.removeFromLeft (70));

    auto bedRow = bench.removeFromTop (32);
    sourceBedLabel.setBounds (bedRow.removeFromLeft (80));
    bedRow.removeFromLeft (6);
    sourceBedAsIsButton.setBounds (bedRow.removeFromLeft (64));
    bedRow.removeFromLeft (6);
    sourceBedExtendButton.setBounds (bedRow.removeFromLeft (116));
    bedRow.removeFromLeft (14);
    bedTriggerLabel.setBounds (bedRow.removeFromLeft (50));
    bedRow.removeFromLeft (6);
    bedTriggerSelector.setBounds (bedRow.removeFromLeft (118));
    bedRow.removeFromLeft (14);
    bedLengthLabel.setBounds (bedRow.removeFromLeft (112));
    bedRow.removeFromLeft (6);
    bedLengthMinusButton.setBounds (bedRow.removeFromLeft (30));
    bedRow.removeFromLeft (4);
    bedLengthBarsEditor.setBounds (bedRow.removeFromLeft (54));
    bedRow.removeFromLeft (4);
    bedLengthPlusButton.setBounds (bedRow.removeFromLeft (30));

    regionSummaryLabel.setBounds (bench.removeFromTop (24));

    bench.removeFromTop (gap);
    auto captureLabels = bench.removeFromTop (22);
    auto labelWidth = captureLabels.getWidth() / 4;
    captureStartLabel.setBounds (captureLabels.removeFromLeft (labelWidth).reduced (0, 0));
    warmupLabel.setBounds (captureLabels.removeFromLeft (labelWidth).reduced (0, 0));
    keepLabel.setBounds (captureLabels.removeFromLeft (labelWidth).reduced (0, 0));
    tailLabel.setBounds (captureLabels.reduced (0, 0));

    auto capture = bench.removeFromTop (34);
    auto layoutNumberControl = [] (juce::Rectangle<int> area,
                                   juce::TextButton& minusButton,
                                   juce::TextEditor& editor,
                                   juce::TextButton& plusButton)
    {
        area = area.reduced (0, 0);
        minusButton.setBounds (area.removeFromLeft (32));
        area.removeFromLeft (4);
        editor.setBounds (area.removeFromLeft (62));
        area.removeFromLeft (4);
        plusButton.setBounds (area.removeFromLeft (32));
    };
    auto controlWidth = capture.getWidth() / 4;
    layoutNumberControl (capture.removeFromLeft (controlWidth), startMinusButton, captureStartBarEditor, startPlusButton);
    layoutNumberControl (capture.removeFromLeft (controlWidth), warmupMinusButton, warmupBarsEditor, warmupPlusButton);
    layoutNumberControl (capture.removeFromLeft (controlWidth), keepMinusButton, keepBarsEditor, keepPlusButton);
    layoutNumberControl (capture, tailMinusButton, tailBarsEditor, tailPlusButton);

    bench.removeFromTop (gap);
    auto bottom = bench.removeFromTop (std::max (348, bench.getHeight() - 26));
    auto fx = bottom.removeFromLeft (bottom.getWidth() / 2 - gap / 2);
    bottom.removeFromLeft (gap);
    auto exportBox = bottom;

    fxTitleLabel.setBounds (fx.removeFromTop (28));
    auto rackArea = fx.removeFromTop (112);
    auto rackRowA = rackArea.removeFromTop (52);
    rackArea.removeFromTop (8);
    auto rackRowB = rackArea.removeFromTop (52);
    const auto chainCount = std::min<int> (static_cast<int> (fxChain.size()), 12);
    std::array<FxModuleCard*, 12> cards { &gainModuleCard, &monoModuleCard, &normalizeModuleCard, &limitModuleCard, &compressorModuleCard,
                                          &crushModuleCard, &filterModuleCard, &driveModuleCard, &eqModuleCard,
                                          &delayModuleCard, &reverbModuleCard, &tapeModuleCard };
    const auto firstRowCount = std::min (chainCount, 6);
    const auto secondRowCount = std::max (0, chainCount - 6);
    const auto cardWidthA = firstRowCount > 0 ? std::max (1, (rackRowA.getWidth() - 8 * firstRowCount - 46) / firstRowCount) : 0;
    const auto cardWidthB = secondRowCount > 0 ? std::max (1, (rackRowB.getWidth() - 8 * secondRowCount) / secondRowCount) : 0;
    for (int index = 0; index < 12; ++index)
    {
        if (index < chainCount)
        {
            auto& row = index < 6 ? rackRowA : rackRowB;
            const auto cardWidth = index < 6 ? cardWidthA : cardWidthB;
            cards[static_cast<std::size_t> (index)]->setBounds (row.removeFromLeft (cardWidth));
            row.removeFromLeft (8);
        }
        else
        {
            cards[static_cast<std::size_t> (index)]->setBounds ({});
        }
    }
    addFxButton.setBounds (rackRowA.removeFromLeft (42));

    fx.removeFromTop (gap);
    auto chainTools = fx.removeFromTop (30);
    moveFxLeftButton.setBounds (chainTools.removeFromLeft (36));
    chainTools.removeFromLeft (6);
    moveFxRightButton.setBounds (chainTools.removeFromLeft (36));
    chainTools.removeFromLeft (6);
    removeFxButton.setBounds (chainTools.removeFromLeft (86));
    fx.removeFromTop (4);

    auto detailHeader = fx.removeFromTop (28);
    fxDetailTitleLabel.setBounds (detailHeader.removeFromLeft (120));
    fxPowerToggle.setBounds (detailHeader.removeFromLeft (90));
    fxResetButton.setBounds (detailHeader.removeFromLeft (72));
    detailHeader.removeFromLeft (8);
    openPluginEditorButton.setBounds (detailHeader.removeFromLeft (116));
    fx.removeFromTop (6);

    auto detail = fx.removeFromTop (112);
    const auto fxDetail = samplebench::calculateFxDetailLayout (selectedFx, detail.getX(), detail.getY(), detail.getWidth(), detail.getHeight());
    auto utilityRow = juce::Rectangle<int> { fxDetail.utilityRow.x, fxDetail.utilityRow.y, fxDetail.utilityRow.width, fxDetail.utilityRow.height };
    gainLabel.setBounds (utilityRow.removeFromTop (18));
    gainSlider.setBounds (utilityRow.removeFromTop (30));
    monoToggle.setBounds (juce::Rectangle<int> { fxDetail.utilityRow.x, fxDetail.utilityRow.y, 180, fxDetail.utilityRow.height });
    normalizeToggle.setBounds (juce::Rectangle<int> { fxDetail.utilityRow.x, fxDetail.utilityRow.y, 220, fxDetail.utilityRow.height });

    auto labelRow = juce::Rectangle<int> { fxDetail.knobLabelRow.x, fxDetail.knobLabelRow.y, fxDetail.knobLabelRow.width, fxDetail.knobLabelRow.height };
    fxParamLabelA.setBounds (labelRow.removeFromLeft (86));
    labelRow.removeFromLeft (8);
    fxParamLabelB.setBounds (labelRow.removeFromLeft (86));
    labelRow.removeFromLeft (8);
    fxParamLabelC.setBounds (labelRow.removeFromLeft (86));
    labelRow.removeFromLeft (8);
    fxParamLabelD.setBounds (labelRow.removeFromLeft (86));
    labelRow.removeFromLeft (8);
    fxParamLabelE.setBounds (labelRow.removeFromLeft (86));
    labelRow.removeFromLeft (8);
    fxParamLabelF.setBounds (labelRow.removeFromLeft (86));

    const auto knobA = juce::Rectangle<int> { fxDetail.knobA.x, fxDetail.knobA.y, fxDetail.knobA.width, fxDetail.knobA.height };
    const auto knobB = juce::Rectangle<int> { fxDetail.knobB.x, fxDetail.knobB.y, fxDetail.knobB.width, fxDetail.knobB.height };
    const auto knobC = juce::Rectangle<int> { fxDetail.knobC.x, fxDetail.knobC.y, fxDetail.knobC.width, fxDetail.knobC.height };
    const auto knobWidth = std::max (62, (detail.getWidth() - 8 * 5) / 6);
    auto genericRow = detail.removeFromTop (68);
    std::array<juce::Rectangle<int>, 6> genericSlots;
    for (auto& slot : genericSlots)
    {
        slot = genericRow.removeFromLeft (knobWidth);
        genericRow.removeFromLeft (8);
    }
    normalizeTargetKnob.setBounds (selectedFx == samplebench::BuiltInEffectId::normalize ? knobA : juce::Rectangle<int> {});
    crushBitsKnob.setBounds (selectedFx == samplebench::BuiltInEffectId::crush ? knobA : juce::Rectangle<int> {});
    crushRateKnob.setBounds (selectedFx == samplebench::BuiltInEffectId::crush ? knobB : juce::Rectangle<int> {});
    crushMixKnob.setBounds (selectedFx == samplebench::BuiltInEffectId::crush ? knobC : juce::Rectangle<int> {});
    filterCutoffKnob.setBounds (selectedFx == samplebench::BuiltInEffectId::filter ? knobA : juce::Rectangle<int> {});
    filterResonanceKnob.setBounds (selectedFx == samplebench::BuiltInEffectId::filter ? knobB : juce::Rectangle<int> {});
    const auto showGenericKnobs = isNewFxDetailEffect (selectedFx);
    genericFxKnobA.setBounds (showGenericKnobs ? genericSlots[0] : juce::Rectangle<int> {});
    genericFxKnobB.setBounds (showGenericKnobs ? genericSlots[1] : juce::Rectangle<int> {});
    genericFxKnobC.setBounds (showGenericKnobs ? genericSlots[2] : juce::Rectangle<int> {});
    genericFxKnobD.setBounds (showGenericKnobs ? genericSlots[3] : juce::Rectangle<int> {});
    genericFxKnobE.setBounds (showGenericKnobs ? genericSlots[4] : juce::Rectangle<int> {});
    genericFxKnobF.setBounds (showGenericKnobs ? genericSlots[5] : juce::Rectangle<int> {});

    auto filterModeRow = juce::Rectangle<int> { fxDetail.filterModeRow.x, fxDetail.filterModeRow.y, fxDetail.filterModeRow.width, fxDetail.filterModeRow.height };
    filterLpButton.setBounds (filterModeRow.removeFromLeft (46));
    filterModeRow.removeFromLeft (6);
    filterHpButton.setBounds (filterModeRow.removeFromLeft (46));
    pluginStatusLabel.setBounds (detail.removeFromTop (26));
    fxHintLabel.setBounds (juce::Rectangle<int> { fxDetail.hintRow.x, fxDetail.hintRow.y, fxDetail.hintRow.width, fxDetail.hintRow.height });

    exportTitleLabel.setBounds (exportBox.removeFromTop (28));
    auto exportLabels = exportBox.removeFromTop (22);
    nameLabel.setBounds (exportLabels.removeFromLeft (170));
    exportLabels.removeFromLeft (gap);
    flavorLabel.setBounds (exportLabels.removeFromLeft (90));
    exportLabels.removeFromLeft (gap);
    versionLabel.setBounds (exportLabels.removeFromLeft (80));
    auto exportRow = exportBox.removeFromTop (34);
    nameEditor.setBounds (exportRow.removeFromLeft (170));
    exportRow.removeFromLeft (gap);
    flavorSelector.setBounds (exportRow.removeFromLeft (90));
    exportRow.removeFromLeft (gap);
    versionEditor.setBounds (exportRow.removeFromLeft (80));

    speedTrickToggle.setBounds (exportBox.removeFromTop (28));
    filenamePreviewLabel.setBounds (exportBox.removeFromTop (34));
    auto renderRow = exportBox.removeFromTop (34);
    renderPreviewButton.setBounds (renderRow.removeFromLeft (140));

    exportBox.removeFromTop (gap);
    auto keepRow = exportBox.removeFromTop (34);
    keepVariationButton.setBounds (keepRow.removeFromLeft (140));
    keepRow.removeFromLeft (gap);
    trashRenderButton.setBounds (keepRow.removeFromLeft (120));
    renderStatusLabel.setBounds (exportBox.removeFromTop (28));

    if (bench.getHeight() > 28)
    {
        bench.removeFromTop (gap);
        boundaryLabel.setBounds (bench.removeFromTop (28));
    }
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    return std::any_of (files.begin(), files.end(), [] (const juce::String& path)
    {
        return isSupportedAudioFile (juce::File { path });
    });
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    importFiles (files);
}

int MainComponent::getNumRows()
{
    return static_cast<int> (currentSamples().size());
}

void MainComponent::paintListBoxItem (int rowNumber,
                                      juce::Graphics& g,
                                      int width,
                                      int height,
                                      bool rowIsSelected)
{
    const auto& samples = currentSamples();
    if (rowNumber < 0 || rowNumber >= static_cast<int> (samples.size()))
        return;

    g.fillAll (rowIsSelected ? colour (samplebench::palette::accent)
                              : colour (samplebench::palette::editor));

    g.setColour (rowIsSelected ? colour (samplebench::palette::inverseText) : colour (samplebench::palette::text));
    g.drawText (juce::String (samples[static_cast<std::size_t> (rowNumber)].displayName),
                10,
                0,
                width - 38,
                height,
                juce::Justification::centredLeft);

    g.setColour ((rowIsSelected ? colour (samplebench::palette::inverseText)
                                : colour (samplebench::palette::mutedText)).withAlpha (0.75f));
    const auto gripX = width - 20;
    const auto centreY = height / 2;
    for (int offset : { -5, 0, 5 })
        g.fillRoundedRectangle (static_cast<float> (gripX),
                                static_cast<float> (centreY + offset),
                                14.0f,
                                2.0f,
                                1.0f);
}

void MainComponent::selectedRowsChanged (int lastRowSelected)
{
    const auto& samples = currentSamples();
    if (lastRowSelected >= 0 && lastRowSelected < static_cast<int> (samples.size()))
        pack.selectSample (samples[static_cast<std::size_t> (lastRowSelected)].id);
    else
        pack.clearSelection();

    trashRenderedPreview();
    refreshBenchView();
}

juce::var MainComponent::getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe)
{
    if (rowsToDescribe.size() <= 0)
        return {};

    const auto row = rowsToDescribe[0];
    const auto& samples = currentSamples();
    if (row < 0 || row >= static_cast<int> (samples.size()))
        return {};

    return sampleDragDescription (samples[static_cast<std::size_t> (row)].id);
}

bool MainComponent::mayDragToExternalWindows() const
{
    return false;
}

juce::MouseCursor MainComponent::getMouseCursorForRow (int row)
{
    const auto& samples = currentSamples();
    return row >= 0 && row < static_cast<int> (samples.size())
        ? juce::MouseCursor::DraggingHandCursor
        : juce::MouseCursor::NormalCursor;
}

juce::String MainComponent::getTooltipForRow (int row)
{
    const auto& samples = currentSamples();
    if (row < 0 || row >= static_cast<int> (samples.size()))
        return {};

    return "Drag onto a bucket to move this sample.";
}

void MainComponent::configureBucketButtons()
{
    const auto& buckets = pack.buckets();
    for (std::size_t index = 0; index < bucketButtons.size(); ++index)
    {
        auto& button = bucketButtons[index];
        button.bucket = buckets[index].id;
        button.setTooltip ("Drop a sample here to move it to " + bucketLabel (buckets[index].id) + ".");
        button.onClick = [this, bucket = buckets[index].id] { selectBucket (bucket); };
        button.onSampleDropped = [this] (std::size_t sampleId, samplebench::BucketId bucket)
        {
            moveSampleToBucket (sampleId, bucket);
        };
        configureButton (button);
        addAndMakeVisible (button);
    }
}

void MainComponent::chooseFilesToImport()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Import audio files",
        juce::File {},
        "*.wav;*.aif;*.aiff;*.flac;*.mp3");

    constexpr auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::canSelectMultipleItems;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        juce::StringArray paths;
        for (const auto& file : chooser.getResults())
            paths.add (file.getFullPathName());

        importFiles (paths);
    });
}

void MainComponent::importFiles (const juce::StringArray& files)
{
    bool importedAny = false;
    for (const auto& path : files)
    {
        const juce::File file { path };
        if (! isSupportedAudioFile (file))
            continue;

        pack.importSample (currentBucket, std::filesystem::path { file.getFullPathName().toStdString() });
        importedAny = true;
    }

    if (importedAny)
        markPackDirty();
    refreshSampleList();
    refreshBucketButtons();
    refreshBenchView();
}

void MainComponent::requestNewPack()
{
    commitPackNameEditor();
    if (! packDirty && ! pack.hasSamples())
    {
        resetToNewPack();
        return;
    }

    savePackBeforeReplacing ([this] { resetToNewPack(); });
}

void MainComponent::resetToNewPack()
{
    // Temp previews are tied to the previous pack/sample. Clear them before swapping the
    // model so transport state cannot keep pointing at stale audio.
    trashRenderedPreview();
    clearNoSamplePreviewState();
    pack = samplebench::Pack::create ("UNTITLED_PACK");
    currentBucket = samplebench::BucketId::drums;
    currentPackFile = juce::File {};
    packDirty = false;
    fxChain.clear();
    selectedFxSlot = -1;
    sampleList.deselectAllRows();
    refreshPackChrome();
    refreshBucketButtons();
    refreshSampleList();
    refreshBenchView();
}

void MainComponent::requestOpenPack()
{
    commitPackNameEditor();
    const auto chooseFile = [this]
    {
        packFileChooser = std::make_unique<juce::FileChooser> ("Open Bench Sampler pack",
                                                               juce::File {},
                                                               "*.sfbpack");
        packFileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectFiles,
                                      [this] (const juce::FileChooser& chooser)
                                      {
                                          const auto file = chooser.getResult();
                                          if (file.existsAsFile())
                                              openPackFile (file);
                                      });
    };

    if (packDirty)
        savePackBeforeReplacing (chooseFile);
    else
        chooseFile();
}

void MainComponent::openPackFile (const juce::File& file)
{
    const auto loaded = samplebench::Pack::loadSessionFromDisk (file.getFullPathName().toStdString());
    if (! loaded.has_value())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Open Pack Failed",
                                                "This file is not a readable Bench Sampler pack session.");
        return;
    }

    // Opening a pack restores saved model state, but bounce previews are runtime scratch
    // files. Rebuild those through Render Preview rather than trusting old temp paths.
    trashRenderedPreview();
    clearNoSamplePreviewState();
    pack = *loaded;
    currentPackFile = file;
    packDirty = false;
    currentBucket = pack.selectedSample().has_value() ? pack.selectedSample()->bucket : samplebench::BucketId::drums;
    fxChain.clear();
    selectedFxSlot = -1;
    refreshPackChrome();
    refreshBucketButtons();
    refreshSampleList();
    sampleList.deselectAllRows();
    if (const auto selected = pack.selectedSample())
    {
        const auto& samples = currentSamples();
        const auto found = std::find_if (samples.begin(), samples.end(), [&] (const samplebench::Sample& sample)
        {
            return sample.id == selected->id;
        });

        if (found != samples.end())
            sampleList.selectRow (static_cast<int> (std::distance (samples.begin(), found)));
    }
    refreshBenchView();
}

void MainComponent::savePack()
{
    commitPackNameEditor();
    if (currentPackFile == juce::File {})
    {
        savePackAs();
        return;
    }

    if (! savePackToFile (currentPackFile))
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Save Pack Failed",
                                                "The pack could not be saved to the selected location.");
}

void MainComponent::savePackAs (std::function<void (bool)> afterSave)
{
    // The pack display name stays independent from the chosen .sfbpack path, but a safe
    // default filename makes the first Save As land where users expect.
    const auto initialFile = currentPackFile == juce::File {}
                           ? juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                 .getChildFile (juce::String::fromUTF8 (samplebench::safeExportFolderName (std::string (pack.name())).c_str()) + ".sfbpack")
                           : currentPackFile;
    packFileChooser = std::make_unique<juce::FileChooser> ("Save Bench Sampler pack",
                                                           initialFile,
                                                           "*.sfbpack");
    packFileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this, afterSave = std::move (afterSave)] (const juce::FileChooser& chooser) mutable
                                  {
                                      auto file = chooser.getResult();
                                      if (file == juce::File {})
                                      {
                                          if (afterSave != nullptr)
                                              afterSave (false);
                                          return;
                                      }

                                      if (file.getFileExtension().isEmpty())
                                          file = file.withFileExtension (".sfbpack");

                                      const auto saved = savePackToFile (file);
                                      if (! saved)
                                          juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                                                  "Save Pack Failed",
                                                                                  "The pack could not be saved to the selected location.");
                                      if (afterSave != nullptr)
                                          afterSave (saved);
                                  });
}

bool MainComponent::savePackToFile (const juce::File& file)
{
    if (! pack.saveSessionToDisk (file.getFullPathName().toStdString()))
        return false;

    currentPackFile = file;
    packDirty = false;
    refreshPackChrome();
    return true;
}

void MainComponent::savePackBeforeReplacing (std::function<void()> afterSaveOrDiscard)
{
    juce::AlertWindow::showYesNoCancelBox (juce::AlertWindow::WarningIcon,
                                           "Save current pack?",
                                           "Save changes to the current pack before continuing?",
                                           "Save",
                                           "Discard",
                                           "Cancel",
                                           this,
                                           juce::ModalCallbackFunction::create ([this, afterSaveOrDiscard = std::move (afterSaveOrDiscard)] (int result) mutable
                                           {
                                               if (result == 1)
                                               {
                                                   if (currentPackFile == juce::File {})
                                                       savePackAs ([afterSaveOrDiscard = std::move (afterSaveOrDiscard)] (bool saved) mutable
                                                       {
                                                           if (saved && afterSaveOrDiscard != nullptr)
                                                               afterSaveOrDiscard();
                                                       });
                                                   else if (savePackToFile (currentPackFile) && afterSaveOrDiscard != nullptr)
                                                   {
                                                       afterSaveOrDiscard();
                                                   }
                                               }
                                               else if (result == 2 && afterSaveOrDiscard != nullptr)
                                               {
                                                   afterSaveOrDiscard();
                                               }
                                           }));
}

void MainComponent::commitPackNameEditor()
{
    const auto previousName = juce::String::fromUTF8 (std::string (pack.name()).c_str());
    const auto requestedName = packNameEditor.getText().trim();
    if (requestedName.isEmpty())
    {
        packNameEditor.setText (previousName, juce::dontSendNotification);
        return;
    }

    if (requestedName == previousName)
        return;

    if (pack.rename (requestedName.toStdString()))
    {
        markPackDirty();
        refreshPackChrome();
    }
    else
    {
        packNameEditor.setText (previousName, juce::dontSendNotification);
    }
}

void MainComponent::requestExportPack()
{
    // Export is a folder operation, not a session save. The .sfbpack keeps editing state;
    // this creates the sampler-ready bucket folders from kept bounces.
    commitPackNameEditor();
    packFileChooser = std::make_unique<juce::FileChooser> ("Choose folder for Pack Export",
                                                           juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                           juce::String {});
    packFileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& chooser)
                                  {
                                      const auto directory = chooser.getResult();
                                      if (directory != juce::File {})
                                          exportPackToDirectory (directory);
                                  });
}

void MainComponent::exportPackToDirectory (const juce::File& destinationDirectory)
{
    samplebench::ExportOptions options;
    options.includeKeptBounces = true;
    options.includeOriginalSources = false;
    options.includeNotesFile = true;

    const auto timestamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M").toStdString();
    const auto result = samplebench::exportPackToFolder (pack,
                                                         std::filesystem::path { destinationDirectory.getFullPathName().toStdString() },
                                                         options,
                                                         timestamp);
    showExportResult (result);
}

void MainComponent::showExportResult (const samplebench::PackExportResult& result)
{
    if (! result.success)
    {
        juce::String message = "The pack could not be exported.";
        for (const auto& error : result.errors)
            message += "\n" + juce::String::fromUTF8 (error.c_str());
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Export Pack Failed",
                                                message);
        return;
    }

    juce::String message;
    message << "Folder: " << juce::String::fromUTF8 (result.folderPath.u8string().c_str())
            << "\nFiles exported: " << result.exportedFiles
            << "\nSkipped: " << result.skippedFiles;
    if (! result.errors.empty())
    {
        message << "\n\nWarnings:";
        for (const auto& error : result.errors)
            message << "\n" << juce::String::fromUTF8 (error.c_str());
    }

    juce::AlertWindow::showOkCancelBox (juce::AlertWindow::InfoIcon,
                                        "Export Complete",
                                        message,
                                        "Open Folder",
                                        "Close",
                                        this,
                                        juce::ModalCallbackFunction::create ([folder = result.folderPath] (int resultCode)
                                        {
                                            if (resultCode == 1)
                                                juce::File { juce::String::fromUTF8 (folder.u8string().c_str()) }.revealToUser();
                                        }));
}

void MainComponent::markPackDirty()
{
    if (loadingControls || suppressPackDirty)
        return;

    packDirty = true;
    refreshPackChrome();
}

void MainComponent::refreshPackChrome()
{
    const auto dirty = packDirty ? "*" : "";
    const auto packName = juce::String { std::string (pack.name()) };
    if (! packNameEditor.hasKeyboardFocus (true))
        packNameEditor.setText (packName, juce::dontSendNotification);
    topPackLabel.setText ("PACK  " + packName + dirty, juce::dontSendNotification);
    subtitleLabel.setText (currentPackFile == juce::File {} ? "Unsaved local pack"
                                                            : currentPackFile.getFileName(),
                           juce::dontSendNotification);
}

void MainComponent::moveSampleToBucket (std::size_t sampleId, samplebench::BucketId targetBucket)
{
    if (! pack.moveSampleToBucket (sampleId, targetBucket))
        return;

    markPackDirty();
    currentBucket = targetBucket;
    refreshBucketButtons();
    refreshSampleList();

    const auto& samples = currentSamples();
    const auto found = std::find_if (samples.begin(), samples.end(), [sampleId] (const samplebench::Sample& sample)
    {
        return sample.id == sampleId;
    });

    if (found != samples.end())
    {
        const auto row = static_cast<int> (std::distance (samples.begin(), found));
        pack.selectSample (sampleId);
        sampleList.selectRow (row);
    }

    refreshBenchView();
}

void MainComponent::selectBucket (samplebench::BucketId bucket)
{
    currentBucket = bucket;
    sampleList.deselectAllRows();
    pack.clearSelection();
    refreshBucketButtons();
    refreshSampleList();
    refreshBenchView();
}

void MainComponent::refreshBucketButtons()
{
    const auto& buckets = pack.buckets();
    for (std::size_t index = 0; index < bucketButtons.size(); ++index)
    {
        const auto count = pack.samplesInBucket (buckets[index].id).size();
        auto& button = bucketButtons[index];
        const auto label = juce::String (buckets[index].folderName) + "  " + juce::String (static_cast<int> (count));
        button.setButtonText (label);
        button.setColour (juce::TextButton::buttonColourId,
                          buckets[index].id == currentBucket
                              ? colour (samplebench::palette::accent)
                              : colour (samplebench::palette::button));
        button.setColour (juce::TextButton::textColourOffId,
                          buckets[index].id == currentBucket
                              ? colour (samplebench::palette::inverseText)
                              : colour (samplebench::palette::text));
    }
}

void MainComponent::refreshSampleList()
{
    sampleList.updateContent();
    sampleList.repaint();
}

void MainComponent::refreshBenchView()
{
    const auto selected = selectedSample();
    loadingControls = true;
    const auto hasSample = selected.has_value();
    setSampleWorkbenchVisible (hasSample);
    setBenchControlsEnabled (hasSample);

    if (! hasSample)
    {
        clearNoSamplePreviewState();
        emptyBenchTitleLabel.setText ("No sample selected", juce::dontSendNotification);
        auto body = juce::String { "Import audio or select a sample from a bucket to open the workbench." };
        if (! currentSamples().empty())
            body += juce::newLine + "Select a sample from " + bucketLabel (currentBucket) + ".";
        emptyBenchLabel.setText (body, juce::dontSendNotification);
        sourceNameLabel.setText ("Source: -", juce::dontSendNotification);
        sourceMetadataLabel.setText ("", juce::dontSendNotification);
        sourceBucketLabel.setText ("-", juce::dontSendNotification);
        waveformOverview.clear();
        sourcePeaks.clear();
        sourceDurationSeconds = 0.0;
        sourceChannelCount = 0;
        fxChain.clear();
        selectedFxSlot = -1;
        setRenderActionsAvailable (false);
        loadingControls = false;
        refreshFilenamePreview();
        resized();
        return;
    }

    const auto& sample = *selected;
    const auto& settings = sample.bench;
    if (settings.customFxModules)
        fxChain = samplebench::resolvePluginModuleAvailability (settings.fxModules, pluginRegistryCache.foundPlugins);
    else if (settings.customEffectChain)
    {
        fxChain.clear();
        for (const auto effect : settings.effectChain)
            fxChain.push_back (samplebench::makeBuiltInFxModule (effect));
    }
    else
    {
        fxChain.clear();
    }
    if (selectedFxSlot < 0 || selectedFxSlot >= static_cast<int> (fxChain.size()))
        selectedFxSlot = fxChain.empty() ? -1 : 0;
    if (const auto* selectedModule = selectedFxModule(); selectedModule != nullptr && selectedModule->kind == samplebench::FxModuleKind::builtIn)
        selectedFx = selectedModule->builtIn;

    sourceNameLabel.setText (juce::String (samplebench::middleTruncatePreservingEnding (sample.displayName, 52)),
                             juce::dontSendNotification);
    sourceNameLabel.setTooltip (sample.displayName);
    sourceBucketLabel.setText (bucketLabel (sample.bucket), juce::dontSendNotification);
    typeSelector.setSelectedId (settings.type == samplebench::SampleType::oneShot ? 1
                               : settings.type == samplebench::SampleType::loop ? 2
                               : 3, juce::dontSendNotification);
    musicalBpmEditor.setText (settings.musicalBpm > 0.0 ? juce::String (settings.musicalBpm, 1) : "", false);
    barsEditor.setText (settings.bars > 0 ? juce::String (settings.bars) : "", false);
    keyEditor.setText (settings.key, false);
    captureStartBarEditor.setText (juce::String (settings.capture.captureStartBar), false);
    warmupBarsEditor.setText (settings.capture.warmupBars == 0 ? "Off" : juce::String (settings.capture.warmupBars), false);
    keepBarsEditor.setText (juce::String (settings.capture.keepBars), false);
    tailBarsEditor.setText (settings.capture.tailBars == 0 ? "Off" : juce::String (settings.capture.tailBars), false);
    sourceBedMode = settings.sourceBedMode;
    bedLengthBarsEditor.setText (juce::String (settings.bedLengthBars), false);
    bedTriggerSelector.setSelectedId (settings.bedTriggerMode == samplebench::BedTriggerMode::oncePerBar ? 2 : 1,
                                      juce::dontSendNotification);
    nameEditor.setText (settings.name.empty() ? sample.shortName : settings.name, false);
    flavorSelector.setSelectedId (settings.flavor == samplebench::RenderFlavor::dry ? 1 : 2, juce::dontSendNotification);
    versionEditor.setText (juce::String (settings.version), false);
    speedTrickToggle.setToggleState (settings.speedTrickEnabled, juce::dontSendNotification);
    gainEnabled = settings.gainEnabled;
    gainSlider.setValue (settings.gainDecibels, juce::dontSendNotification);
    normalizeToggle.setToggleState (settings.normalizeEnabled, juce::dontSendNotification);
    normalizeTargetKnob.setValue (settings.normalizeTargetDecibels, juce::dontSendNotification);
    monoToggle.setToggleState (settings.monoEnabled, juce::dontSendNotification);
    monoToggle.setButtonText (settings.monoEnabled ? "Mono On" : "Mono Off");
    limitEnabled = settings.limitEnabled;
    limitCeilingDecibels = settings.limitCeilingDecibels;
    limitInputDecibels = settings.limitInputDecibels;
    limitReleaseMs = settings.limitReleaseMs;
    compressorEnabled = settings.compressorEnabled;
    compressorThresholdDecibels = settings.compressorThresholdDecibels;
    compressorRatio = settings.compressorRatio;
    compressorAttackMs = settings.compressorAttackMs;
    compressorReleaseMs = settings.compressorReleaseMs;
    compressorMakeupDecibels = settings.compressorMakeupDecibels;
    compressorMix = settings.compressorMix;
    crushEnabled = settings.crushEnabled;
    crushBitsKnob.setValue (settings.crushBits, juce::dontSendNotification);
    const auto rateIndex = settings.crushSampleRate <= 0.0 ? 0
                         : settings.crushSampleRate >= 40000.0 ? 1
                         : settings.crushSampleRate >= 30000.0 ? 2
                         : settings.crushSampleRate >= 20000.0 ? 3
                         : settings.crushSampleRate >= 15000.0 ? 4
                         : 5;
    crushRateKnob.setValue (rateIndex, juce::dontSendNotification);
    crushMixKnob.setValue (settings.crushMix * 100.0f, juce::dontSendNotification);
    filterEnabled = settings.filterEnabled;
    filterMode = settings.filterMode;
    filterCutoffKnob.setValue (settings.filterCutoffHz, juce::dontSendNotification);
    filterResonanceKnob.setValue (settings.filterResonance, juce::dontSendNotification);
    driveEnabled = settings.driveEnabled;
    driveAmount = settings.driveAmount;
    driveTone = settings.driveTone;
    driveMix = settings.driveMix;
    driveOutputDecibels = settings.driveOutputDecibels;
    eqEnabled = settings.eqEnabled;
    eqLowDecibels = settings.eqLowDecibels;
    eqMidDecibels = settings.eqMidDecibels;
    eqHighDecibels = settings.eqHighDecibels;
    delayEnabled = settings.delayEnabled;
    delayDivision = settings.delayDivision;
    delayFeedback = settings.delayFeedback;
    delayMix = settings.delayMix;
    delayTone = settings.delayTone;
    reverbEnabled = settings.reverbEnabled;
    reverbSize = settings.reverbSize;
    reverbDecaySeconds = settings.reverbDecaySeconds;
    reverbMix = settings.reverbMix;
    reverbTone = settings.reverbTone;
    tapeEnabled = settings.tapeEnabled;
    tapeDrive = settings.tapeDrive;
    tapeWobble = settings.tapeWobble;
    tapeTone = settings.tapeTone;
    tapeNoise = settings.tapeNoise;
    tapeMix = settings.tapeMix;
    refreshFxControls();

    loadingControls = false;
    refreshSourceMetadata (sample);
    loadWaveformForSample (sample);
    setRenderActionsAvailable (temporaryRenderFile.existsAsFile());
    refreshSourceBedControls();
    suppressPackDirty = true;
    refreshFilenamePreview();
    suppressPackDirty = false;
}

void MainComponent::refreshFilenamePreview()
{
    if (loadingControls)
        return;

    const auto sample = selectedSample();
    if (! sample.has_value())
    {
        filenamePreviewLabel.setText ("Filename: -", juce::dontSendNotification);
        return;
    }

    const auto settings = settingsFromControls();
    pack.updateBenchSettings (sample->id, settings);
    markPackDirty();
    waveformOverview.setBenchSettings (settings);
    if (sourceDurationSeconds > 0.0 && ! playbackState.isPlaying)
    {
        const auto regions = samplebench::calculateCaptureRegions (settings.musicalBpm, settings.capture);
        sourceVisibleRange = samplebench::makeInitialVisibleWindow (sourceDurationSeconds,
                                                                    regions.keep.startSeconds,
                                                                    regions.bounce.endSeconds - regions.bounce.startSeconds);
    }
    const auto filename = samplebench::buildFinalFilename (settings);
    filenamePreviewLabel.setText (juce::String (samplebench::middleTruncatePreservingEnding (filename, 46)),
                                  juce::dontSendNotification);
    filenamePreviewLabel.setTooltip (filename);
    refreshCaptureChips();
    refreshFxControls();
    refreshSourceBedControls();
}

void MainComponent::refreshCaptureChips()
{
    const auto warmup = std::max (0, intFromEditor (warmupBarsEditor, 0));
    const auto keep = std::max (1, intFromEditor (keepBarsEditor, 4));
    const auto tail = std::max (0, intFromEditor (tailBarsEditor, 0));
    const std::array<int, 5> warmupValues { 0, 1, 4, 8, 16 };
    const std::array<int, 5> keepValues { 1, 2, 4, 8, 16 };
    const std::array<int, 4> tailValues { 0, 1, 2, 4 };

    for (std::size_t index = 0; index < warmupQuickButtons.size(); ++index)
        configureChip (warmupQuickButtons[index], warmup == warmupValues[index]);

    for (std::size_t index = 0; index < keepQuickButtons.size(); ++index)
        configureChip (keepQuickButtons[index], keep == keepValues[index]);

    for (std::size_t index = 0; index < tailQuickButtons.size(); ++index)
        configureChip (tailQuickButtons[index], tail == tailValues[index]);
}

void MainComponent::setBenchControlsEnabled (bool enabled)
{
    for (auto* component : { static_cast<juce::Component*> (&typeSelector),
                             static_cast<juce::Component*> (&musicalBpmEditor),
                             static_cast<juce::Component*> (&barsEditor),
                             static_cast<juce::Component*> (&keyEditor),
                             static_cast<juce::Component*> (&playSourceButton),
                             static_cast<juce::Component*> (&captureStartBarEditor),
                             static_cast<juce::Component*> (&warmupBarsEditor),
                             static_cast<juce::Component*> (&warmupMinusButton),
                             static_cast<juce::Component*> (&warmupPlusButton),
                             static_cast<juce::Component*> (&keepBarsEditor),
                             static_cast<juce::Component*> (&keepMinusButton),
                             static_cast<juce::Component*> (&keepPlusButton),
                             static_cast<juce::Component*> (&tailBarsEditor),
                             static_cast<juce::Component*> (&tailMinusButton),
                             static_cast<juce::Component*> (&tailPlusButton),
                             static_cast<juce::Component*> (&returnToStartButton),
                             static_cast<juce::Component*> (&playPreviewButton),
                             static_cast<juce::Component*> (&stopPreviewButton),
                             static_cast<juce::Component*> (&loopPreviewToggle),
                             static_cast<juce::Component*> (&sourceTargetButton),
                             static_cast<juce::Component*> (&bounceTargetButton),
                             static_cast<juce::Component*> (&sourceBedAsIsButton),
                             static_cast<juce::Component*> (&sourceBedExtendButton),
                             static_cast<juce::Component*> (&bedTriggerSelector),
                             static_cast<juce::Component*> (&bedLengthBarsEditor),
                             static_cast<juce::Component*> (&bedLengthMinusButton),
                             static_cast<juce::Component*> (&bedLengthPlusButton),
                             static_cast<juce::Component*> (&gainModuleCard),
                             static_cast<juce::Component*> (&monoModuleCard),
                             static_cast<juce::Component*> (&normalizeModuleCard),
                             static_cast<juce::Component*> (&crushModuleCard),
                             static_cast<juce::Component*> (&filterModuleCard),
                             static_cast<juce::Component*> (&limitModuleCard),
                             static_cast<juce::Component*> (&compressorModuleCard),
                             static_cast<juce::Component*> (&driveModuleCard),
                             static_cast<juce::Component*> (&eqModuleCard),
                             static_cast<juce::Component*> (&delayModuleCard),
                             static_cast<juce::Component*> (&reverbModuleCard),
                             static_cast<juce::Component*> (&tapeModuleCard),
                             static_cast<juce::Component*> (&addFxButton),
                             static_cast<juce::Component*> (&moveFxLeftButton),
                             static_cast<juce::Component*> (&moveFxRightButton),
                             static_cast<juce::Component*> (&removeFxButton),
                             static_cast<juce::Component*> (&fxPowerToggle),
                             static_cast<juce::Component*> (&fxResetButton),
                             static_cast<juce::Component*> (&openPluginEditorButton),
                             static_cast<juce::Component*> (&gainSlider),
                             static_cast<juce::Component*> (&normalizeToggle),
                             static_cast<juce::Component*> (&normalizeTargetKnob),
                             static_cast<juce::Component*> (&monoToggle),
                             static_cast<juce::Component*> (&crushBitsKnob),
                             static_cast<juce::Component*> (&crushRateKnob),
                             static_cast<juce::Component*> (&crushMixKnob),
                             static_cast<juce::Component*> (&filterLpButton),
                             static_cast<juce::Component*> (&filterHpButton),
                             static_cast<juce::Component*> (&filterCutoffKnob),
                             static_cast<juce::Component*> (&filterResonanceKnob),
                             static_cast<juce::Component*> (&genericFxKnobA),
                             static_cast<juce::Component*> (&genericFxKnobB),
                             static_cast<juce::Component*> (&genericFxKnobC),
                             static_cast<juce::Component*> (&genericFxKnobD),
                             static_cast<juce::Component*> (&genericFxKnobE),
                             static_cast<juce::Component*> (&genericFxKnobF),
                             static_cast<juce::Component*> (&nameEditor),
                             static_cast<juce::Component*> (&flavorSelector),
                             static_cast<juce::Component*> (&versionEditor),
                             static_cast<juce::Component*> (&speedTrickToggle),
                             static_cast<juce::Component*> (&renderPreviewButton) })
    {
        component->setEnabled (enabled);
    }

    for (auto& button : keepQuickButtons)
        button.setEnabled (enabled);

    for (auto& button : warmupQuickButtons)
        button.setEnabled (enabled);

    for (auto& button : tailQuickButtons)
        button.setEnabled (enabled);

    startMinusButton.setEnabled (enabled);
    startPlusButton.setEnabled (enabled);
    sourceKeepScopeButton.setEnabled (enabled);
    sourceFullScopeButton.setEnabled (enabled);

    if (! enabled)
        setRenderActionsAvailable (false);
}

void MainComponent::setSampleWorkbenchVisible (bool visible)
{
    benchContent.setSampleWorkbenchVisible (visible);

    for (auto* component : { static_cast<juce::Component*> (&benchTitleLabel),
                             static_cast<juce::Component*> (&sourceNameLabel),
                             static_cast<juce::Component*> (&sourceMetadataLabel),
                             static_cast<juce::Component*> (&sourceBucketLabel),
                             static_cast<juce::Component*> (&typeLabel),
                             static_cast<juce::Component*> (&bpmLabel),
                             static_cast<juce::Component*> (&barsFieldLabel),
                             static_cast<juce::Component*> (&keyFieldLabel),
                             static_cast<juce::Component*> (&typeSelector),
                             static_cast<juce::Component*> (&musicalBpmEditor),
                             static_cast<juce::Component*> (&barsEditor),
                             static_cast<juce::Component*> (&keyEditor),
                             static_cast<juce::Component*> (&playSourceButton),
                             static_cast<juce::Component*> (&waveformOverview),
                             static_cast<juce::Component*> (&captureTitleLabel),
                             static_cast<juce::Component*> (&returnToStartButton),
                             static_cast<juce::Component*> (&playPreviewButton),
                             static_cast<juce::Component*> (&stopPreviewButton),
                             static_cast<juce::Component*> (&loopPreviewToggle),
                             static_cast<juce::Component*> (&previewTimeLabel),
                             static_cast<juce::Component*> (&previewTargetLabel),
                             static_cast<juce::Component*> (&sourceTargetButton),
                             static_cast<juce::Component*> (&bounceTargetButton),
                             static_cast<juce::Component*> (&sourceBedLabel),
                             static_cast<juce::Component*> (&sourceBedAsIsButton),
                             static_cast<juce::Component*> (&sourceBedExtendButton),
                             static_cast<juce::Component*> (&bedTriggerLabel),
                             static_cast<juce::Component*> (&bedTriggerSelector),
                             static_cast<juce::Component*> (&bedLengthLabel),
                             static_cast<juce::Component*> (&bedLengthBarsEditor),
                             static_cast<juce::Component*> (&bedLengthMinusButton),
                             static_cast<juce::Component*> (&bedLengthPlusButton),
                             static_cast<juce::Component*> (&previewScopeLabel),
                             static_cast<juce::Component*> (&sourceKeepScopeButton),
                             static_cast<juce::Component*> (&sourceFullScopeButton),
                             static_cast<juce::Component*> (&captureStartLabel),
                             static_cast<juce::Component*> (&warmupLabel),
                             static_cast<juce::Component*> (&keepLabel),
                             static_cast<juce::Component*> (&tailLabel),
                             static_cast<juce::Component*> (&regionSummaryLabel),
                             static_cast<juce::Component*> (&captureStartBarEditor),
                             static_cast<juce::Component*> (&startMinusButton),
                             static_cast<juce::Component*> (&startPlusButton),
                             static_cast<juce::Component*> (&warmupBarsEditor),
                             static_cast<juce::Component*> (&warmupMinusButton),
                             static_cast<juce::Component*> (&warmupPlusButton),
                             static_cast<juce::Component*> (&keepBarsEditor),
                             static_cast<juce::Component*> (&keepMinusButton),
                             static_cast<juce::Component*> (&keepPlusButton),
                             static_cast<juce::Component*> (&tailBarsEditor),
                             static_cast<juce::Component*> (&tailMinusButton),
                             static_cast<juce::Component*> (&tailPlusButton),
                             static_cast<juce::Component*> (&fxTitleLabel),
                             static_cast<juce::Component*> (&gainModuleCard),
                             static_cast<juce::Component*> (&monoModuleCard),
                             static_cast<juce::Component*> (&normalizeModuleCard),
                             static_cast<juce::Component*> (&crushModuleCard),
                             static_cast<juce::Component*> (&filterModuleCard),
                             static_cast<juce::Component*> (&limitModuleCard),
                             static_cast<juce::Component*> (&compressorModuleCard),
                             static_cast<juce::Component*> (&driveModuleCard),
                             static_cast<juce::Component*> (&eqModuleCard),
                             static_cast<juce::Component*> (&delayModuleCard),
                             static_cast<juce::Component*> (&reverbModuleCard),
                             static_cast<juce::Component*> (&tapeModuleCard),
                             static_cast<juce::Component*> (&addFxButton),
                             static_cast<juce::Component*> (&moveFxLeftButton),
                             static_cast<juce::Component*> (&moveFxRightButton),
                             static_cast<juce::Component*> (&removeFxButton),
                             static_cast<juce::Component*> (&fxDetailTitleLabel),
                             static_cast<juce::Component*> (&fxParamLabelA),
                             static_cast<juce::Component*> (&fxParamLabelB),
                             static_cast<juce::Component*> (&fxParamLabelC),
                             static_cast<juce::Component*> (&fxParamLabelD),
                             static_cast<juce::Component*> (&fxParamLabelE),
                             static_cast<juce::Component*> (&fxParamLabelF),
                             static_cast<juce::Component*> (&fxPowerToggle),
                             static_cast<juce::Component*> (&fxResetButton),
                             static_cast<juce::Component*> (&openPluginEditorButton),
                             static_cast<juce::Component*> (&pluginStatusLabel),
                             static_cast<juce::Component*> (&gainSlider),
                             static_cast<juce::Component*> (&gainLabel),
                             static_cast<juce::Component*> (&normalizeToggle),
                             static_cast<juce::Component*> (&monoToggle),
                             static_cast<juce::Component*> (&normalizeTargetKnob),
                             static_cast<juce::Component*> (&crushBitsKnob),
                             static_cast<juce::Component*> (&crushRateKnob),
                             static_cast<juce::Component*> (&crushMixKnob),
                             static_cast<juce::Component*> (&filterCutoffKnob),
                             static_cast<juce::Component*> (&filterResonanceKnob),
                             static_cast<juce::Component*> (&genericFxKnobA),
                             static_cast<juce::Component*> (&genericFxKnobB),
                             static_cast<juce::Component*> (&genericFxKnobC),
                             static_cast<juce::Component*> (&genericFxKnobD),
                             static_cast<juce::Component*> (&genericFxKnobE),
                             static_cast<juce::Component*> (&genericFxKnobF),
                             static_cast<juce::Component*> (&filterLpButton),
                             static_cast<juce::Component*> (&filterHpButton),
                             static_cast<juce::Component*> (&fxHintLabel),
                             static_cast<juce::Component*> (&exportTitleLabel),
                             static_cast<juce::Component*> (&nameLabel),
                             static_cast<juce::Component*> (&flavorLabel),
                             static_cast<juce::Component*> (&versionLabel),
                             static_cast<juce::Component*> (&nameEditor),
                             static_cast<juce::Component*> (&flavorSelector),
                             static_cast<juce::Component*> (&versionEditor),
                             static_cast<juce::Component*> (&speedTrickToggle),
                             static_cast<juce::Component*> (&filenamePreviewLabel),
                             static_cast<juce::Component*> (&renderPreviewButton),
                             static_cast<juce::Component*> (&keepVariationButton),
                             static_cast<juce::Component*> (&trashRenderButton),
                             static_cast<juce::Component*> (&renderStatusLabel),
                             static_cast<juce::Component*> (&boundaryLabel) })
    {
        component->setVisible (visible);
    }

    if (! visible)
    {
        keepQuickLabel.setVisible (false);
        warmupQuickLabel.setVisible (false);
        tailQuickLabel.setVisible (false);
        for (auto& button : keepQuickButtons)
            button.setVisible (false);
        for (auto& button : warmupQuickButtons)
            button.setVisible (false);
        for (auto& button : tailQuickButtons)
            button.setVisible (false);
    }

    keyFieldLabel.setVisible (false);
    keyEditor.setVisible (false);
    playSourceButton.setVisible (false);
    emptyBenchTitleLabel.setVisible (! visible);
    emptyBenchLabel.setVisible (! visible);
    emptyBenchImportButton.setVisible (! visible);
    resized();
}

void MainComponent::clearNoSamplePreviewState()
{
    sourceTransport.stop();
    renderTransport.stop();
    sourceTransport.setSource (nullptr);
    renderTransport.setSource (nullptr);
    sourceReaderSource = nullptr;
    renderReaderSource = nullptr;

    if (temporaryRenderFile.existsAsFile())
        temporaryRenderFile.deleteFile();
    temporaryRenderFile = juce::File();
    pendingVariation = std::nullopt;
    bouncePeaks.clear();
    bounceDurationSeconds = 0.0;
    bounceChannelCount = 0;
    bounceSampleRate = 0.0;
    playbackState = samplebench::stopPlayback (playbackState);
    setMeterLevels (0.0f, 0.0f, false);
    renderStatusLabel.setText ("No Bounce", juce::dontSendNotification);
    transportStatusLabel.setText ("No Sample", juce::dontSendNotification);
}

void MainComponent::setRenderActionsAvailable (bool available)
{
    keepVariationButton.setEnabled (available);
    trashRenderButton.setEnabled (available);
    refreshTransportControls();
}

void MainComponent::timerCallback()
{
    refreshTransportControls();
    updateWaveformPlaybackView();
    updateMeterFromPlayback();

    if (playbackState.isPlaying
        && playbackState.loopEnabled
        && playbackState.target == samplebench::PlaybackTarget::source
        && sourcePreviewScope == SourcePreviewScope::keep)
    {
        const auto loop = currentSourcePreviewRegion();
        const auto position = sourceTransport.getCurrentPosition();
        const auto wrapped = samplebench::wrappedLoopPosition (position, loop);
        if (std::abs (wrapped - position) > 0.0001)
            sourceTransport.setPosition (wrapped);
        return;
    }

    if (! playbackState.isPlaying || playbackState.loopEnabled)
        return;

    const auto finished = playbackState.target == samplebench::PlaybackTarget::source
        ? sourceTransport.hasStreamFinished()
        : playbackState.target == samplebench::PlaybackTarget::bounce && renderTransport.hasStreamFinished();

    if (finished)
        stopPlayback();
}

void MainComponent::setKeepBars (int bars)
{
    keepBarsEditor.setText (juce::String (bars), juce::sendNotification);
}

void MainComponent::setWarmupBars (int bars)
{
    warmupBarsEditor.setText (bars <= 0 ? "Off" : juce::String (bars), juce::sendNotification);
}

void MainComponent::setTailBars (int bars)
{
    tailBarsEditor.setText (bars <= 0 ? "Off" : juce::String (bars), juce::sendNotification);
}

void MainComponent::nudgeStartBar (int delta)
{
    const auto current = std::max (1, intFromEditor (captureStartBarEditor, 1));
    captureStartBarEditor.setText (juce::String (std::max (1, current + delta)), juce::sendNotification);
}

void MainComponent::nudgeWarmupBars (int delta)
{
    const auto current = std::max (0, intFromEditor (warmupBarsEditor, 0));
    setWarmupBars (std::max (0, current + delta));
}

void MainComponent::nudgeKeepBars (int delta)
{
    const auto current = std::max (1, intFromEditor (keepBarsEditor, 4));
    keepBarsEditor.setText (juce::String (std::max (1, current + delta)), juce::sendNotification);
}

void MainComponent::nudgeTailBars (int delta)
{
    const auto current = std::max (0, intFromEditor (tailBarsEditor, 0));
    setTailBars (std::max (0, current + delta));
}

void MainComponent::nudgeBedLengthBars (int delta)
{
    const auto current = std::max (1, intFromEditor (bedLengthBarsEditor, 16));
    bedLengthBarsEditor.setText (juce::String (std::max (1, current + delta)), juce::sendNotification);
}

void MainComponent::selectSourcePreviewScope (SourcePreviewScope scope)
{
    sourcePreviewScope = scope;
    sourceTransport.setLooping (playbackState.loopEnabled
                                && playbackState.target == samplebench::PlaybackTarget::source
                                && sourcePreviewScope == SourcePreviewScope::full);
    if (playbackState.previewTarget == samplebench::PlaybackTarget::source)
        returnPreviewToStart();

    refreshTransportControls();
}

void MainComponent::selectSourceBedMode (samplebench::SourceBedMode mode)
{
    sourceBedMode = mode;
    auto settings = settingsFromControls();
    settings.sourceBedMode = mode;

    if (mode == samplebench::SourceBedMode::extendForFx)
    {
        if (settings.type == samplebench::SampleType::oneShot)
        {
            settings.bedLengthBars = std::max (settings.bedLengthBars, 8);
            settings.bedTriggerMode = samplebench::BedTriggerMode::oncePerBar;
            settings.capture.warmupBars = std::max (settings.capture.warmupBars, 4);
        }
        else
        {
            settings.bedLengthBars = std::max (settings.bedLengthBars, 16);
            settings.bedTriggerMode = samplebench::BedTriggerMode::loopContinuously;
        }
    }

    bedLengthBarsEditor.setText (juce::String (settings.bedLengthBars), false);
    bedTriggerSelector.setSelectedId (settings.bedTriggerMode == samplebench::BedTriggerMode::oncePerBar ? 2 : 1,
                                      juce::dontSendNotification);
    warmupBarsEditor.setText (settings.capture.warmupBars == 0 ? "Off" : juce::String (settings.capture.warmupBars), false);

    if (const auto sample = selectedSample())
        pack.updateBenchSettings (sample->id, settings);

    refreshSourceBedControls();
    if (const auto sample = selectedSample())
        loadWaveformForSample (*sample);
    refreshFilenamePreview();
}

void MainComponent::refreshSourceBedControls()
{
    const auto settings = settingsFromControls();
    const auto extend = settings.sourceBedMode == samplebench::SourceBedMode::extendForFx;

    sourceBedAsIsButton.setColour (juce::TextButton::buttonColourId,
                                   ! extend ? colour (samplebench::palette::accent)
                                            : colour (samplebench::palette::button));
    sourceBedExtendButton.setColour (juce::TextButton::buttonColourId,
                                     extend ? colour (samplebench::palette::accent)
                                            : colour (samplebench::palette::button));
    sourceBedAsIsButton.setColour (juce::TextButton::textColourOffId,
                                   ! extend ? colour (samplebench::palette::inverseText)
                                            : colour (samplebench::palette::text));
    sourceBedExtendButton.setColour (juce::TextButton::textColourOffId,
                                     extend ? colour (samplebench::palette::inverseText)
                                            : colour (samplebench::palette::text));
    bedTriggerSelector.setEnabled (extend);
    bedLengthBarsEditor.setEnabled (extend);
    bedLengthMinusButton.setEnabled (extend);
    bedLengthPlusButton.setEnabled (extend);
}

void MainComponent::selectFxModule (samplebench::BuiltInEffectId effect)
{
    selectedFx = effect;
    selectedFxSlot = selectedFxChainIndex();
    refreshFxControls();
}

void MainComponent::selectFxChainSlot (int slot)
{
    if (slot < 0 || slot >= static_cast<int> (fxChain.size()))
        return;

    selectedFxSlot = slot;
    if (fxChain[static_cast<std::size_t> (slot)].kind == samplebench::FxModuleKind::builtIn)
        selectedFx = fxChain[static_cast<std::size_t> (slot)].builtIn;
    refreshFxControls();
}

void MainComponent::showAddFxMenu()
{
    if (! selectedSample().has_value())
        return;

    pluginRegistryCache = samplebench::PluginRegistry::loadFromDisk (pluginRegistryCacheFile().getFullPathName().toStdString());

    juce::PopupMenu menu;
    const std::array<samplebench::BuiltInEffectId, 12> effects {
        samplebench::BuiltInEffectId::gain,
        samplebench::BuiltInEffectId::mono,
        samplebench::BuiltInEffectId::normalize,
        samplebench::BuiltInEffectId::limit,
        samplebench::BuiltInEffectId::compressor,
        samplebench::BuiltInEffectId::crush,
        samplebench::BuiltInEffectId::filter,
        samplebench::BuiltInEffectId::drive,
        samplebench::BuiltInEffectId::eq,
        samplebench::BuiltInEffectId::tape,
        samplebench::BuiltInEffectId::delay,
        samplebench::BuiltInEffectId::reverb
    };

    auto addEffectItem = [&] (int itemId, samplebench::BuiltInEffectId effect)
    {
        menu.addItem (itemId,
                      effectLabel (effect),
                      ! fxChainContains (effect));
    };

    std::vector<samplebench::CachedPluginDescription> pluginItems;
    for (const auto& plugin : pluginRegistryCache.foundPlugins)
        if (isPluginAvailableForMenu (plugin))
            pluginItems.push_back (plugin);

    juce::PopupMenu pluginsMenu;
    if (pluginItems.empty())
    {
        pluginsMenu.addItem (999, "No scanned VST3 effects. Open Settings > Plugins.", false);
    }
    else
    {
        std::sort (pluginItems.begin(), pluginItems.end(), [] (const auto& left, const auto& right)
        {
            return left.name < right.name;
        });

        for (std::size_t index = 0; index < pluginItems.size(); ++index)
        {
            auto label = juce::String::fromUTF8 (pluginItems[index].name.c_str());
            if (! pluginItems[index].manufacturer.empty())
                label += " - " + juce::String::fromUTF8 (pluginItems[index].manufacturer.c_str());
            pluginsMenu.addItem (1000 + static_cast<int> (index), label, fxChain.size() < 12);
        }
    }
    menu.addSubMenu ("Plugins", pluginsMenu);
    menu.addSeparator();
    menu.addSectionHeader ("UTILITY");
    addEffectItem (1, samplebench::BuiltInEffectId::gain);
    addEffectItem (2, samplebench::BuiltInEffectId::mono);
    addEffectItem (3, samplebench::BuiltInEffectId::normalize);
    addEffectItem (4, samplebench::BuiltInEffectId::limit);
    menu.addSeparator();
    menu.addSectionHeader ("DYNAMICS");
    addEffectItem (5, samplebench::BuiltInEffectId::compressor);
    menu.addSeparator();
    menu.addSectionHeader ("COLOR");
    addEffectItem (6, samplebench::BuiltInEffectId::crush);
    addEffectItem (7, samplebench::BuiltInEffectId::filter);
    addEffectItem (8, samplebench::BuiltInEffectId::drive);
    addEffectItem (9, samplebench::BuiltInEffectId::eq);
    addEffectItem (10, samplebench::BuiltInEffectId::tape);
    menu.addSeparator();
    menu.addSectionHeader ("SPACE");
    addEffectItem (11, samplebench::BuiltInEffectId::delay);
    addEffectItem (12, samplebench::BuiltInEffectId::reverb);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addFxButton),
                        [this, effects, pluginItems] (int result)
                        {
                            if (result > 0 && result <= static_cast<int> (effects.size()))
                            {
                                addFxModuleToChain (effects[static_cast<std::size_t> (result - 1)]);
                                return;
                            }

                            const auto pluginIndex = result - 1000;
                            if (pluginIndex >= 0 && pluginIndex < static_cast<int> (pluginItems.size()))
                                addPluginModuleToChain (pluginItems[static_cast<std::size_t> (pluginIndex)]);
                        });
}

void MainComponent::addFxModuleToChain (samplebench::BuiltInEffectId effect)
{
    if (! selectedSample().has_value())
        return;

    if (fxChainContains (effect) || fxChain.size() >= 12)
        return;

    fxChain.push_back (samplebench::makeBuiltInFxModule (effect));
    selectedFxSlot = static_cast<int> (fxChain.size()) - 1;
    selectedFx = effect;
    enableSelectedFxFromParameterEdit();
    handleFxControlsChanged();
    resized();
}

void MainComponent::addPluginModuleToChain (const samplebench::CachedPluginDescription& plugin)
{
    if (! selectedSample().has_value())
        return;

    if (fxChain.size() >= 12)
        return;

    auto module = samplebench::makePluginFxModule (plugin);
    juce::String error;
    auto instance = createPluginInstance (module.plugin, 44100.0, 512, error);
    if (instance == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Plugin could not be loaded",
                                                error.isNotEmpty() ? error : "The selected VST3 did not load.");
        return;
    }

    juce::MemoryBlock initialState;
    instance->getStateInformation (initialState);
    module.plugin.stateBlob.clear();
    if (initialState.getSize() > 0)
    {
        const auto* data = static_cast<const std::uint8_t*> (initialState.getData());
        module.plugin.stateBlob.assign (data, data + initialState.getSize());
    }
    fxChain.push_back (std::move (module));
    selectedFxSlot = static_cast<int> (fxChain.size()) - 1;
    handleFxControlsChanged();
    resized();
}

void MainComponent::removeSelectedFxModule()
{
    const auto selectedIndex = selectedFxChainIndex();
    if (selectedIndex < 0)
        return;

    fxChain.erase (fxChain.begin() + selectedIndex);
    selectedFxSlot = fxChain.empty() ? -1 : std::min<int> (selectedIndex, static_cast<int> (fxChain.size()) - 1);
    if (const auto* module = selectedFxModule(); module != nullptr && module->kind == samplebench::FxModuleKind::builtIn)
        selectedFx = module->builtIn;

    handleFxControlsChanged();
    resized();
}

void MainComponent::moveSelectedFxModule (int delta)
{
    const auto selectedIndex = selectedFxChainIndex();
    const auto nextIndex = selectedIndex + delta;
    if (selectedIndex < 0 || nextIndex < 0 || nextIndex >= static_cast<int> (fxChain.size()))
        return;

    std::swap (fxChain[static_cast<std::size_t> (selectedIndex)], fxChain[static_cast<std::size_t> (nextIndex)]);
    selectedFxSlot = nextIndex;
    handleFxControlsChanged();
    resized();
}

bool MainComponent::fxChainContains (samplebench::BuiltInEffectId effect) const
{
    return std::any_of (fxChain.begin(), fxChain.end(), [effect] (const auto& module)
    {
        return module.kind == samplebench::FxModuleKind::builtIn && module.builtIn == effect;
    });
}

int MainComponent::selectedFxChainIndex() const
{
    if (selectedFxSlot < 0 || selectedFxSlot >= static_cast<int> (fxChain.size()))
        return -1;

    return selectedFxSlot;
}

samplebench::FxModule* MainComponent::selectedFxModule()
{
    if (selectedFxSlot < 0 || selectedFxSlot >= static_cast<int> (fxChain.size()))
        return nullptr;

    return &fxChain[static_cast<std::size_t> (selectedFxSlot)];
}

const samplebench::FxModule* MainComponent::selectedFxModule() const
{
    if (selectedFxSlot < 0 || selectedFxSlot >= static_cast<int> (fxChain.size()))
        return nullptr;

    return &fxChain[static_cast<std::size_t> (selectedFxSlot)];
}

void MainComponent::resetSelectedFxModule()
{
    auto* module = selectedFxModule();
    if (module != nullptr && module->kind == samplebench::FxModuleKind::plugin)
    {
        module->plugin.enabled = true;
        module->plugin.stateBlob.clear();
        handleFxControlsChanged();
        return;
    }

    if (selectedFx == samplebench::BuiltInEffectId::gain)
    {
        gainEnabled = true;
        gainSlider.setValue (0.0, juce::dontSendNotification);
    }
    else if (selectedFx == samplebench::BuiltInEffectId::mono)
    {
        monoToggle.setToggleState (false, juce::dontSendNotification);
        monoToggle.setButtonText ("Mono Off");
    }
    else if (selectedFx == samplebench::BuiltInEffectId::normalize)
    {
        normalizeToggle.setToggleState (false, juce::dontSendNotification);
        normalizeTargetKnob.setValue (-1.0, juce::dontSendNotification);
    }
    else if (selectedFx == samplebench::BuiltInEffectId::limit)
    {
        limitEnabled = false;
        limitCeilingDecibels = -1.0f;
        limitInputDecibels = 0.0f;
        limitReleaseMs = 80.0f;
    }
    else if (selectedFx == samplebench::BuiltInEffectId::compressor)
    {
        compressorEnabled = false;
        compressorThresholdDecibels = -18.0f;
        compressorRatio = 4.0f;
        compressorAttackMs = 10.0f;
        compressorReleaseMs = 120.0f;
        compressorMakeupDecibels = 0.0f;
        compressorMix = 1.0f;
    }
    else if (selectedFx == samplebench::BuiltInEffectId::crush)
    {
        crushEnabled = false;
        crushBitsKnob.setValue (12.0, juce::dontSendNotification);
        crushRateKnob.setValue (3.0, juce::dontSendNotification);
        crushMixKnob.setValue (100.0, juce::dontSendNotification);
    }
    else if (selectedFx == samplebench::BuiltInEffectId::filter)
    {
        filterEnabled = false;
        filterMode = samplebench::FilterMode::lowPass;
        filterCutoffKnob.setValue (12000.0, juce::dontSendNotification);
        filterResonanceKnob.setValue (0.2, juce::dontSendNotification);
    }
    else if (selectedFx == samplebench::BuiltInEffectId::drive)
    {
        driveEnabled = false;
        driveAmount = 0.25f;
        driveTone = 0.5f;
        driveMix = 1.0f;
        driveOutputDecibels = 0.0f;
    }
    else if (selectedFx == samplebench::BuiltInEffectId::eq)
    {
        eqEnabled = false;
        eqLowDecibels = 0.0f;
        eqMidDecibels = 0.0f;
        eqHighDecibels = 0.0f;
    }
    else if (selectedFx == samplebench::BuiltInEffectId::delay)
    {
        delayEnabled = false;
        delayDivision = 2;
        delayFeedback = 0.25f;
        delayMix = 0.2f;
        delayTone = 0.35f;
    }
    else if (selectedFx == samplebench::BuiltInEffectId::reverb)
    {
        reverbEnabled = false;
        reverbSize = 0.35f;
        reverbDecaySeconds = 2.0f;
        reverbMix = 0.2f;
        reverbTone = 0.35f;
    }
    else if (selectedFx == samplebench::BuiltInEffectId::tape)
    {
        tapeEnabled = false;
        tapeDrive = 0.2f;
        tapeWobble = 0.1f;
        tapeTone = 0.35f;
        tapeNoise = 0.0f;
        tapeMix = 1.0f;
    }

    handleFxControlsChanged();
}

void MainComponent::handleFxControlsChanged()
{
    sourceFxPreviewCacheKey.clear();
    if (temporaryFxPreviewFile.existsAsFile())
        temporaryFxPreviewFile.deleteFile();

    refreshFxControls();
    refreshFilenamePreview();

    const auto sample = selectedSample();
    if (! sample.has_value() || loadingControls)
        return;

    const auto settings = settingsFromControls();
    pack.updateBenchSettings (sample->id, settings);
    loadWaveformForSample (*sample);

    if (playbackState.target == samplebench::PlaybackTarget::source && playbackState.isPlaying)
    {
        const auto previousPosition = sourceTransport.getCurrentPosition();
        loadTransportFile (sourceTransport, sourceReaderSource, sourcePlaybackFile (*sample, settings));
        auto nextPosition = std::max (0.0, previousPosition);
        if (playbackState.loopEnabled && sourcePreviewScope == SourcePreviewScope::keep)
            nextPosition = samplebench::wrappedLoopPosition (nextPosition, currentSourcePreviewRegion());

        sourceTransport.setPosition (nextPosition);
        sourceTransport.setLooping (playbackState.loopEnabled && sourcePreviewScope == SourcePreviewScope::full);
        sourceTransport.start();
    }
}

void MainComponent::handleFxParameterChanged()
{
    enableSelectedFxFromParameterEdit();

    if (fxParameterDragActive)
    {
        fxParameterChangedDuringDrag = true;
        refreshFxControls();
        return;
    }

    handleFxControlsChanged();
}

void MainComponent::beginFxParameterDrag()
{
    fxParameterDragActive = true;
    fxParameterChangedDuringDrag = false;
}

void MainComponent::endFxParameterDrag()
{
    if (! fxParameterDragActive)
        return;

    fxParameterDragActive = false;
    if (fxParameterChangedDuringDrag)
    {
        fxParameterChangedDuringDrag = false;
        handleFxControlsChanged();
    }
}

void MainComponent::enableSelectedFxFromParameterEdit()
{
    const auto* module = selectedFxModule();
    if (module != nullptr && module->kind != samplebench::FxModuleKind::builtIn)
        return;

    if (selectedFx == samplebench::BuiltInEffectId::gain)
        gainEnabled = true;
    else if (selectedFx == samplebench::BuiltInEffectId::mono)
        monoToggle.setToggleState (true, juce::dontSendNotification);
    else if (selectedFx == samplebench::BuiltInEffectId::normalize)
        normalizeToggle.setToggleState (true, juce::dontSendNotification);
    else if (selectedFx == samplebench::BuiltInEffectId::limit)
        limitEnabled = true;
    else if (selectedFx == samplebench::BuiltInEffectId::compressor)
        compressorEnabled = true;
    else if (selectedFx == samplebench::BuiltInEffectId::crush)
        crushEnabled = true;
    else if (selectedFx == samplebench::BuiltInEffectId::filter)
        filterEnabled = true;
    else if (selectedFx == samplebench::BuiltInEffectId::drive)
        driveEnabled = true;
    else if (selectedFx == samplebench::BuiltInEffectId::eq)
        eqEnabled = true;
    else if (selectedFx == samplebench::BuiltInEffectId::delay)
        delayEnabled = true;
    else if (selectedFx == samplebench::BuiltInEffectId::reverb)
        reverbEnabled = true;
    else if (selectedFx == samplebench::BuiltInEffectId::tape)
        tapeEnabled = true;
}

void MainComponent::openSelectedPluginEditor()
{
    const auto selectedIndex = selectedFxChainIndex();
    auto* module = selectedFxModule();
    if (module == nullptr || module->kind != samplebench::FxModuleKind::plugin)
        return;

    if (! module->plugin.enabled || module->plugin.status != samplebench::PluginModuleStatus::loaded)
        return;

    const auto sampleRate = audioDeviceManager.getCurrentAudioDevice() != nullptr
        ? audioDeviceManager.getCurrentAudioDevice()->getCurrentSampleRate()
        : 44100.0;
    juce::String error;
    auto instance = createPluginInstance (module->plugin, sampleRate, 512, error);
    if (instance == nullptr)
    {
        module->plugin.status = samplebench::PluginModuleStatus::failed;
        module->plugin.enabled = false;
        module->plugin.errorMessage = error.toStdString();
        handleFxControlsChanged();
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Plugin could not be opened",
                                                error.isNotEmpty() ? error : "The selected VST3 did not load.");
        return;
    }

    auto window = std::make_unique<PluginEditorWindow> (
        pluginName (module->plugin),
        std::move (instance),
        selectedIndex,
        [this] (int slot, const juce::MemoryBlock& state) { storePluginStateForSlot (slot, state); });
    window->setVisible (true);
    window->toFront (true);
    pluginEditorWindows.push_back (std::move (window));
}

void MainComponent::storePluginStateForSlot (int slot, const juce::MemoryBlock& state)
{
    if (slot < 0 || slot >= static_cast<int> (fxChain.size()))
        return;

    auto& module = fxChain[static_cast<std::size_t> (slot)];
    if (module.kind != samplebench::FxModuleKind::plugin)
        return;

    module.plugin.stateBlob.clear();
    if (state.getSize() > 0)
    {
        const auto* data = static_cast<const std::uint8_t*> (state.getData());
        module.plugin.stateBlob.assign (data, data + state.getSize());
    }
    if (shuttingDown)
        return;

    handleFxControlsChanged();
}

void MainComponent::openPluginSettings()
{
    if (pluginSettingsWindow == nullptr)
        pluginSettingsWindow = std::make_unique<PluginScanWindow> (pluginRegistryCacheFile());

    pluginSettingsWindow->setVisible (true);
    pluginSettingsWindow->toFront (true);
}

juce::File MainComponent::pluginRegistryCacheFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Bench Sampler")
        .getChildFile ("plugin_cache.txt");
}

void MainComponent::handleGenericFxKnobChanged()
{
    const auto* module = selectedFxModule();
    if (module != nullptr && module->kind != samplebench::FxModuleKind::builtIn)
        return;

    if (selectedFx == samplebench::BuiltInEffectId::limit)
    {
        limitCeilingDecibels = static_cast<float> (genericFxKnobA.getValue());
        limitInputDecibels = static_cast<float> (genericFxKnobB.getValue());
        limitReleaseMs = static_cast<float> (genericFxKnobC.getValue());
    }
    else if (selectedFx == samplebench::BuiltInEffectId::compressor)
    {
        compressorThresholdDecibels = static_cast<float> (genericFxKnobA.getValue());
        compressorRatio = static_cast<float> (genericFxKnobB.getValue());
        compressorAttackMs = static_cast<float> (genericFxKnobC.getValue());
        compressorReleaseMs = static_cast<float> (genericFxKnobD.getValue());
        compressorMakeupDecibels = static_cast<float> (genericFxKnobE.getValue());
        compressorMix = static_cast<float> (genericFxKnobF.getValue() / 100.0);
    }
    else if (selectedFx == samplebench::BuiltInEffectId::drive)
    {
        driveAmount = static_cast<float> (genericFxKnobA.getValue() / 100.0);
        driveTone = static_cast<float> (genericFxKnobB.getValue() / 100.0);
        driveMix = static_cast<float> (genericFxKnobC.getValue() / 100.0);
        driveOutputDecibels = static_cast<float> (genericFxKnobD.getValue());
    }
    else if (selectedFx == samplebench::BuiltInEffectId::eq)
    {
        eqLowDecibels = static_cast<float> (genericFxKnobA.getValue());
        eqMidDecibels = static_cast<float> (genericFxKnobB.getValue());
        eqHighDecibels = static_cast<float> (genericFxKnobC.getValue());
    }
    else if (selectedFx == samplebench::BuiltInEffectId::delay)
    {
        delayDivision = static_cast<int> (std::round (genericFxKnobA.getValue()));
        delayFeedback = static_cast<float> (genericFxKnobB.getValue() / 100.0);
        delayMix = static_cast<float> (genericFxKnobC.getValue() / 100.0);
        delayTone = static_cast<float> (genericFxKnobD.getValue() / 100.0);
    }
    else if (selectedFx == samplebench::BuiltInEffectId::reverb)
    {
        reverbSize = static_cast<float> (genericFxKnobA.getValue() / 100.0);
        reverbDecaySeconds = static_cast<float> (genericFxKnobB.getValue());
        reverbMix = static_cast<float> (genericFxKnobC.getValue() / 100.0);
        reverbTone = static_cast<float> (genericFxKnobD.getValue() / 100.0);
    }
    else if (selectedFx == samplebench::BuiltInEffectId::tape)
    {
        tapeDrive = static_cast<float> (genericFxKnobA.getValue() / 100.0);
        tapeWobble = static_cast<float> (genericFxKnobB.getValue() / 100.0);
        tapeTone = static_cast<float> (genericFxKnobC.getValue() / 100.0);
        tapeNoise = static_cast<float> (genericFxKnobD.getValue() / 100.0);
        tapeMix = static_cast<float> (genericFxKnobE.getValue() / 100.0);
    }

    handleFxParameterChanged();
}

void MainComponent::setGenericFxLabels (const juce::String& a,
                                        const juce::String& b,
                                        const juce::String& c,
                                        const juce::String& d,
                                        const juce::String& e,
                                        const juce::String& f)
{
    fxParamLabelA.setText (a, juce::dontSendNotification);
    fxParamLabelB.setText (b, juce::dontSendNotification);
    fxParamLabelC.setText (c, juce::dontSendNotification);
    fxParamLabelD.setText (d, juce::dontSendNotification);
    fxParamLabelE.setText (e, juce::dontSendNotification);
    fxParamLabelF.setText (f, juce::dontSendNotification);
}

void MainComponent::syncGenericFxControlsFromState()
{
    const auto* module = selectedFxModule();
    if (module != nullptr && module->kind != samplebench::FxModuleKind::builtIn)
        return;

    for (auto* knob : { &genericFxKnobA, &genericFxKnobB, &genericFxKnobC, &genericFxKnobD, &genericFxKnobE, &genericFxKnobF })
    {
        knob->setRange (0.0, 100.0, 1.0);
        knob->textFromValueFunction = [] (double value) { return juce::String (static_cast<int> (std::round (value))) + "%"; };
    }

    if (selectedFx == samplebench::BuiltInEffectId::limit)
    {
        setGenericFxLabels ("Ceiling", "Input", "Release");
        genericFxKnobA.setRange (-12.0, 0.0, 0.1);
        genericFxKnobB.setRange (0.0, 24.0, 0.1);
        genericFxKnobC.setRange (10.0, 500.0, 1.0);
        genericFxKnobA.setValue (limitCeilingDecibels, juce::dontSendNotification);
        genericFxKnobB.setValue (limitInputDecibels, juce::dontSendNotification);
        genericFxKnobC.setValue (limitReleaseMs, juce::dontSendNotification);
        genericFxKnobA.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " dB"; };
        genericFxKnobB.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " dB"; };
        genericFxKnobC.textFromValueFunction = [] (double value) { return juce::String (static_cast<int> (std::round (value))) + " ms"; };
    }
    else if (selectedFx == samplebench::BuiltInEffectId::compressor)
    {
        setGenericFxLabels ("Thresh", "Ratio", "Attack", "Release", "Makeup", "Mix");
        genericFxKnobA.setRange (-48.0, 0.0, 0.1);
        genericFxKnobB.setRange (1.0, 20.0, 0.1);
        genericFxKnobC.setRange (0.1, 100.0, 0.1);
        genericFxKnobD.setRange (20.0, 1000.0, 1.0);
        genericFxKnobE.setRange (0.0, 24.0, 0.1);
        genericFxKnobF.setRange (0.0, 100.0, 1.0);
        genericFxKnobA.setValue (compressorThresholdDecibels, juce::dontSendNotification);
        genericFxKnobB.setValue (compressorRatio, juce::dontSendNotification);
        genericFxKnobC.setValue (compressorAttackMs, juce::dontSendNotification);
        genericFxKnobD.setValue (compressorReleaseMs, juce::dontSendNotification);
        genericFxKnobE.setValue (compressorMakeupDecibels, juce::dontSendNotification);
        genericFxKnobF.setValue (compressorMix * 100.0f, juce::dontSendNotification);
        genericFxKnobA.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " dB"; };
        genericFxKnobB.textFromValueFunction = [] (double value) { return juce::String (value, 1) + ":1"; };
        genericFxKnobC.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " ms"; };
        genericFxKnobD.textFromValueFunction = [] (double value) { return juce::String (static_cast<int> (std::round (value))) + " ms"; };
        genericFxKnobE.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " dB"; };
    }
    else if (selectedFx == samplebench::BuiltInEffectId::drive)
    {
        setGenericFxLabels ("Drive", "Tone", "Mix", "Output");
        genericFxKnobD.setRange (-12.0, 12.0, 0.1);
        genericFxKnobA.setValue (driveAmount * 100.0f, juce::dontSendNotification);
        genericFxKnobB.setValue (driveTone * 100.0f, juce::dontSendNotification);
        genericFxKnobC.setValue (driveMix * 100.0f, juce::dontSendNotification);
        genericFxKnobD.setValue (driveOutputDecibels, juce::dontSendNotification);
        genericFxKnobD.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " dB"; };
    }
    else if (selectedFx == samplebench::BuiltInEffectId::eq)
    {
        setGenericFxLabels ("Low", "Mid", "High");
        genericFxKnobA.setRange (-12.0, 12.0, 0.1);
        genericFxKnobB.setRange (-12.0, 12.0, 0.1);
        genericFxKnobC.setRange (-12.0, 12.0, 0.1);
        genericFxKnobA.setValue (eqLowDecibels, juce::dontSendNotification);
        genericFxKnobB.setValue (eqMidDecibels, juce::dontSendNotification);
        genericFxKnobC.setValue (eqHighDecibels, juce::dontSendNotification);
        genericFxKnobA.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " dB"; };
        genericFxKnobB.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " dB"; };
        genericFxKnobC.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " dB"; };
    }
    else if (selectedFx == samplebench::BuiltInEffectId::delay)
    {
        setGenericFxLabels ("Time", "Feedback", "Mix", "Tone");
        genericFxKnobA.setRange (0.0, 4.0, 1.0);
        genericFxKnobA.setValue (delayDivision, juce::dontSendNotification);
        genericFxKnobB.setValue (delayFeedback * 100.0f, juce::dontSendNotification);
        genericFxKnobC.setValue (delayMix * 100.0f, juce::dontSendNotification);
        genericFxKnobD.setValue (delayTone * 100.0f, juce::dontSendNotification);
        genericFxKnobA.textFromValueFunction = [] (double value)
        {
            const auto index = static_cast<int> (std::round (value));
            if (index == 0) return juce::String { "1/16" };
            if (index == 1) return juce::String { "1/8" };
            if (index == 2) return juce::String { "1/4" };
            if (index == 3) return juce::String { "1/2" };
            return juce::String { "1 bar" };
        };
    }
    else if (selectedFx == samplebench::BuiltInEffectId::reverb)
    {
        setGenericFxLabels ("Size", "Decay", "Mix", "Tone");
        genericFxKnobB.setRange (0.1, 8.0, 0.1);
        genericFxKnobA.setValue (reverbSize * 100.0f, juce::dontSendNotification);
        genericFxKnobB.setValue (reverbDecaySeconds, juce::dontSendNotification);
        genericFxKnobC.setValue (reverbMix * 100.0f, juce::dontSendNotification);
        genericFxKnobD.setValue (reverbTone * 100.0f, juce::dontSendNotification);
        genericFxKnobB.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " s"; };
    }
    else if (selectedFx == samplebench::BuiltInEffectId::tape)
    {
        setGenericFxLabels ("Drive", "Wobble", "Tone", "Noise", "Mix");
        genericFxKnobA.setValue (tapeDrive * 100.0f, juce::dontSendNotification);
        genericFxKnobB.setValue (tapeWobble * 100.0f, juce::dontSendNotification);
        genericFxKnobC.setValue (tapeTone * 100.0f, juce::dontSendNotification);
        genericFxKnobD.setValue (tapeNoise * 100.0f, juce::dontSendNotification);
        genericFxKnobE.setValue (tapeMix * 100.0f, juce::dontSendNotification);
    }

}

void MainComponent::refreshFxControls()
{
    if (! selectedSample().has_value())
    {
        for (auto* component : { static_cast<juce::Component*> (&gainModuleCard),
                                 static_cast<juce::Component*> (&monoModuleCard),
                                 static_cast<juce::Component*> (&normalizeModuleCard),
                                 static_cast<juce::Component*> (&limitModuleCard),
                                 static_cast<juce::Component*> (&compressorModuleCard),
                                 static_cast<juce::Component*> (&crushModuleCard),
                                 static_cast<juce::Component*> (&filterModuleCard),
                                 static_cast<juce::Component*> (&driveModuleCard),
                                 static_cast<juce::Component*> (&eqModuleCard),
                                 static_cast<juce::Component*> (&delayModuleCard),
                                 static_cast<juce::Component*> (&reverbModuleCard),
                                 static_cast<juce::Component*> (&tapeModuleCard),
                                 static_cast<juce::Component*> (&addFxButton),
                                 static_cast<juce::Component*> (&moveFxLeftButton),
                                 static_cast<juce::Component*> (&moveFxRightButton),
                                 static_cast<juce::Component*> (&removeFxButton),
                                 static_cast<juce::Component*> (&fxDetailTitleLabel),
                                 static_cast<juce::Component*> (&fxPowerToggle),
                                 static_cast<juce::Component*> (&fxResetButton),
                                 static_cast<juce::Component*> (&openPluginEditorButton),
                                 static_cast<juce::Component*> (&pluginStatusLabel),
                                 static_cast<juce::Component*> (&gainLabel),
                                 static_cast<juce::Component*> (&gainSlider),
                                 static_cast<juce::Component*> (&monoToggle),
                                 static_cast<juce::Component*> (&normalizeToggle),
                                 static_cast<juce::Component*> (&normalizeTargetKnob),
                                 static_cast<juce::Component*> (&crushBitsKnob),
                                 static_cast<juce::Component*> (&crushRateKnob),
                                 static_cast<juce::Component*> (&crushMixKnob),
                                 static_cast<juce::Component*> (&filterLpButton),
                                 static_cast<juce::Component*> (&filterHpButton),
                                 static_cast<juce::Component*> (&filterCutoffKnob),
                                 static_cast<juce::Component*> (&filterResonanceKnob),
                                 static_cast<juce::Component*> (&genericFxKnobA),
                                 static_cast<juce::Component*> (&genericFxKnobB),
                                 static_cast<juce::Component*> (&genericFxKnobC),
                                 static_cast<juce::Component*> (&genericFxKnobD),
                                 static_cast<juce::Component*> (&genericFxKnobE),
                                 static_cast<juce::Component*> (&genericFxKnobF),
                                 static_cast<juce::Component*> (&fxHintLabel),
                                 static_cast<juce::Component*> (&fxParamLabelA),
                                 static_cast<juce::Component*> (&fxParamLabelB),
                                 static_cast<juce::Component*> (&fxParamLabelC),
                                 static_cast<juce::Component*> (&fxParamLabelD),
                                 static_cast<juce::Component*> (&fxParamLabelE),
                                 static_cast<juce::Component*> (&fxParamLabelF) })
            component->setVisible (false);

        addFxButton.setEnabled (false);
        moveFxLeftButton.setEnabled (false);
        moveFxRightButton.setEnabled (false);
        removeFxButton.setEnabled (false);
        return;
    }

    if (selectedFxSlot < 0 || selectedFxSlot >= static_cast<int> (fxChain.size()))
        selectedFxSlot = fxChain.empty() ? -1 : 0;

    const auto* selectedModule = selectedFxModule();
    const auto selectedIsBuiltIn = selectedModule != nullptr && selectedModule->kind == samplebench::FxModuleKind::builtIn;
    const auto selectedIsPlugin = selectedModule != nullptr && selectedModule->kind == samplebench::FxModuleKind::plugin;
    if (selectedIsBuiltIn)
        selectedFx = selectedModule->builtIn;

    const auto gainSummary = formatSignedDb (static_cast<float> (gainSlider.getValue()));
    const auto monoOn = monoToggle.getToggleState();
    const auto normalizeOn = normalizeToggle.getToggleState();
    const auto crushBits = static_cast<int> (std::round (crushBitsKnob.getValue()));
    const auto crushRateText = crushRateKnob.getTextFromValue (crushRateKnob.getValue());
    const auto filterCutoffText = filterCutoffKnob.getTextFromValue (filterCutoffKnob.getValue());
    const auto delayDivisionText = [] (int division)
    {
        if (division == 0)
            return juce::String { "1/16" };
        if (division == 1)
            return juce::String { "1/8" };
        if (division == 2)
            return juce::String { "1/4" };
        if (division == 3)
            return juce::String { "1/2" };
        return juce::String { "1 bar" };
    };
    const auto summaryForEffect = [&] (samplebench::BuiltInEffectId effect)
    {
        if (effect == samplebench::BuiltInEffectId::gain)
            return gainSummary;
        if (effect == samplebench::BuiltInEffectId::mono)
            return monoOn ? juce::String { "Mono On" } : juce::String { "Stereo" };
        if (effect == samplebench::BuiltInEffectId::normalize)
            return normalizeTargetKnob.getTextFromValue (normalizeTargetKnob.getValue());
        if (effect == samplebench::BuiltInEffectId::limit)
            return juce::String (limitCeilingDecibels, 1) + " dB ceil";
        if (effect == samplebench::BuiltInEffectId::compressor)
            return juce::String (compressorRatio, 1) + ":1 / " + juce::String (compressorThresholdDecibels, 0) + " dB";
        if (effect == samplebench::BuiltInEffectId::crush)
            return juce::String (crushBits) + "-bit / " + crushRateText;
        if (effect == samplebench::BuiltInEffectId::filter)
            return juce::String (filterMode == samplebench::FilterMode::highPass ? "HP " : "LP ") + filterCutoffText;
        if (effect == samplebench::BuiltInEffectId::drive)
            return juce::String ("Soft ") + juce::String (static_cast<int> (std::round (driveAmount * 100.0f))) + "%";
        if (effect == samplebench::BuiltInEffectId::eq)
            return juce::String ("L") + juce::String (eqLowDecibels, 0) + " M" + juce::String (eqMidDecibels, 0) + " H" + juce::String (eqHighDecibels, 0);
        if (effect == samplebench::BuiltInEffectId::delay)
            return delayDivisionText (delayDivision) + " " + juce::String (static_cast<int> (std::round (delayFeedback * 100.0f))) + "%";
        if (effect == samplebench::BuiltInEffectId::reverb)
            return juce::String (reverbDecaySeconds, 1) + "s " + juce::String (static_cast<int> (std::round (reverbMix * 100.0f))) + "%";
        return juce::String ("Drive ") + juce::String (static_cast<int> (std::round (tapeDrive * 100.0f))) + " / Wob " + juce::String (static_cast<int> (std::round (tapeWobble * 100.0f)));
    };
    const auto powerForEffect = [&] (samplebench::BuiltInEffectId effect)
    {
        if (effect == samplebench::BuiltInEffectId::gain)
            return gainEnabled;
        if (effect == samplebench::BuiltInEffectId::mono)
            return monoOn;
        if (effect == samplebench::BuiltInEffectId::normalize)
            return normalizeOn;
        if (effect == samplebench::BuiltInEffectId::limit)
            return limitEnabled;
        if (effect == samplebench::BuiltInEffectId::compressor)
            return compressorEnabled;
        if (effect == samplebench::BuiltInEffectId::crush)
            return crushEnabled;
        if (effect == samplebench::BuiltInEffectId::filter)
            return filterEnabled;
        if (effect == samplebench::BuiltInEffectId::drive)
            return driveEnabled;
        if (effect == samplebench::BuiltInEffectId::eq)
            return eqEnabled;
        if (effect == samplebench::BuiltInEffectId::delay)
            return delayEnabled;
        if (effect == samplebench::BuiltInEffectId::reverb)
            return reverbEnabled;
        return tapeEnabled;
    };
    const auto summaryForModule = [&] (const samplebench::FxModule& module)
    {
        if (module.kind == samplebench::FxModuleKind::builtIn)
            return summaryForEffect (module.builtIn);

        if (module.plugin.status == samplebench::PluginModuleStatus::missing)
            return juce::String { "Missing" };
        if (module.plugin.status == samplebench::PluginModuleStatus::failed)
            return juce::String { "Failed" };
        return module.plugin.manufacturer.empty()
            ? juce::String { "VST3" }
            : juce::String::fromUTF8 (module.plugin.manufacturer.c_str());
    };

    std::array<FxModuleCard*, 12> cards { &gainModuleCard, &monoModuleCard, &normalizeModuleCard, &limitModuleCard, &compressorModuleCard,
                                          &crushModuleCard, &filterModuleCard, &driveModuleCard, &eqModuleCard,
                                          &delayModuleCard, &reverbModuleCard, &tapeModuleCard };
    for (std::size_t index = 0; index < cards.size(); ++index)
    {
        const auto visible = index < fxChain.size();
        cards[index]->setVisible (visible);
        if (! visible)
            continue;

        const auto& module = fxChain[index];
        cards[index]->setModuleName (moduleLabel (module));
        cards[index]->setCardState (moduleIsPowered (module,
                                                     gainEnabled,
                                                     monoOn,
                                                     normalizeOn,
                                                     limitEnabled,
                                                     compressorEnabled,
                                                     crushEnabled,
                                                     filterEnabled,
                                                     driveEnabled,
                                                     eqEnabled,
                                                     delayEnabled,
                                                     reverbEnabled,
                                                     tapeEnabled),
                                    selectedFxSlot == static_cast<int> (index),
                                    summaryForModule (module));
    }

    const auto hasSelectedFx = selectedModule != nullptr;
    const auto showGain = selectedIsBuiltIn && selectedFx == samplebench::BuiltInEffectId::gain;
    const auto showMono = selectedIsBuiltIn && selectedFx == samplebench::BuiltInEffectId::mono;
    const auto showNormalize = selectedIsBuiltIn && selectedFx == samplebench::BuiltInEffectId::normalize;
    const auto showCrush = selectedIsBuiltIn && selectedFx == samplebench::BuiltInEffectId::crush;
    const auto showFilter = selectedIsBuiltIn && selectedFx == samplebench::BuiltInEffectId::filter;
    const auto showGeneric = selectedIsBuiltIn && isNewFxDetailEffect (selectedFx);
    if (showGeneric)
        syncGenericFxControlsFromState();

    gainLabel.setVisible (showGain);
    gainSlider.setVisible (showGain);
    monoToggle.setVisible (showMono);
    normalizeToggle.setVisible (showNormalize);
    normalizeTargetKnob.setVisible (showNormalize);
    crushBitsKnob.setVisible (showCrush);
    crushRateKnob.setVisible (showCrush);
    crushMixKnob.setVisible (showCrush);
    filterLpButton.setVisible (showFilter);
    filterHpButton.setVisible (showFilter);
    filterCutoffKnob.setVisible (showFilter);
    filterResonanceKnob.setVisible (showFilter);
    for (auto* knob : { &genericFxKnobA, &genericFxKnobB, &genericFxKnobC, &genericFxKnobD, &genericFxKnobE, &genericFxKnobF })
        knob->setVisible (showGeneric);
    openPluginEditorButton.setVisible (selectedIsPlugin);
    pluginStatusLabel.setVisible (selectedIsPlugin);
    fxHintLabel.setVisible (showNormalize || showCrush || showFilter || showGeneric || selectedIsPlugin);
    fxParamLabelA.setVisible (showNormalize || showCrush || showFilter || showGeneric);
    fxParamLabelB.setVisible (showCrush || showFilter || showGeneric);
    fxParamLabelC.setVisible (showCrush || showGeneric);
    fxParamLabelD.setVisible (showGeneric);
    fxParamLabelE.setVisible (showGeneric);
    fxParamLabelF.setVisible (showGeneric);

    juce::String title = fxChain.empty() ? "ADD FX" : "SELECT FX";
    bool power = false;
    if (showGain)
    {
        title = "GAIN";
        power = gainEnabled;
    }
    else if (showMono)
    {
        title = "MONO";
        power = monoOn;
    }
    else if (showNormalize)
    {
        title = "NORMALIZE";
        power = normalizeOn;
        fxParamLabelA.setText ("Target", juce::dontSendNotification);
        fxParamLabelB.setText ("", juce::dontSendNotification);
        fxParamLabelC.setText ("", juce::dontSendNotification);
    }
    else if (showCrush)
    {
        title = "CRUSH";
        power = crushEnabled;
        fxParamLabelA.setText ("Bits", juce::dontSendNotification);
        fxParamLabelB.setText ("Rate", juce::dontSendNotification);
        fxParamLabelC.setText ("Mix", juce::dontSendNotification);
    }
    else if (showFilter)
    {
        title = "FILTER";
        power = filterEnabled;
        fxParamLabelA.setText ("Cutoff", juce::dontSendNotification);
        fxParamLabelB.setText ("Res", juce::dontSendNotification);
        fxParamLabelC.setText ("", juce::dontSendNotification);
    }
    else if (showGeneric)
    {
        title = effectLabel (selectedFx);
        power = powerForEffect (selectedFx);
    }
    else if (selectedIsPlugin)
    {
        title = pluginName (selectedModule->plugin);
        power = selectedModule->plugin.enabled && selectedModule->plugin.status == samplebench::PluginModuleStatus::loaded;
        juce::String status = "VST3";
        if (! selectedModule->plugin.manufacturer.empty())
            status += "  " + juce::String::fromUTF8 (selectedModule->plugin.manufacturer.c_str());
        if (selectedModule->plugin.status == samplebench::PluginModuleStatus::missing)
            status += "  Missing - skipped";
        else if (selectedModule->plugin.status == samplebench::PluginModuleStatus::failed)
            status += "  Failed - skipped";
        else
            status += selectedModule->plugin.enabled ? "  Loaded" : "  Bypassed";
        pluginStatusLabel.setText (status, juce::dontSendNotification);
        pluginStatusLabel.setTooltip (juce::String::fromUTF8 (selectedModule->plugin.filePath.u8string().c_str()));
    }

    fxDetailTitleLabel.setText (title, juce::dontSendNotification);
    fxPowerToggle.setToggleState (power, juce::dontSendNotification);
    fxPowerToggle.setEnabled (hasSelectedFx);
    fxResetButton.setEnabled (hasSelectedFx);
    openPluginEditorButton.setEnabled (selectedIsPlugin
                                       && selectedModule->plugin.enabled
                                       && selectedModule->plugin.status == samplebench::PluginModuleStatus::loaded);
    monoToggle.setButtonText (monoOn ? "Mono On" : "Mono Off");
    normalizeToggle.setButtonText ("Normalize on Bounce");
    if (! showNormalize && ! showCrush && ! showFilter && ! showGeneric)
    {
        fxParamLabelA.setText ("", juce::dontSendNotification);
        fxParamLabelB.setText ("", juce::dontSendNotification);
        fxParamLabelC.setText ("", juce::dontSendNotification);
        fxParamLabelD.setText ("", juce::dontSendNotification);
        fxParamLabelE.setText ("", juce::dontSendNotification);
        fxParamLabelF.setText ("", juce::dontSendNotification);
    }
    const auto selectedIndex = selectedFxChainIndex();
    moveFxLeftButton.setEnabled (selectedIndex > 0);
    moveFxRightButton.setEnabled (selectedIndex >= 0 && selectedIndex < static_cast<int> (fxChain.size()) - 1);
    removeFxButton.setEnabled (selectedIndex >= 0);
    addFxButton.setEnabled (fxChain.size() < 12);
    addFxButton.setVisible (true);
    if (showNormalize)
        fxHintLabel.setText ("Render-only: Normalize is applied when creating the Bounce.", juce::dontSendNotification);
    else if (showCrush || showFilter || showGeneric)
        fxHintLabel.setText ("Applies to Source/Bed preview and Render Preview.", juce::dontSendNotification);
    else if (selectedIsPlugin)
        fxHintLabel.setText ("Plugin state is saved with this rack slot. Source/Bed preview and Bounce use this chain order.", juce::dontSendNotification);
    else
        fxHintLabel.setText ("Use + to add effects. Chain order is the processing order.", juce::dontSendNotification);
    configureChip (filterLpButton, filterMode == samplebench::FilterMode::lowPass);
    configureChip (filterHpButton, filterMode == samplebench::FilterMode::highPass);
    resized();
    repaint();
}

samplebench::CaptureRegions MainComponent::currentCaptureRegions() const
{
    const auto settings = settingsFromControls();
    return samplebench::calculateCaptureRegions (settings.musicalBpm, settings.capture);
}

samplebench::VisibleTimeRange MainComponent::currentSourcePreviewRegion() const
{
    if (sourcePreviewScope == SourcePreviewScope::full)
        return { 0.0, sourceDurationSeconds };

    return samplebench::sourcePreviewLoopRegion (currentCaptureRegions());
}

juce::String MainComponent::regionSummaryText() const
{
    const auto regions = currentCaptureRegions();
    const auto settings = settingsFromControls();
    const auto warmupText = regions.warmupBars > 0 ? juce::String (regions.warmupBars) + " bars"
                                                   : juce::String { "Off" };
    const auto tailText = regions.tailBars > 0 ? juce::String (regions.tailBars) + " bars"
                                               : juce::String { "Off" };
    const auto keepStartBar = regions.startBar;
    const auto keepEndBar = regions.startBar + regions.keepBars - 1;
    const auto bounceBars = regions.keepBars + regions.tailBars;
    const auto bounceSeconds = regions.bounce.endSeconds - regions.bounce.startSeconds;

    return "Render path: Warm-up " + warmupText
         + " -> Keep " + juce::String (regions.keepBars) + " bars"
         + " -> Tail " + tailText
         + (settings.sourceBedMode == samplebench::SourceBedMode::extendForFx
                ? "    Bed: " + juce::String (settings.bedLengthBars)
                    + " bars / " + formatTime (samplebench::calculateBedDurationSeconds (settings.musicalBpm,
                                                                                          settings.bedLengthBars))
                    + " " + (settings.bedTriggerMode == samplebench::BedTriggerMode::oncePerBar
                                ? "Once per bar"
                                : "Loop")
                : "")
         + "    Loop preview: "
         + (sourcePreviewScope == SourcePreviewScope::keep
                ? "Keep bars " + juce::String (keepStartBar) + "-" + juce::String (keepEndBar)
                : "Full Source")
         + "    Bounce: " + juce::String (bounceBars) + " bars / " + formatTime (bounceSeconds);
}

juce::File MainComponent::sourcePreviewFile (const samplebench::Sample& sample,
                                             const samplebench::BenchSettings& settings)
{
    if (settings.sourceBedMode != samplebench::SourceBedMode::extendForFx)
        return juce::File { sample.sourcePath.string() };

    return buildSourceBedFile (sample, settings);
}

juce::File MainComponent::buildSourceBedFile (const samplebench::Sample& sample,
                                              const samplebench::BenchSettings& settings)
{
    const juce::File sourceFile { sample.sourcePath.string() };
    const auto cacheKey = juce::String (sample.id) + "|"
                        + sourceFile.getFullPathName() + "|"
                        + juce::String (settings.musicalBpm, 4) + "|"
                        + juce::String (settings.bedLengthBars) + "|"
                        + juce::String (settings.bedTriggerMode == samplebench::BedTriggerMode::oncePerBar ? "bar" : "loop");
    if (temporarySourceBedFile.existsAsFile() && sourceBedCacheKey == cacheKey)
        return temporarySourceBedFile;

    std::unique_ptr<juce::AudioFormatReader> reader (audioFormatManager.createReaderFor (sourceFile));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return sourceFile;

    const auto channels = static_cast<int> (std::min<unsigned int> (reader->numChannels, 2));
    juce::AudioBuffer<float> sourceBuffer (channels, static_cast<int> (reader->lengthInSamples));
    sourceBuffer.clear();
    reader->read (&sourceBuffer, 0, sourceBuffer.getNumSamples(), 0, true, true);

    const auto bedDuration = samplebench::calculateBedDurationSeconds (settings.musicalBpm, settings.bedLengthBars);
    const auto totalSamples = static_cast<int> (std::max (1.0, std::round (bedDuration * reader->sampleRate)));
    juce::AudioBuffer<float> bedBuffer (channels, totalSamples);
    bedBuffer.clear();

    if (settings.bedTriggerMode == samplebench::BedTriggerMode::loopContinuously)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* dest = bedBuffer.getWritePointer (channel);
            const auto* source = sourceBuffer.getReadPointer (channel);
            for (int index = 0; index < totalSamples; ++index)
                dest[index] = source[index % sourceBuffer.getNumSamples()];
        }
    }
    else
    {
        const auto samplesPerBar = static_cast<int> (std::max (1.0, std::round (samplebench::calculateBedDurationSeconds (settings.musicalBpm, 1)
                                                                                * reader->sampleRate)));
        for (int barStart = 0; barStart < totalSamples; barStart += samplesPerBar)
        {
            const auto frames = std::min (sourceBuffer.getNumSamples(), totalSamples - barStart);
            for (int channel = 0; channel < channels; ++channel)
            {
                auto* dest = bedBuffer.getWritePointer (channel, barStart);
                const auto* source = sourceBuffer.getReadPointer (channel);
                for (int index = 0; index < frames; ++index)
                    dest[index] = juce::jlimit (-1.0f, 1.0f, dest[index] + source[index]);
            }
        }
    }

    const auto bedDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("SamplerFoodBench")
                            .getChildFile ("bed");
    bedDir.createDirectory();
    temporarySourceBedFile = bedDir.getChildFile (juce::String (sample.id) + "_bed.wav");
    temporarySourceBedFile.deleteFile();

    juce::WavAudioFormat wav;
    auto output = std::make_unique<juce::FileOutputStream> (temporarySourceBedFile);
    if (! output->openedOk())
        return sourceFile;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (output.release(), reader->sampleRate, static_cast<unsigned int> (channels), 16, {}, 0));
    if (writer == nullptr)
        return sourceFile;

    writer->writeFromAudioSampleBuffer (bedBuffer, 0, bedBuffer.getNumSamples());
    writer.reset();
    sourceBedCacheKey = cacheKey;
    return temporarySourceBedFile.existsAsFile() ? temporarySourceBedFile : sourceFile;
}

juce::File MainComponent::sourcePlaybackFile (const samplebench::Sample& sample,
                                              const samplebench::BenchSettings& settings)
{
    const auto rawFile = sourcePreviewFile (sample, settings);
    if (! samplebench::effectChainAffectsPreview (settings))
        return rawFile;

    return buildFxPreviewFile (sample, settings);
}

juce::File MainComponent::buildFxPreviewFile (const samplebench::Sample& sample,
                                              const samplebench::BenchSettings& settings)
{
    const auto rawFile = sourcePreviewFile (sample, settings);
    const auto cacheKey = juce::String (sample.id) + "|" + previewFxKey (rawFile, settings);
    if (temporaryFxPreviewFile.existsAsFile() && sourceFxPreviewCacheKey == cacheKey)
        return temporaryFxPreviewFile;

    // JUCE's transport reads files, so source monitoring with FX is rendered to a temp
    // WAV. Bounce export still goes through its own render path; this is only audition cache.
    const auto previewDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("SamplerFoodBench")
                                .getChildFile ("preview-fx");
    previewDir.createDirectory();
    temporaryFxPreviewFile = previewDir.getChildFile (juce::String (sample.id) + "_source_fx.wav");
    temporaryFxPreviewFile.deleteFile();

    if (! writeProcessedAudioFile (rawFile, temporaryFxPreviewFile, settings, samplebench::EffectProcessMode::preview, 0, -1))
        return rawFile;

    sourceFxPreviewCacheKey = cacheKey;
    return temporaryFxPreviewFile.existsAsFile() ? temporaryFxPreviewFile : rawFile;
}

std::unique_ptr<juce::AudioPluginInstance> MainComponent::createPluginInstance (const samplebench::HostedPluginModule& plugin,
                                                                                double sampleRate,
                                                                                int blockSize,
                                                                                juce::String& errorMessage)
{
    if (plugin.filePath.empty() || ! juce::File { juce::String::fromUTF8 (plugin.filePath.u8string().c_str()) }.exists())
    {
        errorMessage = "Plugin file is missing.";
        return nullptr;
    }

    juce::VST3PluginFormat format;
    auto instance = format.createInstanceFromDescription (toPluginDescription (plugin),
                                                         sampleRate,
                                                         blockSize,
                                                         errorMessage);

    if (instance == nullptr)
        return nullptr;

    if (! plugin.stateBlob.empty())
        instance->setStateInformation (plugin.stateBlob.data(), static_cast<int> (plugin.stateBlob.size()));

    return instance;
}

bool MainComponent::processFxModules (juce::AudioBuffer<float>& buffer,
                                      double sampleRate,
                                      const samplebench::BenchSettings& settings,
                                      samplebench::EffectProcessMode mode,
                                      juce::String& warning)
{
    constexpr int blockSize = 512;
    auto ok = true;

    // Built-ins live in the model DSP code so tests can exercise them without JUCE UI
    // plumbing. The app bridges one module at a time into the editable FX rack order.
    auto processBuiltIn = [&] (samplebench::BuiltInEffectId effect)
    {
        samplebench::AudioBufferData audio;
        audio.sampleRate = sampleRate;
        audio.channels.resize (static_cast<std::size_t> (buffer.getNumChannels()));
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* source = buffer.getReadPointer (channel);
            audio.channels[static_cast<std::size_t> (channel)].assign (source, source + buffer.getNumSamples());
        }

        auto singleSettings = settings;
        singleSettings.customFxModules = false;
        singleSettings.fxModules.clear();
        singleSettings.customEffectChain = true;
        singleSettings.effectChain = { effect };
        audio = samplebench::applyEffectChain (std::move (audio), singleSettings, mode);

        if (audio.channels.empty() || audio.channels.front().empty())
            return;

        juce::AudioBuffer<float> nextBuffer (static_cast<int> (audio.channels.size()),
                                             static_cast<int> (audio.channels.front().size()));
        nextBuffer.clear();
        for (int channel = 0; channel < nextBuffer.getNumChannels(); ++channel)
        {
            auto* dest = nextBuffer.getWritePointer (channel);
            const auto& source = audio.channels[static_cast<std::size_t> (channel)];
            std::copy (source.begin(), source.end(), dest);
        }
        buffer = std::move (nextBuffer);
    };

    for (const auto& module : samplebench::activeFxModules (settings))
    {
        if (module.kind == samplebench::FxModuleKind::builtIn)
        {
            processBuiltIn (module.builtIn);
            continue;
        }

        const auto& plugin = module.plugin;
        if (! plugin.enabled || plugin.status != samplebench::PluginModuleStatus::loaded)
            continue;

        juce::String error;
        auto instance = createPluginInstance (plugin, sampleRate, blockSize, error);
        if (instance == nullptr)
        {
            ok = false;
            warning = pluginName (plugin) + " skipped: " + (error.isNotEmpty() ? error : "could not load");
            continue;
        }

        // Plugin processing is best-effort in this prototype. One bad VST should warn and
        // skip, not take the whole bounce path down with it.
        const auto channels = buffer.getNumChannels();
        BenchPluginPlayHead playHead { settings.musicalBpm,
                                       sampleRate,
                                       mode == samplebench::EffectProcessMode::preview && playbackState.loopEnabled,
                                       buffer.getNumSamples() };
        instance->setPlayHead (&playHead);
        instance->setPlayConfigDetails (channels, channels, sampleRate, blockSize);
        instance->prepareToPlay (sampleRate, blockSize);
        if (instance->getLatencySamples() > 0)
            juce::Logger::writeToLog ("Bench Sampler: plugin latency not compensated yet: "
                                      + pluginName (plugin)
                                      + " "
                                      + juce::String (instance->getLatencySamples())
                                      + " samples");

        for (int offset = 0; offset < buffer.getNumSamples(); offset += blockSize)
        {
            const auto frames = std::min (blockSize, buffer.getNumSamples() - offset);
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), channels, offset, frames);
            juce::MidiBuffer midi;
            playHead.setTimeInSamples (offset);
            try
            {
                instance->processBlock (block, midi);
            }
            catch (const std::exception& exception)
            {
                ok = false;
                warning = pluginName (plugin) + " failed during processing: " + exception.what();
                break;
            }
            catch (...)
            {
                ok = false;
                warning = pluginName (plugin) + " failed during processing";
                break;
            }
        }

        instance->setPlayHead (nullptr);
        instance->releaseResources();
    }

    return ok;
}

bool MainComponent::writeProcessedAudioFile (const juce::File& inputFile,
                                             const juce::File& outputFile,
                                             const samplebench::BenchSettings& settings,
                                             samplebench::EffectProcessMode mode,
                                             juce::int64 startSample,
                                             juce::int64 samplesToRead)
{
    std::unique_ptr<juce::AudioFormatReader> reader (audioFormatManager.createReaderFor (inputFile));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return false;

    const auto channels = static_cast<int> (std::min<unsigned int> (2, reader->numChannels));
    const auto safeStart = juce::jlimit<juce::int64> (0, reader->lengthInSamples, startSample);
    const auto available = reader->lengthInSamples - safeStart;
    const auto framesToRead = samplesToRead < 0 ? available
                                                : juce::jlimit<juce::int64> (0, available, samplesToRead);
    if (channels <= 0 || framesToRead <= 0 || framesToRead > static_cast<juce::int64> (std::numeric_limits<int>::max()))
        return false;

    juce::AudioBuffer<float> buffer (channels, static_cast<int> (framesToRead));
    buffer.clear();
    reader->read (&buffer, 0, buffer.getNumSamples(), safeStart, true, true);

    juce::String warning;
    const auto processed = processFxModules (buffer, reader->sampleRate, settings, mode, warning);
    juce::ignoreUnused (processed);
    if (warning.isNotEmpty())
        renderStatusLabel.setText (warning, juce::dontSendNotification);

    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
        return false;

    juce::WavAudioFormat wav;
    auto output = std::make_unique<juce::FileOutputStream> (outputFile);
    if (! output->openedOk())
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (output.release(), reader->sampleRate, static_cast<unsigned int> (buffer.getNumChannels()), 16, {}, 0));
    if (writer == nullptr)
        return false;

    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    return true;
}

void MainComponent::updateWaveformPlaybackView()
{
    const auto target = playbackState.previewTarget;
    const auto isBounce = target == samplebench::PlaybackTarget::bounce;
    auto& visible = isBounce ? bounceVisibleRange : sourceVisibleRange;
    const auto duration = isBounce ? bounceDurationSeconds : sourceDurationSeconds;
    const auto position = isBounce ? renderTransport.getCurrentPosition()
                                   : sourceTransport.getCurrentPosition();
    const auto& peaks = isBounce ? bouncePeaks : sourcePeaks;
    const auto channels = isBounce ? bounceChannelCount : sourceChannelCount;

    if (duration <= 0.0 || peaks.empty())
    {
        waveformOverview.setPlaybackView ({ 0.0, 0.0 }, 0.0, false, target);
        return;
    }

    if (visible.endSeconds <= visible.startSeconds)
        visible = samplebench::makeInitialVisibleWindow (duration, 0.0, duration);

    if (playbackState.isPlaying && playbackState.target == target)
        visible = samplebench::autoFollowVisibleWindow (visible, position, duration);

    waveformOverview.setChannelPeaks (peaks, duration, channels);
    waveformOverview.setPlaybackView (visible,
                                      position,
                                      playbackState.isPlaying && playbackState.target == target,
                                      target);
}

void MainComponent::setMeterLevels (float left, float right, bool stereo)
{
    levelMeter.setLevels (left, stereo ? right : left, stereo, left >= 1.0f || right >= 1.0f);
}

void MainComponent::updateMeterFromPlayback()
{
    static float decayedLeft = 0.0f;
    static float decayedRight = 0.0f;

    if (! playbackState.isPlaying)
    {
        decayedLeft *= 0.88f;
        decayedRight *= 0.88f;
        levelMeter.setLevels (decayedLeft, decayedRight, sourceChannelCount >= 2 || bounceChannelCount >= 2, false);
        return;
    }

    const auto target = playbackState.target;
    const auto sample = selectedSample();
    const auto file = target == samplebench::PlaybackTarget::bounce
        ? temporaryRenderFile
        : sample.has_value() ? sourcePlaybackFile (*sample, settingsFromControls()) : juce::File {};
    const auto position = target == samplebench::PlaybackTarget::bounce
        ? renderTransport.getCurrentPosition()
        : sourceTransport.getCurrentPosition();

    std::unique_ptr<juce::AudioFormatReader> reader (audioFormatManager.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0)
        return;

    const auto channels = static_cast<int> (std::min<unsigned int> (reader->numChannels, 2));
    const auto startSample = static_cast<juce::int64> (std::max (0.0, position) * reader->sampleRate);
    const auto frames = static_cast<int> (std::min<juce::int64> (1024, reader->lengthInSamples - startSample));
    if (channels <= 0 || frames <= 0)
        return;

    juce::AudioBuffer<float> buffer (channels, frames);
    buffer.clear();
    reader->read (&buffer, 0, frames, startSample, true, true);

    decayedLeft = std::max (buffer.getMagnitude (0, 0, frames), decayedLeft * 0.72f);
    decayedRight = channels > 1 ? std::max (buffer.getMagnitude (1, 0, frames), decayedRight * 0.72f)
                                : decayedLeft;
    levelMeter.setLevels (decayedLeft, decayedRight, channels > 1, decayedLeft >= 1.0f || decayedRight >= 1.0f);
}

void MainComponent::loadWaveformForSample (const samplebench::Sample& sample)
{
    const auto settings = settingsFromControls();
    const auto overview = readAudioOverview (sourcePlaybackFile (sample, settings));
    sourcePeaks = overview.channelPeaks;
    sourceDurationSeconds = overview.durationSeconds;
    sourceChannelCount = overview.channels;
    sourceSampleRate = overview.sampleRate;

    if (sourcePeaks.empty())
    {
        waveformOverview.clear();
        return;
    }

    const auto regions = samplebench::calculateCaptureRegions (settings.musicalBpm, settings.capture);
    sourceVisibleRange = samplebench::makeInitialVisibleWindow (sourceDurationSeconds,
                                                                regions.keep.startSeconds,
                                                                regions.bounce.endSeconds - regions.bounce.startSeconds);
    waveformOverview.setChannelPeaks (sourcePeaks, sourceDurationSeconds, sourceChannelCount);
    if (settings.sourceBedMode == samplebench::SourceBedMode::extendForFx)
        waveformOverview.setSourceDescription ("Loop bed: " + juce::String (settings.bedLengthBars) + " bars from source");
    else
        waveformOverview.setSourceDescription ({});
    updateWaveformPlaybackView();
}

void MainComponent::refreshSourceMetadata (const samplebench::Sample& sample)
{
    const auto overview = readAudioOverview (juce::File { sample.sourcePath.string() });
    sourceMetadataLabel.setText (formatDurationMetadata (overview.channels,
                                                         overview.sampleRate,
                                                         overview.durationSeconds,
                                                         overview.fileSizeBytes),
                                 juce::dontSendNotification);
}

AudioOverview MainComponent::readAudioOverview (const juce::File& file)
{
    AudioOverview overview;
    overview.fileSizeBytes = file.getSize();

    std::unique_ptr<juce::AudioFormatReader> reader (audioFormatManager.createReaderFor (file));
    if (reader == nullptr)
        return overview;

    overview.channels = static_cast<int> (reader->numChannels);
    overview.sampleRate = reader->sampleRate;
    overview.durationSeconds = reader->sampleRate > 0.0
        ? static_cast<double> (reader->lengthInSamples) / reader->sampleRate
        : 0.0;
    overview.bitsPerSample = static_cast<int> (reader->bitsPerSample);

    constexpr int peakCount = 700;
    const auto channelsToRead = static_cast<int> (std::min<unsigned int> (reader->numChannels, 2));
    overview.channelPeaks.assign (static_cast<std::size_t> (channelsToRead),
                                  std::vector<float> (peakCount, 0.0f));

    const auto blockSize = std::max<juce::int64> (1, reader->lengthInSamples / peakCount);
    juce::AudioBuffer<float> buffer (channelsToRead, static_cast<int> (blockSize));

    for (int index = 0; index < peakCount; ++index)
    {
        const auto start = static_cast<juce::int64> (index) * blockSize;
        const auto frames = static_cast<int> (std::min<juce::int64> (blockSize, reader->lengthInSamples - start));
        if (frames <= 0)
            break;

        buffer.clear();
        reader->read (&buffer, 0, frames, start, true, true);

        for (int channel = 0; channel < channelsToRead; ++channel)
            overview.channelPeaks[static_cast<std::size_t> (channel)][static_cast<std::size_t> (index)] =
                buffer.getMagnitude (channel, 0, frames);
    }

    return overview;
}

void MainComponent::playSource()
{
    const auto sample = selectedSample();
    if (! sample.has_value())
        return;

    const auto settings = settingsFromControls();
    loadWaveformForSample (*sample);
    loadTransportFile (sourceTransport, sourceReaderSource, sourcePlaybackFile (*sample, settings));
    startPlaybackTarget (samplebench::PlaybackTarget::source);
}

void MainComponent::returnPreviewToStart()
{
    if (playbackState.previewTarget == samplebench::PlaybackTarget::source)
        sourceTransport.setPosition (currentSourcePreviewRegion().startSeconds);
    else
        renderTransport.setPosition (0.0);

    updateWaveformPlaybackView();
    refreshTransportControls();
}

void MainComponent::playSelectedPreview()
{
    if (playbackState.previewTarget == samplebench::PlaybackTarget::source)
    {
        playSource();
        return;
    }

    playBouncePreview();
}

void MainComponent::stopPlayback()
{
    sourceTransport.stop();
    renderTransport.stop();
    sourceTransport.setPosition (0.0);
    renderTransport.setPosition (0.0);
    sourceTransport.setLooping (false);
    renderTransport.setLooping (false);
    playbackState = samplebench::stopPlayback (playbackState);
    updateWaveformPlaybackView();
    refreshTransportControls();
}

void MainComponent::startPlaybackTarget (samplebench::PlaybackTarget target)
{
    sourceTransport.stop();
    renderTransport.stop();
    sourceTransport.setLooping (false);
    renderTransport.setLooping (false);

    playbackState = samplebench::startPlayback (playbackState, target);

    if (target == samplebench::PlaybackTarget::source)
    {
        sourceTransport.setPosition (currentSourcePreviewRegion().startSeconds);
        sourceTransport.setLooping (playbackState.loopEnabled && sourcePreviewScope == SourcePreviewScope::full);
        sourceTransport.start();
    }
    else if (target == samplebench::PlaybackTarget::bounce)
    {
        renderTransport.setPosition (0.0);
        renderTransport.setLooping (playbackState.loopEnabled);
        renderTransport.start();
    }

    refreshTransportControls();
}

void MainComponent::refreshTransportControls()
{
    const auto sample = selectedSample();
    const auto hasSample = sample.has_value();
    const auto hasBounce = temporaryRenderFile.existsAsFile();

    juce::String status = "No Bounce";
    if (pendingVariation.has_value())
        status = playbackState.target == samplebench::PlaybackTarget::bounce && playbackState.isPlaying ? "Playing Bounce"
               : playbackState.target == samplebench::PlaybackTarget::source && playbackState.isPlaying ? "Playing Source"
               : "Bounce Ready";
    else if (playbackState.target == samplebench::PlaybackTarget::source && playbackState.isPlaying)
        status = "Playing Source";

    transportStatusLabel.setText (status, juce::dontSendNotification);
    loopPreviewToggle.setToggleState (playbackState.loopEnabled, juce::dontSendNotification);

    const auto selectedTarget = playbackState.previewTarget;
    sourceTargetButton.setColour (juce::TextButton::buttonColourId,
                                  selectedTarget == samplebench::PlaybackTarget::source
                                      ? colour (samplebench::palette::accent)
                                      : colour (samplebench::palette::button));
    bounceTargetButton.setColour (juce::TextButton::buttonColourId,
                                  selectedTarget == samplebench::PlaybackTarget::bounce
                                      ? colour (samplebench::palette::accent)
                                      : colour (samplebench::palette::button));
    sourceTargetButton.setColour (juce::TextButton::textColourOffId,
                                  selectedTarget == samplebench::PlaybackTarget::source
                                      ? colour (samplebench::palette::inverseText)
                                      : colour (samplebench::palette::text));
    bounceTargetButton.setColour (juce::TextButton::textColourOffId,
                                  selectedTarget == samplebench::PlaybackTarget::bounce
                                      ? colour (samplebench::palette::inverseText)
                                      : colour (samplebench::palette::text));

    sourceKeepScopeButton.setColour (juce::TextButton::buttonColourId,
                                     sourcePreviewScope == SourcePreviewScope::keep
                                         ? colour (samplebench::palette::accent)
                                         : colour (samplebench::palette::button));
    sourceFullScopeButton.setColour (juce::TextButton::buttonColourId,
                                     sourcePreviewScope == SourcePreviewScope::full
                                         ? colour (samplebench::palette::accent)
                                         : colour (samplebench::palette::button));
    sourceKeepScopeButton.setColour (juce::TextButton::textColourOffId,
                                     sourcePreviewScope == SourcePreviewScope::keep
                                         ? colour (samplebench::palette::inverseText)
                                         : colour (samplebench::palette::text));
    sourceFullScopeButton.setColour (juce::TextButton::textColourOffId,
                                     sourcePreviewScope == SourcePreviewScope::full
                                         ? colour (samplebench::palette::inverseText)
                                         : colour (samplebench::palette::text));
    sourceKeepScopeButton.setEnabled (selectedTarget == samplebench::PlaybackTarget::source);
    sourceFullScopeButton.setEnabled (selectedTarget == samplebench::PlaybackTarget::source);

    const auto canPlay = hasSample
                      && (selectedTarget == samplebench::PlaybackTarget::source || hasBounce);
    playPreviewButton.setEnabled (canPlay);
    const auto isPlaying = playbackState.isPlaying;
    playPreviewButton.setColour (juce::TextButton::buttonColourId,
                                 isPlaying ? colour (samplebench::palette::accent)
                                           : colour (samplebench::palette::button));
    stopPreviewButton.setColour (juce::TextButton::buttonColourId,
                                 isPlaying ? colour (samplebench::palette::button)
                                           : colour (samplebench::palette::darkButton));
    loopPreviewToggle.setColour (juce::ToggleButton::tickColourId,
                                 playbackState.loopEnabled ? colour (samplebench::palette::accent)
                                                           : colour (samplebench::palette::border));
    loopPreviewToggle.setTooltip (selectedTarget == samplebench::PlaybackTarget::source
                                      ? (sourcePreviewScope == SourcePreviewScope::keep
                                             ? "Loop Keep Region"
                                             : "Loop Full Source")
                                      : "Loop Bounce");

    const auto position = selectedTarget == samplebench::PlaybackTarget::source
        ? sourceTransport.getCurrentPosition()
        : renderTransport.getCurrentPosition();
    const auto total = selectedTarget == samplebench::PlaybackTarget::source
        ? sourceDurationSeconds
        : bounceDurationSeconds;
    previewTimeLabel.setText (formatTime (position) + " / " + formatTime (total), juce::dontSendNotification);
    regionSummaryLabel.setText (regionSummaryText(), juce::dontSendNotification);
}

void MainComponent::selectPreviewTarget (samplebench::PlaybackTarget target)
{
    playbackState = samplebench::setPreviewTarget (playbackState, target);
    if (target == samplebench::PlaybackTarget::bounce && ! temporaryRenderFile.existsAsFile())
        renderStatusLabel.setText ("No Bounce", juce::dontSendNotification);

    updateWaveformPlaybackView();
    refreshTransportControls();
}

void MainComponent::renderPreview()
{
    const auto sample = selectedSample();
    if (! sample.has_value())
        return;

    setRenderActionsAvailable (false);
    renderStatusLabel.setText ("Rendering...", juce::dontSendNotification);

    const auto settings = settingsFromControls();
    pack.updateBenchSettings (sample->id, settings);

    const auto sourceFile = sourcePreviewFile (*sample, settings);
    std::unique_ptr<juce::AudioFormatReader> reader (
        audioFormatManager.createReaderFor (sourceFile));

    if (reader == nullptr)
    {
        renderStatusLabel.setText ("Render: could not read source file", juce::dontSendNotification);
        return;
    }

    const auto sampleRate = reader->sampleRate;
    const auto regions = samplebench::calculateCaptureRegions (settings.musicalBpm, settings.capture);
    // One-shots default to the whole file, while loops/textures use the bar-based capture
    // region. That keeps "Render Preview" faithful to the export rules.
    const auto shouldUseWholeSource = settings.sourceBedMode == samplebench::SourceBedMode::asIs
                                   && settings.type == samplebench::SampleType::oneShot;
    const auto startSample = shouldUseWholeSource ? 0
                                                  : static_cast<juce::int64> (std::llround (regions.bounce.startSeconds * sampleRate));
    const auto requestedSamples = shouldUseWholeSource
        ? reader->lengthInSamples - startSample
        : static_cast<juce::int64> (std::llround ((regions.bounce.endSeconds - regions.bounce.startSeconds) * sampleRate));
    const auto samplesToWrite = juce::jlimit<juce::int64> (0, reader->lengthInSamples - startSample, requestedSamples);

    if (samplesToWrite <= 0)
    {
        renderStatusLabel.setText ("Render: capture region is outside the source file", juce::dontSendNotification);
        return;
    }

    const auto finalFilename = samplebench::buildFinalFilename (settings);
    const auto renderDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("SamplerFoodBench")
                               .getChildFile ("preview");
    renderDir.createDirectory();
    temporaryRenderFile = renderDir.getChildFile (finalFilename);
    temporaryRenderFile.deleteFile();

    if (! writeProcessedAudioFile (sourceFile,
                                   temporaryRenderFile,
                                   settings,
                                   samplebench::EffectProcessMode::bounce,
                                   startSample,
                                   samplesToWrite))
    {
        renderStatusLabel.setText ("Render: could not create processed preview WAV", juce::dontSendNotification);
        return;
    }

    // Pending means "auditioned but not committed." Keep Bounce copies this temp WAV into
    // the variation list; Trash Bounce simply drops this pending record.
    pendingVariation = samplebench::RenderedVariation {
        temporaryRenderFile.getFullPathName().toStdString(),
        finalFilename,
        finalFilename,
        sample->id,
        sample->sourcePath,
        settings
    };

    loadTransportFile (renderTransport, renderReaderSource, temporaryRenderFile);
    const auto bounceOverview = readAudioOverview (temporaryRenderFile);
    bouncePeaks = bounceOverview.channelPeaks;
    bounceDurationSeconds = bounceOverview.durationSeconds;
    bounceChannelCount = bounceOverview.channels;
    bounceSampleRate = bounceOverview.sampleRate;
    bounceVisibleRange = samplebench::makeInitialVisibleWindow (bounceDurationSeconds, 0.0, bounceDurationSeconds);
    setRenderActionsAvailable (true);
    playbackState = samplebench::setPreviewTarget (playbackState, samplebench::PlaybackTarget::bounce);
    updateWaveformPlaybackView();
    refreshTransportControls();
    renderStatusLabel.setText ("Bounce Ready  "
                                   + formatDurationMetadata (bounceChannelCount,
                                                             bounceSampleRate,
                                                             bounceDurationSeconds,
                                                             temporaryRenderFile.getSize())
                                   + "  "
                                   + temporaryRenderFile.getFileName()
                                   + "  "
                                   + juce::String (samplebench::effectChainSummary (settings)),
                               juce::dontSendNotification);
    renderStatusLabel.setTooltip (samplebench::effectChainSummary (settings));
}

void MainComponent::playBouncePreview()
{
    if (! temporaryRenderFile.existsAsFile())
    {
        renderStatusLabel.setText ("No Bounce", juce::dontSendNotification);
        refreshTransportControls();
        return;
    }

    loadTransportFile (renderTransport, renderReaderSource, temporaryRenderFile);
    startPlaybackTarget (samplebench::PlaybackTarget::bounce);
}

void MainComponent::keepRenderedVariation()
{
    const auto sample = selectedSample();
    if (! sample.has_value() || ! pendingVariation.has_value() || ! temporaryRenderFile.existsAsFile())
        return;

    const auto bucketDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("SamplerFoodBench")
                               .getChildFile ("kept")
                               .getChildFile (bucketLabel (sample->bucket));
    bucketDir.createDirectory();

    const auto destination = bucketDir.getChildFile (pendingVariation->finalFilename);
    destination.deleteFile();
    temporaryRenderFile.copyFileTo (destination);

    auto variation = *pendingVariation;
    variation.filePath = destination.getFullPathName().toStdString();
    variation.displayName = destination.getFileName().toStdString();
    pack.keepVariation (sample->id, variation);
    markPackDirty();

    renderStatusLabel.setText ("Kept Bounce  " + destination.getFileName(), juce::dontSendNotification);
    transportStatusLabel.setText ("Kept", juce::dontSendNotification);
    refreshBenchView();
}

void MainComponent::trashRenderedPreview()
{
    if (playbackState.target == samplebench::PlaybackTarget::bounce)
        stopPlayback();

    renderTransport.stop();
    renderTransport.setSource (nullptr);
    renderReaderSource = nullptr;

    if (temporaryRenderFile.existsAsFile())
        temporaryRenderFile.deleteFile();

    temporaryRenderFile = juce::File();
    pendingVariation = std::nullopt;
    bouncePeaks.clear();
    bounceDurationSeconds = 0.0;
    bounceChannelCount = 0;
    bounceSampleRate = 0.0;
    setRenderActionsAvailable (false);
    renderStatusLabel.setText ("No Bounce", juce::dontSendNotification);
    if (playbackState.previewTarget == samplebench::PlaybackTarget::bounce)
        playbackState = samplebench::setPreviewTarget (playbackState, samplebench::PlaybackTarget::source);
    updateWaveformPlaybackView();
    refreshTransportControls();
}

void MainComponent::loadTransportFile (juce::AudioTransportSource& transport,
                                       std::unique_ptr<juce::AudioFormatReaderSource>& readerSource,
                                       const juce::File& file)
{
    transport.stop();
    transport.setSource (nullptr);
    readerSource = nullptr;

    std::unique_ptr<juce::AudioFormatReader> reader (audioFormatManager.createReaderFor (file));
    if (reader == nullptr)
        return;

    const auto sampleRate = reader->sampleRate;
    auto newSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
    transport.setSource (newSource.get(), 0, nullptr, sampleRate);
    readerSource = std::move (newSource);
}

samplebench::BenchSettings MainComponent::settingsFromControls() const
{
    samplebench::BenchSettings settings;
    settings.type = typeSelector.getSelectedId() == 2 ? samplebench::SampleType::loop
                 : typeSelector.getSelectedId() == 3 ? samplebench::SampleType::texture
                 : samplebench::SampleType::oneShot;
    settings.musicalBpm = doubleFromEditor (musicalBpmEditor, 0.0);
    settings.bars = std::max (0, intFromEditor (barsEditor, 0));
    settings.key = keyEditor.getText().toStdString();
    settings.capture.captureStartBar = std::max (1, intFromEditor (captureStartBarEditor, 1));
    settings.capture.warmupBars = std::max (0, intFromEditor (warmupBarsEditor, 0));
    settings.capture.keepBars = std::max (1, intFromEditor (keepBarsEditor, 4));
    settings.capture.tailBars = std::max (0, intFromEditor (tailBarsEditor, 0));
    settings.name = nameEditor.getText().trim().toStdString();
    settings.flavor = flavorSelector.getSelectedId() == 2 ? samplebench::RenderFlavor::wet
                                                           : samplebench::RenderFlavor::dry;
    settings.version = std::max (1, intFromEditor (versionEditor, 1));
    settings.speedTrickEnabled = speedTrickToggle.getToggleState();
    settings.gainEnabled = gainEnabled;
    settings.gainDecibels = static_cast<float> (gainSlider.getValue());
    settings.normalizeEnabled = normalizeToggle.getToggleState();
    settings.normalizeTargetDecibels = static_cast<float> (normalizeTargetKnob.getValue());
    settings.monoEnabled = monoToggle.getToggleState();
    settings.limitEnabled = limitEnabled;
    settings.limitCeilingDecibels = limitCeilingDecibels;
    settings.limitInputDecibels = limitInputDecibels;
    settings.limitReleaseMs = limitReleaseMs;
    settings.compressorEnabled = compressorEnabled;
    settings.compressorThresholdDecibels = compressorThresholdDecibels;
    settings.compressorRatio = compressorRatio;
    settings.compressorAttackMs = compressorAttackMs;
    settings.compressorReleaseMs = compressorReleaseMs;
    settings.compressorMakeupDecibels = compressorMakeupDecibels;
    settings.compressorMix = compressorMix;
    settings.crushEnabled = crushEnabled;
    settings.crushBits = static_cast<int> (std::round (crushBitsKnob.getValue()));
    switch (static_cast<int> (std::round (crushRateKnob.getValue())))
    {
        case 0: settings.crushSampleRate = 0.0; break;
        case 1: settings.crushSampleRate = 44100.0; break;
        case 2: settings.crushSampleRate = 32000.0; break;
        case 3: settings.crushSampleRate = 22050.0; break;
        case 4: settings.crushSampleRate = 16000.0; break;
        default: settings.crushSampleRate = 11025.0; break;
    }
    settings.crushMix = static_cast<float> (crushMixKnob.getValue() / 100.0);
    settings.filterEnabled = filterEnabled;
    settings.filterMode = filterMode;
    settings.filterCutoffHz = static_cast<float> (filterCutoffKnob.getValue());
    settings.filterResonance = static_cast<float> (filterResonanceKnob.getValue());
    settings.driveEnabled = driveEnabled;
    settings.driveAmount = driveAmount;
    settings.driveTone = driveTone;
    settings.driveMix = driveMix;
    settings.driveOutputDecibels = driveOutputDecibels;
    settings.eqEnabled = eqEnabled;
    settings.eqLowDecibels = eqLowDecibels;
    settings.eqMidDecibels = eqMidDecibels;
    settings.eqHighDecibels = eqHighDecibels;
    settings.delayEnabled = delayEnabled;
    settings.delayDivision = delayDivision;
    settings.delayFeedback = delayFeedback;
    settings.delayMix = delayMix;
    settings.delayTone = delayTone;
    settings.reverbEnabled = reverbEnabled;
    settings.reverbSize = reverbSize;
    settings.reverbDecaySeconds = reverbDecaySeconds;
    settings.reverbMix = reverbMix;
    settings.reverbTone = reverbTone;
    settings.tapeEnabled = tapeEnabled;
    settings.tapeDrive = tapeDrive;
    settings.tapeWobble = tapeWobble;
    settings.tapeTone = tapeTone;
    settings.tapeNoise = tapeNoise;
    settings.tapeMix = tapeMix;
    settings.customEffectChain = true;
    settings.effectChain.clear();
    for (const auto& module : fxChain)
        if (module.kind == samplebench::FxModuleKind::builtIn)
            settings.effectChain.push_back (module.builtIn);
    settings.customFxModules = true;
    settings.fxModules = fxChain;
    settings.sourceBedMode = sourceBedMode;
    settings.bedLengthBars = std::max (1, intFromEditor (bedLengthBarsEditor, 16));
    settings.bedTriggerMode = bedTriggerSelector.getSelectedId() == 2 ? samplebench::BedTriggerMode::oncePerBar
                                                                      : samplebench::BedTriggerMode::loopContinuously;
    return settings;
}

std::optional<samplebench::Sample> MainComponent::selectedSample() const
{
    return pack.selectedSample();
}

const std::vector<samplebench::Sample>& MainComponent::currentSamples() const
{
    return pack.samplesInBucket (currentBucket);
}

bool MainComponent::isSupportedAudioFile (const juce::File& file)
{
    const auto extension = file.getFileExtension().toLowerCase();
    return extension == ".wav"
        || extension == ".aif"
        || extension == ".aiff"
        || extension == ".flac"
        || extension == ".mp3";
}

juce::String MainComponent::bucketLabel (samplebench::BucketId bucket)
{
    switch (bucket)
    {
        case samplebench::BucketId::drums: return "A_DRUMS";
        case samplebench::BucketId::bass: return "B_BASS";
        case samplebench::BucketId::melody: return "C_MELODY";
        case samplebench::BucketId::other: return "D_OTHER";
    }

    return "D_OTHER";
}

juce::String MainComponent::sampleTypeLabel (samplebench::SampleType type)
{
    switch (type)
    {
        case samplebench::SampleType::oneShot: return "one-shot";
        case samplebench::SampleType::loop: return "loop";
        case samplebench::SampleType::texture: return "texture";
    }

    return "one-shot";
}
