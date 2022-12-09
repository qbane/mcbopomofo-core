#include <fmt/format.h>

#include <memory>

extern "C" {
#include <glib/gi18n.h>
#include <stdint.h>

#include "McBopomofoApi.h"
}

#include "LocalizedStrings.hpp"
#include "../Key.h"
#include "../KeyHandler.h"
#include "../LanguageModelLoader.h"
#include "McBopomofoLM.h"

struct _McbpmfApiCore {
  GObject parent;
  McBopomofo::KeyHandler* keyhandler;
  std::unique_ptr<McBopomofo::InputState> state;
  // const char* lm_path;
};
G_DEFINE_TYPE(McbpmfApiCore, mcbpmf_api_core, G_TYPE_OBJECT)

struct _McbpmfApiKeyDef {
  GObject parent;
  McBopomofo::Key* body;
};
G_DEFINE_TYPE(McbpmfApiKeyDef, mcbpmf_api_keydef, G_TYPE_OBJECT)

struct _McbpmfApiInputState {
  GObject parent;
  McBopomofo::InputState* body;
};
G_DEFINE_TYPE(McbpmfApiInputState, mcbpmf_api_input_state, G_TYPE_OBJECT)

#define __MSTATE_CAST_TO(x, t) (dynamic_cast<McBopomofo::InputStates::t*>(x))
#define __MSTATE_CASTABLE(x, t) (__MSTATE_CAST_TO(x, t) != nullptr)

static void mcbpmf_api_core_class_init(McbpmfApiCoreClass*) {}
static void mcbpmf_api_core_init(McbpmfApiCore*) {}

McbpmfApiCore* mcbpmf_api_core_new(void) {
  // from McBopomofoEngine::constructor, set as initial state
  std::unique_ptr<McBopomofo::InputState> emptyState(new McBopomofo::InputStates::Empty);

  auto ret = reinterpret_cast<McbpmfApiCore*>(g_object_new(MCBPMF_API_TYPE_CORE, NULL));
  ret->state.swap(emptyState);
  // ret->lm_path = lm_path;
  return ret;
}

