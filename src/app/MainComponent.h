#pragma once

#include "app/BenchPalette.h"
#include "app/PluginScanWindow.h"
#include "app/WaveformOverview.h"
#include "model/SampleBenchModel.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

class RotaryKnob final : public juce::Slider
{
public:
    RotaryKnob();
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    double dragStartProportion = 0.0;
    float dragStartY = 0.0f;
    bool directDragActive = false;
};

class FxModuleCard final : public juce::Button
{
public:
    explicit FxModuleCard (juce::String moduleName);
    void setModuleName (juce::String moduleName);
    void setCardState (bool powerOn, bool isSelected, juce::String newSummary);
    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    juce::String name;
    juce::String summary;
    bool powered = false;
    bool selected = false;
};

class LevelMeterComponent final : public juce::Component
{
public:
    void setLevels (float leftPeak, float rightPeak, bool stereo, bool clipping);
    void paint (juce::Graphics& g) override;

private:
    float left = 0.0f;
    float right = 0.0f;
    bool isStereo = false;
    bool isClipping = false;
};

class BucketDropButton final : public juce::TextButton,
                               public juce::DragAndDropTarget
{
public:
    BucketDropButton() = default;

    samplebench::BucketId bucket = samplebench::BucketId::drums;
    std::function<void (std::size_t, samplebench::BucketId)> onSampleDropped;

    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDragEnter (const SourceDetails& dragSourceDetails) override;
    void itemDragExit (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;
    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    bool dropHighlighted = false;
};

class BenchContentComponent final : public juce::Component
{
public:
    void setSampleWorkbenchVisible (bool visible);
    void paint (juce::Graphics& g) override;

private:
    bool sampleWorkbenchVisible = true;
};

struct AudioOverview
{
    std::vector<std::vector<float>> channelPeaks;
    double durationSeconds = 0.0;
    int channels = 0;
    double sampleRate = 0.0;
    int bitsPerSample = 0;
    juce::int64 fileSizeBytes = 0;
};

enum class SourcePreviewScope
{
    keep,
    full
};

class MainComponent final : public juce::Component,
                            public juce::DragAndDropContainer,
                            public juce::FileDragAndDropTarget,
                            public juce::ListBoxModel,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber,
                           juce::Graphics& g,
                           int width,
                           int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe) override;
    bool mayDragToExternalWindows() const override;
    juce::MouseCursor getMouseCursorForRow (int row) override;
    juce::String getTooltipForRow (int row) override;

