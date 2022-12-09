#include <fmt/format.h>

#include <memory>

extern "C" {
#include <glib/gi18n.h>
#include <stdint.h>

#include "McBopomofoApi.h"
}

#include "../Key.h"
#include "../KeyHandler.h"
#include "../LanguageModelLoader.h"
#include "McBopomofoLM.h"

struct _McbpmfApiCore {
  GObject parent;
  McBopomofo::KeyHandler* keyhandler;
  std::unique_ptr<McBopomofo::InputState> state;
};

struct _McbpmfApiKeyDef {
  GObject parent;
  McBopomofo::Key body;
};
struct _McbpmfApiInputState {
  GObject parent;
  McBopomofo::InputState body;
};

class DummyUserPhraseAdder : public McBopomofo::UserPhraseAdder {
 public:
  void addUserPhrase([[maybe_unused]] const std::string_view& reading,
                     [[maybe_unused]] const std::string_view& phrase) {
  }
};

class KeyHandlerLocalizedString : public McBopomofo::KeyHandler::LocalizedStrings {
 public:
  std::string cursorIsBetweenSyllables(
    const std::string& prevReading, const std::string& nextReading) override {
    return fmt::format(_("Cursor is between syllables {0} and {1}"),
                       prevReading, nextReading);
  }

  std::string syllablesRequired(size_t syllables) override {
    return fmt::format(_("{0} syllables required"), std::to_string(syllables));
  }

  std::string syllablesMaximum(size_t syllables) override {
    return fmt::format(_("{0} syllables maximum"), std::to_string(syllables));
  }

  std::string phraseAlreadyExists() override {
    return _("phrase already exists");
  }

  std::string pressEnterToAddThePhrase() override {
    return _("press Enter to add the phrase");
  }

  std::string markedWithSyllablesAndStatus(const std::string& marked,
                                           const std::string& readingUiText,
                                           const std::string& status) override {
    return fmt::format(_("Marked: {0}, syllables: {1}, {2}"), marked,
                       readingUiText, status);
  }
};

#define __MSTATE_CAST_TO(x, t) (dynamic_cast<McBopomofo::InputStates::t*>(x))
#define __MSTATE_CASTABLE(x, t) (__MSTATE_CAST_TO(x, t) != nullptr)
McbpmfInputStateType mcbpmf_api_state_get_type(McbpmfApiInputState* _ptr) {
  auto ptr = &_ptr->body;
#define X(name, elt)                   \
  if (__MSTATE_CASTABLE(ptr, elt)) {   \
    return _MSTATE_TYPE_TO_ENUM(name); \
  }
  _MSTATE_TYPE_LIST
#undef X
  return MSTATE_TYPE_N;
}

McbpmfApiCore* mcbpmf_api_core_new(void) {
  // from McBopomofoEngine::constructor, set as initial state
  std::unique_ptr<McBopomofo::InputState> emptyState(new McBopomofo::InputStates::Empty);

  auto ret = reinterpret_cast<McbpmfApiCore*>(g_object_new(MCBPMF_API_TYPE_CORE, NULL));
  ret->state.swap(emptyState);
  return ret;
}

void mcbpmf_api_core_init(McbpmfApiCore* _core, const char* lm_path) {
  std::shared_ptr<McBopomofo::McBopomofoLM> lm(new McBopomofo::McBopomofoLM());
  std::shared_ptr<McBopomofo::UserPhraseAdder> upa(new DummyUserPhraseAdder());
  std::unique_ptr<McBopomofo::KeyHandler::LocalizedStrings> lstr(new KeyHandlerLocalizedString());

  lm->setPhraseReplacementEnabled(false);
  lm->setExternalConverterEnabled(false);

  // McBopomofoEngine::constructor(fcitx instance)
  // -- TODO: make lmmodelloader
  auto core = new McBopomofo::KeyHandler(lm, upa, std::move(lstr));

  // -- TODO: keyHandler::setOnAddNewPhrase and addScriptHook

  // McBopomofoEngine::activate(entry, event)
  // TODO: detect engine_name to set to PlainBopomofo
  assert(core->inputMode() == McBopomofo::InputMode::McBopomofo);

  // TODO: chttrans

  core->setKeyboardLayout(Formosa::Mandarin::BopomofoKeyboardLayout::StandardLayout());

  // TODO: set user config

  // -- TODO: languageModelLoader_->reloadUserModelsIfNeeded();
  //    user phrases & excluded phrases

  // -- languageModelLoader_->loadModelForMode(McBopomofo::InputMode::McBopomofo)
  lm->loadLanguageModel(lm_path);

  if (!lm->isDataModelLoaded()) {
    // TODO: needs to export error reason from upstream
    g_error("Language model cannot be loaded!!");
  }

  _core->keyhandler = core;
}

McbpmfApiInputState* mcbpmf_api_core_get_state(McbpmfApiCore* core) {
  return reinterpret_cast<McbpmfApiInputState*>(core->state.get());
}

void mcbpmf_api_core_set_state(McbpmfApiCore* core, McbpmfApiInputState* _state) {
  core->state.reset(&_state->body);
}

void mcbpmf_api_core_reset_state(McbpmfApiCore* core) {
  core->state.reset(new McBopomofo::InputStates::Empty());
}

