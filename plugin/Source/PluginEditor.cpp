#include "PluginEditor.h"

#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace {

constexpr int kRowH = 76;

juce::Colour plateGrey() { return juce::Colour(0xff2c2c2c); }
juce::Colour controlGrey() { return juce::Colour(0xff3a3a3a); }

class VoiceRow : public juce::Component {
 public:
  VoiceRow(int index, ShogunAudioProcessor& proc) : index_(index), proc_(proc) {
    const auto& spec = shogun_ui::voices()[static_cast<size_t>(index)];
    name_.setButtonText(juce::String(spec.name) + " " + juce::String(36 + index));
    name_.setClickingTogglesState(false);
    addAndMakeVisible(name_);

    trig_.setButtonText("TRIG");
    trig_.onClick = [this] { proc_.pushPad(index_); };
    addAndMakeVisible(trig_);

    level_.setSliderStyle(juce::Slider::LinearHorizontal);
    level_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(level_);

    for (int i = 0; i < spec.count; ++i) {
      const auto n = static_cast<size_t>(i);
      captions_[n].setText(spec.knobs[n].label, juce::dontSendNotification);
      captions_[n].setJustificationType(juce::Justification::centred);
      captions_[n].setFont(juce::Font(juce::FontOptions(11.0f)));
      addAndMakeVisible(captions_[n]);
      knobs_[n].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      knobs_[n].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 12);
      addAndMakeVisible(knobs_[n]);
    }
  }

  void setSelected(bool selected) {
    name_.setColour(juce::TextButton::buttonColourId,
                    selected ? juce::Colour(0xff6a6a6a) : controlGrey());
  }

  void resized() override {
    auto r = getLocalBounds().reduced(2, 2);
    name_.setBounds(r.removeFromLeft(78).reduced(0, 20));
    trig_.setBounds(r.removeFromLeft(48).reduced(2, 24));
    level_.setBounds(r.removeFromLeft(86).reduced(4, 30));
    const int count = shogun_ui::voices()[static_cast<size_t>(index_)].count;
    for (int i = 0; i < count; ++i) {
      const auto n = static_cast<size_t>(i);
      auto cell = r.removeFromLeft(84);
      captions_[n].setBounds(cell.removeFromTop(14));
      knobs_[n].setBounds(cell.reduced(2, 0));
    }
  }

  juce::TextButton name_;
  juce::TextButton trig_;
  juce::Slider level_;
  std::array<juce::Label, 8> captions_;
  std::array<juce::Slider, 8> knobs_;

 private:
  int index_;
  ShogunAudioProcessor& proc_;
};

}  // namespace

struct ShogunAudioProcessorEditor::Plate {
  juce::LookAndFeel_V4 look;
  juce::TextButton intButton{"INT"};
  juce::TextButton extButton{"EXT"};
  juce::Label tempoLabel{"", "Tempo"};
  juce::Slider tempo;
  juce::Label masterLabel{"", "Master"};
  juce::Slider master;
  juce::Label scaleLabel{"", "Scale"};
  juce::ComboBox scale;
  juce::Label stepLabel;
  std::array<juce::TextButton, 16> steps;
  juce::Component list;
  juce::OwnedArray<VoiceRow> rows;
  juce::Viewport viewport;
  std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;
  int selected = 0;
};

ShogunAudioProcessorEditor::ShogunAudioProcessorEditor(ShogunAudioProcessor& proc)
    : AudioProcessorEditor(proc), proc_(proc), plate_(std::make_unique<Plate>()) {
  auto& ui = *plate_;
  ui.look.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe0e0e0));
  ui.look.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffe0e0e0));
  ui.look.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff555555));
  ui.look.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffeeeeee));
  ui.look.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
  ui.look.setColour(juce::TextButton::buttonColourId, controlGrey());
  ui.look.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff2f2f2));
  ui.look.setColour(juce::ComboBox::backgroundColourId, controlGrey());
  ui.look.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff2f2f2));
  ui.look.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff666666));
  ui.look.setColour(juce::Label::textColourId, juce::Colour(0xffdddddd));
  ui.look.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff777777));
  setLookAndFeel(&ui.look);

  ui.intButton.onClick = [this] {
    if (auto* param = proc_.apvts.getParameter("clock")) param->setValueNotifyingHost(0.0f);
    refreshMode();
  };
  ui.extButton.onClick = [this] {
    if (auto* param = proc_.apvts.getParameter("clock")) param->setValueNotifyingHost(1.0f);
    refreshMode();
  };
  addAndMakeVisible(ui.intButton);
  addAndMakeVisible(ui.extButton);

  ui.tempo.setSliderStyle(juce::Slider::LinearHorizontal);
  ui.tempo.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 18);
  ui.master.setSliderStyle(juce::Slider::LinearHorizontal);
  ui.master.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
  addAndMakeVisible(ui.tempoLabel);
  addAndMakeVisible(ui.tempo);
  addAndMakeVisible(ui.masterLabel);
  addAndMakeVisible(ui.master);
  addAndMakeVisible(ui.scaleLabel);
  ui.scale.addItem("8", 1);
  ui.scale.addItem("6", 2);
  ui.scale.addItem("4", 3);
  ui.scale.addItem("3", 4);
  addAndMakeVisible(ui.scale);

  ui.sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      proc_.apvts, "tempo", ui.tempo));
  ui.sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      proc_.apvts, "master", ui.master));
  ui.scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
      proc_.apvts, "scale", ui.scale);

  ui.stepLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(ui.stepLabel);
  for (int i = 0; i < 16; ++i) {
    ui.steps[static_cast<size_t>(i)].setButtonText(juce::String(i + 1));
    ui.steps[static_cast<size_t>(i)].onClick = [this, i] {
      proc_.toggleStep(plate_->selected, i);
      refreshSteps();
    };
    addAndMakeVisible(ui.steps[static_cast<size_t>(i)]);
  }

  for (int i = 0; i < 16; ++i) {
    auto* row = ui.rows.add(new VoiceRow(i, proc_));
    row->name_.onClick = [this, i] { selectVoice(i); };
    ui.list.addAndMakeVisible(row);
    const auto& spec = shogun_ui::voices()[static_cast<size_t>(i)];
    ui.sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc_.apvts, spec.levelId, row->level_));
    for (int k = 0; k < spec.count; ++k) {
      ui.sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          proc_.apvts, spec.knobs[static_cast<size_t>(k)].id, row->knobs_[static_cast<size_t>(k)]));
    }
  }

  ui.viewport.setViewedComponent(&ui.list, false);
  ui.viewport.setScrollBarsShown(true, false);
  addAndMakeVisible(ui.viewport);

  selectVoice(0);
  refreshMode();
  setSize(980, 640);
  startTimerHz(12);
}