void mcbpmf_api_core_init_(McbpmfApiCore* _core, const char* lm_path) {
  std::shared_ptr<McBopomofo::McBopomofoLM> lm(new McBopomofo::McBopomofoLM());
  std::shared_ptr<McBopomofo::UserPhraseAdder> upa(new DummyUserPhraseAdder());
  std::unique_ptr<McBopomofo::KeyHandler::LocalizedStrings> lstr(new KeyHandlerLocalizedStrings());

  lm->setPhraseReplacementEnabled(false);
  lm->setExternalConverterEnabled(false);

  // McBopomofoEngine::constructor(fcitx instance)
  // -- TODO: make lmmodelloader
  auto core = new McBopomofo::KeyHandler(lm, upa, std::move(lstr));

  // -- TODO: keyHandler::setOnAddNewPhrase and addScriptHook
  core->setOnAddNewPhrase([](const std::string& phrase) {
    std::cerr << fmt::format("khdl callback: added new phrase [{0}] !!", phrase) << std::endl;
  });

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

void mcbpmf_api_core_destroy(McbpmfApiCore* ptr) {
  delete ptr->state.release();
  delete ptr->keyhandler;
}

McbpmfApiInputState* mcbpmf_api_core_get_state(McbpmfApiCore* core) {
  return mcbpmf_api_input_state_new(core->state.get());
}

void mcbpmf_api_core_set_state(McbpmfApiCore* core, McbpmfApiInputState* _state) {
  core->state.reset(_state->body);
}

void mcbpmf_api_core_reset_state(McbpmfApiCore* core, bool full_reset) {
  core->state.reset(new McBopomofo::InputStates::Empty());
  if (full_reset) {
    core->keyhandler->reset();
  }
}

// this method is private
// size_t mcbpmf_api_key_handler_get_actual_cursor_pos(key_handler_t* khdl) {
//   return khdl->real->actualCandidateCursorIndex();
// }

static void mcbpmf_api_input_state_class_init(McbpmfApiInputStateClass*) {}
static void mcbpmf_api_input_state_init(McbpmfApiInputState*) {}

static McbpmfApiInputState* mcbpmf_api_input_state_new(void* s) {
  auto p = reinterpret_cast<McbpmfApiInputState*>(g_object_new(MCBPMF_API_TYPE_INPUT_STATE, NULL));
  p->body = reinterpret_cast<McBopomofo::InputState*>(s);
  return p;
}

// McBopomofoEngine::keyEvent
bool mcbpmf_api_core_handle_key(
  McbpmfApiCore* core, McbpmfApiKeyDef* key, McbpmfApiInputState** result, bool* has_error) {
  // NOTE: go to candidate phase if state is InputStates::ChoosingCandidate
  // use "handleCandidateKeyEvent"

  if (has_error) *has_error = false;

  bool accepted = core->keyhandler->handle(
    *key->body, core->state.get(),
    [&](std::unique_ptr<McBopomofo::InputState> next) {
      if (*result) g_object_unref(reinterpret_cast<GObject*>(*result));
      *result = mcbpmf_api_input_state_new(next.release());
      // TODO: the callback function maybe called more than once
      // McBopomofoEngine::enterNewState(context, std::move(next));
    },
    [&]() {
      if (*result) g_object_unref(reinterpret_cast<GObject*>(*result));
      *result = nullptr;
      if (has_error) *has_error = true;
    });

  return accepted;
}

// keyHandler::candidateSelected or keyHandler::candidatePanelCancelled
bool mcbpmf_api_core_select_candidate(
  McbpmfApiCore* core, int which, McbpmfApiInputState** result) {
  auto statePtr = __MSTATE_CAST_TO(core->state.get(), ChoosingCandidate);
  if (statePtr == nullptr) return false;
  auto& cands = statePtr->candidates;

  auto cb = [&](std::unique_ptr<McBopomofo::InputState> next) {
    if (*result) g_object_unref(reinterpret_cast<GObject*>(*result));
    *result = mcbpmf_api_input_state_new(next.release());
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

static void mcbpmf_api_keydef_class_init(McbpmfApiKeyDefClass*) {}
static void mcbpmf_api_keydef_init(McbpmfApiKeyDef*) {}

McbpmfApiKeyDef* mcbpmf_api_keydef_new_ascii(char c, unsigned char mods) {
  auto p = reinterpret_cast<McbpmfApiKeyDef*>(g_object_new(MCBPMF_API_TYPE_KEYDEF, NULL));
  p->body = new auto(McBopomofo::Key::asciiKey(c, mods & MKEY_SHIFT_MASK, mods & MKEY_CTRL_MASK));
  return p;
}

McbpmfApiKeyDef* mcbpmf_api_keydef_new_named(int mkey, unsigned char mods) {
  if (!(mkey > 0 && mkey < MKEY_LIST_LENGTH)) return nullptr;
  auto p = reinterpret_cast<McbpmfApiKeyDef*>(g_object_new(MCBPMF_API_TYPE_KEYDEF, NULL));
  p->body = new auto(McBopomofo::Key::namedKey(MKEY_LIST[mkey], mods & MKEY_SHIFT_MASK, mods & MKEY_CTRL_MASK));
  return p;
}

void mcbpmf_api_keydef_destroy(McbpmfApiKeyDef* p) {
  delete p->body;
}

McbpmfInputStateType mcbpmf_api_state_get_type(McbpmfApiInputState* _ptr) {
  auto ptr = _ptr->body;
#define X(name, elt)                   \
  if (__MSTATE_CASTABLE(ptr, elt)) {   \
    return _MSTATE_TYPE_TO_ENUM(name); \
  }
  _MSTATE_TYPE_LIST
#undef X
  return MSTATE_TYPE_N;
}

bool mcbpmf_api_state_is_empty(McbpmfApiInputState* _state) {
  auto state = _state->body;
  // use "!__MSTATE_CASTABLE(state, NonEmpty)" will produce wrong result!
  return __MSTATE_CASTABLE(state, Empty) ||
         __MSTATE_CASTABLE(state, EmptyIgnoringPrevious) ||
         __MSTATE_CASTABLE(state, Committing);
}

// TODO: serialize peek_xxx into gvariants to prevent multiple castings
const char* mcbpmf_api_state_peek_buf(McbpmfApiInputState* _state) {
  auto state = _state->body;
  // McBopomofoEngine::handleInputtingState
  if (auto nonempty = __MSTATE_CAST_TO(state, NotEmpty)) {
    return nonempty->composingBuffer.c_str();
    // cursorIndex, tooltip
  }
  if (auto committing = __MSTATE_CAST_TO(state, Committing)) {
    return committing->text.c_str();
  }
  return nullptr;
}

int mcbpmf_api_state_peek_index(McbpmfApiInputState* _state) {
  auto state = _state->body;
  // McBopomofoEngine::handleInputtingState
  if (auto nonempty = __MSTATE_CAST_TO(state, NotEmpty)) {
    return nonempty->cursorIndex;
  }
  return -1;
}

const char* mcbpmf_api_state_peek_tooltip(McbpmfApiInputState* _state) {
  auto state = _state->body;
  if (auto nonempty = __MSTATE_CAST_TO(state, NotEmpty)) {
    return nonempty->tooltip.c_str();
  }
  return nullptr;
}

// TODO: return a slice of it
GPtrArray* mcbpmf_api_state_get_candidates(McbpmfApiInputState* _state) {
  auto state = _state->body;
  if (auto choosing = __MSTATE_CAST_TO(state, ChoosingCandidate)) {
    auto& cs = choosing->candidates;
    auto ret = g_ptr_array_new_full(cs.size() * 2, nullptr);
    for (const auto& c : cs) {
      // FIXME: I don't know how to do this in C++ style
      g_ptr_array_add(ret, (gpointer)c.reading.c_str());
      g_ptr_array_add(ret, (gpointer)c.value.c_str());
    }
    return ret;
  }
  return nullptr;
}