// McBopomofoEngine::keyEvent
bool mcbpmf_api_keyhandler_handle_key(
  McbpmfApiCore* core, McbpmfApiKeyDef* key, McbpmfApiInputState** _result, bool* has_error) {
  // NOTE: go to candidate phase if state is InputStates::ChoosingCandidate
  // use "handleCandidateKeyEvent"

  auto result = reinterpret_cast<McBopomofo::InputState**>(&(*_result)->body);

  if (has_error) *has_error = false;

  bool accepted = core->keyhandler->handle(
    key->body, core->state.get(),
    [&](std::unique_ptr<McBopomofo::InputState> next) {
      *result = next.release();
      // TODO: identify the type of next state and act accordingly
      // McBopomofoEngine::enterNewState(context, std::move(next));
    },
    [&]() {
      *result = nullptr;
      if (has_error) *has_error = true;
    });

  return accepted;
}

// keyHandler::candidateSelected or keyHandler::candidatePanelCancelled
bool mcbpmf_api_core_select_candidate(
  McbpmfApiCore* core, int which, McbpmfApiInputState** _result) {
  auto statePtr = __MSTATE_CAST_TO(core->state.get(), ChoosingCandidate);
  if (statePtr == nullptr) return false;
  auto& cands = statePtr->candidates;

  auto result = reinterpret_cast<McBopomofo::InputState**>(&(*_result)->body);

  auto cb = [&](std::unique_ptr<McBopomofo::InputState> next) {
    *result = next.release();
  };

  if (which < 0) {
    // candidate list is dismissed
    core->keyhandler->candidatePanelCancelled(cb);
  } else {
    // which must be an index in the candidate list
    if (static_cast<size_t>(which) > cands.size()) return false;
    auto& cand = cands[which];
    core->keyhandler->candidateSelected(cand, cb);
  }

  return true;
}

#define __KEYNAME(k) McBopomofo::Key::KeyName::k
static const McBopomofo::Key::KeyName MKEY_LIST[] = {
  __KEYNAME(ASCII),
  __KEYNAME(LEFT),
  __KEYNAME(RIGHT),
  __KEYNAME(UP),
  __KEYNAME(DOWN),
  __KEYNAME(HOME),
  __KEYNAME(END),
  __KEYNAME(UNKNOWN),
};
#undef __KEYNAME
static constexpr int MKEY_LIST_LENGTH = sizeof(MKEY_LIST) / sizeof(McBopomofo::Key::KeyName);

McbpmfApiKeyDef* mcbpmf_api_keydef_new_ascii(char c, unsigned char mods) {
  auto p = new auto(McBopomofo::Key::asciiKey(c, mods & MKEY_SHIFT_MASK, mods & MKEY_CTRL_MASK));
  return reinterpret_cast<McbpmfApiKeyDef*>(p);
}

McbpmfApiKeyDef* mcbpmf_api_keydef_new_named(int mkey, unsigned char mods) {
  if (!(mkey > 0 && mkey < MKEY_LIST_LENGTH)) return nullptr;
  auto p = new auto(McBopomofo::Key::namedKey(MKEY_LIST[mkey], mods & MKEY_SHIFT_MASK, mods & MKEY_CTRL_MASK));
  return reinterpret_cast<McbpmfApiKeyDef*>(p);
}

void mcbpmf_api_keydef_destroy(McbpmfApiKeyDef* p) {
  delete &(p->body);
}

void mcbpmf_api_keyhandler_destroy(McbpmfApiCore* ptr) {
  delete ptr->keyhandler;
}

bool mcbpmf_api_state_is_empty(McbpmfApiInputState* _ptr) {
  auto ptr = &_ptr->body;
  // use "!__MSTATE_CASTABLE(ptr, NonEmpty)" will produce wrong result!
  return __MSTATE_CASTABLE(ptr, Empty) ||
         __MSTATE_CASTABLE(ptr, EmptyIgnoringPrevious) ||
         __MSTATE_CASTABLE(ptr, Committing);
}

const char* mcbpmf_api_state_peek_buf(McbpmfApiInputState* _state) {
  auto& state = _state->body;
  // McBopomofoEngine::handleInputtingState
  if (auto nonempty = __MSTATE_CAST_TO(&state, NotEmpty)) {
    return nonempty->composingBuffer.c_str();
    // cursorIndex, tooltip
  }
  if (auto committing = __MSTATE_CAST_TO(&state, Committing)) {
    return committing->text.c_str();
  }
  return nullptr;
}

int mcbpmf_api_state_peek_index(McbpmfApiInputState* _state) {
  auto& state = _state->body;
  // McBopomofoEngine::handleInputtingState
  if (auto nonempty = __MSTATE_CAST_TO(&state, NotEmpty)) {
    return nonempty->cursorIndex;
  }
  return -1;
}

GPtrArray* mcbpmf_api_state_get_candidates(McbpmfApiInputState* _state) {
  auto& state = _state->body;
  if (auto choosing = __MSTATE_CAST_TO(&state, ChoosingCandidate)) {
    auto& cs = choosing->candidates;
    // the container should be freed by consumer
    auto ret = g_ptr_array_new_full(cs.size() * 2, g_free);
    for (const auto& c : cs) {
      // FIXME: I don't know how to do this in C++ style
      g_ptr_array_add(ret, (gpointer)c.reading.c_str());
      g_ptr_array_add(ret, (gpointer)c.value.c_str());
    }
    return ret;
  }
  return nullptr;
}