ShogunAudioProcessorEditor::~ShogunAudioProcessorEditor() {
  setLookAndFeel(nullptr);
  if (plate_ != nullptr) plate_->viewport.setViewedComponent(nullptr, false);
}

void ShogunAudioProcessorEditor::paint(juce::Graphics& g) {
  g.fillAll(plateGrey());
  g.setColour(juce::Colour(0xfff4f4f4));
  g.setFont(juce::Font(juce::FontOptions(22.0f)));
  g.drawText("SHOGUN", 16, 8, 200, 26, juce::Justification::left);
  g.setColour(juce::Colour(0xffbdbdbd));
  g.setFont(juce::Font(juce::FontOptions(13.0f)));
  g.drawText("placeholder plate", 16, 32, 220, 16, juce::Justification::left);
  g.drawText("MIDI 36 to 51 plays the row. INT follows the steps. EXT waits for TRIG or MIDI.",
             250, 32, 700, 16, juce::Justification::left);
}

void ShogunAudioProcessorEditor::resized() {
  auto& ui = *plate_;
  auto area = getLocalBounds().reduced(10);
  area.removeFromTop(52);
  auto controls = area.removeFromTop(36);
  ui.intButton.setBounds(controls.removeFromLeft(58).reduced(2, 4));
  ui.extButton.setBounds(controls.removeFromLeft(58).reduced(2, 4));
  controls.removeFromLeft(8);
  ui.tempoLabel.setBounds(controls.removeFromLeft(48));
  ui.tempo.setBounds(controls.removeFromLeft(180).reduced(0, 6));
  ui.masterLabel.setBounds(controls.removeFromLeft(52));
  ui.master.setBounds(controls.removeFromLeft(160).reduced(0, 6));
  ui.scaleLabel.setBounds(controls.removeFromLeft(44));
  ui.scale.setBounds(controls.removeFromLeft(64).reduced(0, 6));

  auto steps = area.removeFromTop(40);
  ui.stepLabel.setBounds(steps.removeFromLeft(150).reduced(0, 8));
  for (auto& button : ui.steps) button.setBounds(steps.removeFromLeft(46).reduced(2, 6));

  ui.viewport.setBounds(area);
  const int rowW = juce::jmax(860, ui.viewport.getWidth() - 16);
  ui.list.setSize(rowW, 16 * kRowH);
  for (int i = 0; i < ui.rows.size(); ++i)
    ui.rows[i]->setBounds(0, i * kRowH, rowW, kRowH);
}

void ShogunAudioProcessorEditor::timerCallback() {
  refreshMode();
  refreshSteps();
}

void ShogunAudioProcessorEditor::selectVoice(int voice) {
  plate_->selected = voice;
  for (int i = 0; i < plate_->rows.size(); ++i) plate_->rows[i]->setSelected(i == voice);
  refreshSteps();
}

void ShogunAudioProcessorEditor::refreshMode() {
  const auto* clock = proc_.apvts.getRawParameterValue("clock");
  const int mode = clock == nullptr ? 0 : static_cast<int>(std::lround(clock->load()));
  auto paintButton = [](juce::TextButton& button, bool on, juce::Colour colour) {
    button.setToggleState(on, juce::dontSendNotification);
    button.setColour(juce::TextButton::buttonColourId, on ? colour : controlGrey());
  };
  paintButton(plate_->intButton, mode == 0, juce::Colour(0xff4e6b4e));
  paintButton(plate_->extButton, mode != 0, juce::Colour(0xff6b4e4e));
}

void ShogunAudioProcessorEditor::refreshSteps() {
  const int voice = plate_->selected;
  const auto& spec = shogun_ui::voices()[static_cast<size_t>(voice)];
  plate_->stepLabel.setText(juce::String(spec.name) + "  step " + juce::String(proc_.currentStep()),
                            juce::dontSendNotification);
  for (int i = 0; i < 16; ++i) {
    const bool on = proc_.isStepOn(voice, i);
    auto& button = plate_->steps[static_cast<size_t>(i)];
    button.setColour(juce::TextButton::buttonColourId, on ? juce::Colour(0xffdedede) : controlGrey());
    button.setColour(juce::TextButton::textColourOffId,
                     on ? juce::Colour(0xff111111) : juce::Colour(0xfff2f2f2));
  }
}