private:
    void timerCallback() override;
    void configureBucketButtons();
    void chooseFilesToImport();
    void importFiles (const juce::StringArray& files);
    void requestNewPack();
    void resetToNewPack();
    void requestOpenPack();
    void openPackFile (const juce::File& file);
    void savePack();
    void savePackAs (std::function<void (bool)> afterSave = {});
    bool savePackToFile (const juce::File& file);
    void savePackBeforeReplacing (std::function<void()> afterSaveOrDiscard);
    void commitPackNameEditor();
    void requestExportPack();
    void exportPackToDirectory (const juce::File& destinationDirectory);
    void showExportResult (const samplebench::PackExportResult& result);
    void markPackDirty();
    void refreshPackChrome();
    void moveSampleToBucket (std::size_t sampleId, samplebench::BucketId targetBucket);
    void selectBucket (samplebench::BucketId bucket);
    void refreshBucketButtons();
    void refreshSampleList();
    void refreshBenchView();
    void refreshFilenamePreview();
    void refreshCaptureChips();
    void setBenchControlsEnabled (bool enabled);
    void setSampleWorkbenchVisible (bool visible);
    void clearNoSamplePreviewState();
    void setRenderActionsAvailable (bool available);
    void loadWaveformForSample (const samplebench::Sample& sample);
    void refreshSourceMetadata (const samplebench::Sample& sample);
    void updateWaveformPlaybackView();
    void updateMeterFromPlayback();
    void setMeterLevels (float left, float right, bool stereo);
    [[nodiscard]] AudioOverview readAudioOverview (const juce::File& file);
    void setKeepBars (int bars);
    void setWarmupBars (int bars);
    void setTailBars (int bars);
    void nudgeStartBar (int delta);
    void nudgeWarmupBars (int delta);
    void nudgeKeepBars (int delta);
    void nudgeTailBars (int delta);
    void nudgeBedLengthBars (int delta);
    void selectSourcePreviewScope (SourcePreviewScope scope);
    void selectSourceBedMode (samplebench::SourceBedMode mode);
    void refreshSourceBedControls();
    void selectFxModule (samplebench::BuiltInEffectId effect);
    void selectFxChainSlot (int slot);
    void showAddFxMenu();
    void addFxModuleToChain (samplebench::BuiltInEffectId effect);
    void addPluginModuleToChain (const samplebench::CachedPluginDescription& plugin);
    void removeSelectedFxModule();
    void moveSelectedFxModule (int delta);
    [[nodiscard]] bool fxChainContains (samplebench::BuiltInEffectId effect) const;
    [[nodiscard]] int selectedFxChainIndex() const;
    [[nodiscard]] samplebench::FxModule* selectedFxModule();
    [[nodiscard]] const samplebench::FxModule* selectedFxModule() const;
    void handleGenericFxKnobChanged();
    void syncGenericFxControlsFromState();
    void setGenericFxLabels (const juce::String& a = {},
                             const juce::String& b = {},
                             const juce::String& c = {},
                             const juce::String& d = {},
                             const juce::String& e = {},
                             const juce::String& f = {});
    void refreshFxControls();
    void resetSelectedFxModule();
    void handleFxControlsChanged();
    void handleFxParameterChanged();
    void beginFxParameterDrag();
    void endFxParameterDrag();
    void enableSelectedFxFromParameterEdit();
    void openSelectedPluginEditor();
    void storePluginStateForSlot (int slot, const juce::MemoryBlock& state);
    void openPluginSettings();
    [[nodiscard]] juce::File pluginRegistryCacheFile() const;

    void returnPreviewToStart();
    void playSelectedPreview();
    void playSource();
    void renderPreview();
    void playBouncePreview();
    void stopPlayback();
    void startPlaybackTarget (samplebench::PlaybackTarget target);
    void refreshTransportControls();
    void selectPreviewTarget (samplebench::PlaybackTarget target);
    [[nodiscard]] samplebench::CaptureRegions currentCaptureRegions() const;
    [[nodiscard]] samplebench::VisibleTimeRange currentSourcePreviewRegion() const;
    [[nodiscard]] juce::String regionSummaryText() const;
    [[nodiscard]] juce::File sourcePreviewFile (const samplebench::Sample& sample,
                                                const samplebench::BenchSettings& settings);
    [[nodiscard]] juce::File buildSourceBedFile (const samplebench::Sample& sample,
                                                 const samplebench::BenchSettings& settings);
    [[nodiscard]] juce::File sourcePlaybackFile (const samplebench::Sample& sample,
                                                 const samplebench::BenchSettings& settings);
    [[nodiscard]] juce::File buildFxPreviewFile (const samplebench::Sample& sample,
                                                 const samplebench::BenchSettings& settings);
    [[nodiscard]] bool writeProcessedAudioFile (const juce::File& inputFile,
                                                const juce::File& outputFile,
                                                const samplebench::BenchSettings& settings,
                                                samplebench::EffectProcessMode mode,
                                                juce::int64 startSample,
                                                juce::int64 samplesToRead);
    [[nodiscard]] bool processFxModules (juce::AudioBuffer<float>& buffer,
                                         double sampleRate,
                                         const samplebench::BenchSettings& settings,
                                         samplebench::EffectProcessMode mode,
                                         juce::String& warning);
    [[nodiscard]] std::unique_ptr<juce::AudioPluginInstance> createPluginInstance (const samplebench::HostedPluginModule& plugin,
                                                                                  double sampleRate,
                                                                                  int blockSize,
                                                                                  juce::String& errorMessage);
    void keepRenderedVariation();
    void trashRenderedPreview();
    void loadTransportFile (juce::AudioTransportSource& transport,
                            std::unique_ptr<juce::AudioFormatReaderSource>& readerSource,
                            const juce::File& file);

    [[nodiscard]] samplebench::BenchSettings settingsFromControls() const;
    [[nodiscard]] std::optional<samplebench::Sample> selectedSample() const;

    [[nodiscard]] const std::vector<samplebench::Sample>& currentSamples() const;
    [[nodiscard]] static bool isSupportedAudioFile (const juce::File& file);
    [[nodiscard]] static juce::String bucketLabel (samplebench::BucketId bucket);
    [[nodiscard]] static juce::String sampleTypeLabel (samplebench::SampleType type);

    samplebench::Pack pack;
    samplebench::BucketId currentBucket = samplebench::BucketId::drums;
    juce::File currentPackFile;
    bool packDirty = false;
    bool suppressPackDirty = false;

    juce::Label titleLabel;
    juce::TextEditor packNameEditor;
    juce::Label subtitleLabel;
    juce::Label topAppLabel;
    juce::Label topPackLabel;
    juce::Label transportStatusLabel;
    juce::TextButton settingsButton { "SETTINGS" };
    juce::TextButton exportPackButton { "EXPORT PACK" };

    juce::TextButton newPackButton { "New" };
    juce::TextButton openPackButton { "Open" };
    juce::TextButton savePackButton { "Save" };
    juce::TextButton importButton { "Import Audio" };
    std::array<BucketDropButton, 4> bucketButtons;
    juce::ListBox sampleList { "Samples", this };

    juce::Viewport benchViewport { "Bench Viewport" };
    BenchContentComponent benchContent;
    juce::Label benchTitleLabel;
    juce::Label sourceNameLabel;
    juce::Label sourceMetadataLabel;
    juce::Label sourceBucketLabel;
    juce::Label typeLabel;
    juce::Label bpmLabel;
    juce::Label barsFieldLabel;
    juce::Label keyFieldLabel;
    juce::ComboBox typeSelector;
    juce::TextEditor musicalBpmEditor;
    juce::TextEditor barsEditor;
    juce::TextEditor keyEditor;
    juce::TextButton playSourceButton { "Play Source" };

    WaveformOverview waveformOverview;
    LevelMeterComponent levelMeter;
    juce::Label captureTitleLabel;
    juce::TextButton returnToStartButton { "|<" };
    juce::TextButton playPreviewButton { "Play" };
    juce::TextButton stopPreviewButton { "Stop" };
    juce::ToggleButton loopPreviewToggle { "Loop" };
    juce::Label previewTimeLabel;
    juce::Label previewTargetLabel;
    juce::TextButton sourceTargetButton { "Source" };
    juce::TextButton bounceTargetButton { "Bounce" };
    juce::Label sourceBedLabel;
    juce::TextButton sourceBedAsIsButton { "As-is" };
    juce::TextButton sourceBedExtendButton { "Extend for FX" };
    juce::Label bedTriggerLabel;
    juce::ComboBox bedTriggerSelector;
    juce::Label bedLengthLabel;
    juce::TextEditor bedLengthBarsEditor;
    juce::TextButton bedLengthMinusButton { "-" };
    juce::TextButton bedLengthPlusButton { "+" };
    juce::Label previewScopeLabel;
    juce::TextButton sourceKeepScopeButton { "Keep" };
    juce::TextButton sourceFullScopeButton { "Full" };
    juce::Label captureStartLabel;
    juce::Label warmupLabel;
    juce::Label keepLabel;
    juce::Label tailLabel;
    juce::Label regionSummaryLabel;
    juce::TextEditor captureStartBarEditor;
    juce::TextButton startMinusButton { "-" };
    juce::TextButton startPlusButton { "+" };
    juce::TextEditor warmupBarsEditor;
    juce::TextButton warmupMinusButton { "-" };
    juce::TextButton warmupPlusButton { "+" };
    juce::TextEditor keepBarsEditor;
    juce::TextButton keepMinusButton { "-" };
    juce::TextButton keepPlusButton { "+" };
    juce::TextEditor tailBarsEditor;
    juce::TextButton tailMinusButton { "-" };
    juce::TextButton tailPlusButton { "+" };
    juce::Label keepQuickLabel;
    juce::Label warmupQuickLabel;
    juce::Label tailQuickLabel;
    std::array<juce::TextButton, 5> keepQuickButtons {
        juce::TextButton { "1" }, juce::TextButton { "2" }, juce::TextButton { "4" },
        juce::TextButton { "8" }, juce::TextButton { "16" }
    };
    std::array<juce::TextButton, 5> warmupQuickButtons {
        juce::TextButton { "Off" }, juce::TextButton { "1" },
        juce::TextButton { "4" }, juce::TextButton { "8" },
        juce::TextButton { "16" }
    };
    std::array<juce::TextButton, 4> tailQuickButtons {
        juce::TextButton { "Off" }, juce::TextButton { "1" },
        juce::TextButton { "2" }, juce::TextButton { "4" }
    };

    juce::Label fxTitleLabel;
    FxModuleCard gainModuleCard { "GAIN" };
    FxModuleCard monoModuleCard { "MONO" };
    FxModuleCard normalizeModuleCard { "NORM" };
    FxModuleCard crushModuleCard { "CRUSH" };
    FxModuleCard filterModuleCard { "FILTER" };
    FxModuleCard limitModuleCard { "LIMIT" };
    FxModuleCard compressorModuleCard { "COMP" };
    FxModuleCard driveModuleCard { "DRIVE" };
    FxModuleCard eqModuleCard { "EQ" };
    FxModuleCard delayModuleCard { "DELAY" };
    FxModuleCard reverbModuleCard { "REVERB" };
    FxModuleCard tapeModuleCard { "TAPE" };
    juce::TextButton addFxButton { "+" };
    juce::TextButton moveFxLeftButton { "<" };
    juce::TextButton moveFxRightButton { ">" };
    juce::TextButton removeFxButton { "Remove" };
    juce::Label fxDetailTitleLabel;
    juce::Label fxParamLabelA;
    juce::Label fxParamLabelB;
    juce::Label fxParamLabelC;
    juce::Label fxParamLabelD;
    juce::Label fxParamLabelE;
    juce::Label fxParamLabelF;
    juce::ToggleButton fxPowerToggle { "Power" };
    juce::TextButton fxResetButton { "Reset" };
    juce::TextButton openPluginEditorButton { "Open Editor" };
    juce::Label pluginStatusLabel;
    juce::Slider gainSlider;
    juce::Label gainLabel;
    juce::ToggleButton normalizeToggle { "Normalize on Bounce" };
    juce::ToggleButton monoToggle { "Mono Off" };
    RotaryKnob normalizeTargetKnob;
    RotaryKnob crushBitsKnob;
    RotaryKnob crushRateKnob;
    RotaryKnob crushMixKnob;
    RotaryKnob filterCutoffKnob;
    RotaryKnob filterResonanceKnob;
    RotaryKnob genericFxKnobA;
    RotaryKnob genericFxKnobB;
    RotaryKnob genericFxKnobC;
    RotaryKnob genericFxKnobD;
    RotaryKnob genericFxKnobE;
    RotaryKnob genericFxKnobF;
    juce::TextButton filterLpButton { "LP" };
    juce::TextButton filterHpButton { "HP" };
    juce::Label fxHintLabel;

    juce::Label exportTitleLabel;
    juce::Label nameLabel;
    juce::Label flavorLabel;
    juce::Label versionLabel;
    juce::TextEditor nameEditor;
    juce::ComboBox flavorSelector;
    juce::TextEditor versionEditor;
    juce::ToggleButton speedTrickToggle { "Speed Trick" };
    juce::Label filenamePreviewLabel;
    juce::TextButton renderPreviewButton { "Render Preview" };
    juce::TextButton keepVariationButton { "Keep Bounce" };
    juce::TextButton trashRenderButton { "Trash Bounce" };
    juce::Label renderStatusLabel;

    juce::Label boundaryLabel;
    juce::Label emptyBenchTitleLabel;
    juce::Label emptyBenchLabel;
    juce::TextButton emptyBenchImportButton { "Import Audio" };

    juce::AudioFormatManager audioFormatManager;
    juce::AudioDeviceManager audioDeviceManager;
    juce::AudioSourcePlayer audioSourcePlayer;
    juce::MixerAudioSource mixerSource;
    juce::AudioTransportSource sourceTransport;
    juce::AudioTransportSource renderTransport;
    std::unique_ptr<juce::AudioFormatReaderSource> sourceReaderSource;
    std::unique_ptr<juce::AudioFormatReaderSource> renderReaderSource;

    juce::File temporaryRenderFile;
    juce::File temporarySourceBedFile;
    juce::File temporaryFxPreviewFile;
    juce::String sourceBedCacheKey;
    juce::String sourceFxPreviewCacheKey;
    std::optional<samplebench::RenderedVariation> pendingVariation;
    samplebench::PlaybackState playbackState;
    SourcePreviewScope sourcePreviewScope = SourcePreviewScope::keep;
    samplebench::SourceBedMode sourceBedMode = samplebench::SourceBedMode::asIs;
    samplebench::BuiltInEffectId selectedFx = samplebench::BuiltInEffectId::gain;
    int selectedFxSlot = -1;
    std::vector<samplebench::FxModule> fxChain;
    bool gainEnabled = true;
    bool limitEnabled = false;
    bool compressorEnabled = false;
    bool crushEnabled = false;
    bool filterEnabled = false;
    bool driveEnabled = false;
    bool eqEnabled = false;
    bool delayEnabled = false;
    bool reverbEnabled = false;
    bool tapeEnabled = false;
    samplebench::FilterMode filterMode = samplebench::FilterMode::lowPass;
    float limitCeilingDecibels = -1.0f;
    float limitInputDecibels = 0.0f;
    float limitReleaseMs = 80.0f;
    float compressorThresholdDecibels = -18.0f;
    float compressorRatio = 4.0f;
    float compressorAttackMs = 10.0f;
    float compressorReleaseMs = 120.0f;
    float compressorMakeupDecibels = 0.0f;
    float compressorMix = 1.0f;
    float driveAmount = 0.25f;
    float driveTone = 0.5f;
    float driveMix = 1.0f;
    float driveOutputDecibels = 0.0f;
    float eqLowDecibels = 0.0f;
    float eqMidDecibels = 0.0f;
    float eqHighDecibels = 0.0f;
    int delayDivision = 2;
    float delayFeedback = 0.25f;
    float delayMix = 0.2f;
    float delayTone = 0.35f;
    float reverbSize = 0.35f;
    float reverbDecaySeconds = 2.0f;
    float reverbMix = 0.2f;
    float reverbTone = 0.35f;
    float tapeDrive = 0.2f;
    float tapeWobble = 0.1f;
    float tapeTone = 0.35f;
    float tapeNoise = 0.0f;
    float tapeMix = 1.0f;
    bool fxParameterDragActive = false;
    bool fxParameterChangedDuringDrag = false;
    samplebench::VisibleTimeRange sourceVisibleRange;
    samplebench::VisibleTimeRange bounceVisibleRange;
    std::vector<std::vector<float>> sourcePeaks;
    std::vector<std::vector<float>> bouncePeaks;
    double sourceDurationSeconds = 0.0;
    double bounceDurationSeconds = 0.0;
    int sourceChannelCount = 0;
    int bounceChannelCount = 0;
    double sourceSampleRate = 0.0;
    double bounceSampleRate = 0.0;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::FileChooser> packFileChooser;
    std::unique_ptr<PluginScanWindow> pluginSettingsWindow;
    std::vector<std::unique_ptr<juce::DocumentWindow>> pluginEditorWindows;
    samplebench::PluginRegistry pluginRegistryCache;
    bool loadingControls = false;
    bool shuttingDown = false;
};
