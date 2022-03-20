// Copyright (c) 2022 and onwards The McBopomofo Authors.
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_MCBOPOMOFO_H_
#define SRC_MCBOPOMOFO_H_

#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/i18n.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>

#include <memory>
#include <string>
#include <type_traits>

#include "InputState.h"
#include "KeyHandler.h"

namespace McBopomofo {

enum class BopomofoKeyboardLayout {
  Standard,
  Eten,
  Hsu,
  Et26,
  HanyuPinyin,
  IBM
};
FCITX_CONFIG_ENUM_NAME_WITH_I18N(BopomofoKeyboardLayout, N_("standard"),
                                 N_("eten"), N_("hsu"), N_("et26"),
                                 N_("hanyupinyin"), N_("ibm"));

FCITX_CONFIGURATION(
    McBopomofoConfig,
    // Keyboard layout: standard, eten, etc.

    fcitx::OptionWithAnnotation<BopomofoKeyboardLayout,
                                BopomofoKeyboardLayoutI18NAnnotation>
        bopomofoKeyboardLayout{this, "BopomofoKeyboardLayout",
                               _("Bopomofo Keyboard Layout"),
                               BopomofoKeyboardLayout::Standard};
    fcitx::Option<bool> convertsToSimplifiedChinese{
        this, "ConvertsToSimplifiedChinese",
        _("Converts to Simplified Chinese"), false};
    // Whether to map Dvorak characters back to Qwerty layout;
    // this is a workaround of fcitx5/wayland's limitations.
    // See https://bugzilla.gnome.org/show_bug.cgi?id=162726
    // TODO(unassigned): Remove this once fcitx5 handles Dvorak better.
    fcitx::Option<bool> mapsDvorakToQwerty{this, "MapDvorakToQWERTY",
                                           _("Map Dvorak to QWERTY"), false};);

class McBopomofoEngine : public fcitx::InputMethodEngine {
 public:
  McBopomofoEngine(fcitx::Instance* instance);
  fcitx::Instance* instance() { return instance_; }

  void activate(const fcitx::InputMethodEntry& entry,
                fcitx::InputContextEvent& event) override;
  void reset(const fcitx::InputMethodEntry& entry,
             fcitx::InputContextEvent& event) override;
  void keyEvent(const fcitx::InputMethodEntry& entry,
                fcitx::KeyEvent& keyEvent) override;

  const fcitx::Configuration* getConfig() const override;
  void setConfig(const fcitx::RawConfig& config) override;
  void reloadConfig() override;

 private:
  FCITX_ADDON_DEPENDENCY_LOADER(chttrans, instance_->addonManager());
  fcitx::Instance* instance_;

  void handleCandidateKeyEvent(fcitx::InputContext* context, fcitx::Key key,
                               fcitx::CommonCandidateList* candidateList);

  // Handles state transitions.
  void enterNewState(fcitx::InputContext* context,
                     std::unique_ptr<InputState> newState);

  // Methods below enterNewState raw pointers as they don't affect ownership.
  void handleEmptyState(fcitx::InputContext* context, InputState* prev,
                        InputStates::Empty* current);
  void handleEmptyIgnoringPreviousState(
      fcitx::InputContext* context, InputState* prev,
      InputStates::EmptyIgnoringPrevious* current);
  void handleCommittingState(fcitx::InputContext* context, InputState* prev,
                             InputStates::Committing* current);
  void handleInputtingState(fcitx::InputContext* context, InputState* prev,
                            InputStates::Inputting* current);
  void handleCandidatesState(fcitx::InputContext* context, InputState* prev,
                             InputStates::ChoosingCandidate* current);

  // Helpers.

  // Updates the preedit with a not-empty state's composing buffer and cursor
  // index.
  void updatePreedit(fcitx::InputContext* context,
                     InputStates::NotEmpty* state);

  // Commits the text to the context, applying any conversions along the way.
  void commitString(fcitx::InputContext* context, std::string text);

  std::unique_ptr<KeyHandler> keyHandler_;
  std::unique_ptr<InputState> state_;
  McBopomofoConfig config_;
  fcitx::KeyList selectionKeys_;
};

class McBopomofoEngineFactory : public fcitx::AddonFactory {
  fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
    FCITX_UNUSED(manager);
    return new McBopomofoEngine(manager->instance());
  }
};

}  // namespace McBopomofo

#endif  // SRC_MCBOPOMOFO_H_
