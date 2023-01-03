#include <fmt/format.h>

#include <memory>

extern "C" {
#include <glib/gi18n.h>
#include <stdint.h>

#include "McBopomofoApi.h"
}

#include "LocalizedStrings.hpp"
#include "UserPhraseAdderProxy.hpp"
#include "../Key.h"
#include "../KeyHandler.h"
#include "McBopomofoLM.h"

struct _McbpmfApiCore {
  GObject parent;
  McBopomofo::McBopomofoLM* model;
  McBopomofo::KeyHandler* keyhandler;
  std::unique_ptr<McBopomofo::InputState> state = nullptr;
};
G_DEFINE_TYPE(McbpmfApiCore, mcbpmf_api_core, G_TYPE_OBJECT)

typedef enum {
  CORE_PROP_INPUT_MODE = 1,
  CORE_PROP_KEYBOARD_LAYOUT,
  CORE_PROP_SELECT_CAND_AFTER_CURSOR,
  CORE_PROP_AUTO_ADVANCE_CURSOR,
  CORE_PROP_PUT_LCASE_LETTERS_TO_BUFFER,
  CORE_PROP_ESC_CLEARS_BUFFER,
  CORE_PROP_CTRL_ENTER_BEHAVIOR,
  CORE_PROP_N,
} McbpmfApiCoreProperty;

typedef enum {
  CORE_CUSTOM_PHRASE_ADDED,
  CORE_SIG_N,
} McbpmfApiCoreSignal;

static GParamSpec* mcbpmf_api_core_properties[CORE_PROP_N] = { nullptr, };
static guint mcbpmf_api_core_signals[CORE_SIG_N] = {};

struct _McbpmfApiKeyDef {
  McBopomofo::Key* body;
  gint ref_count;
};
G_DEFINE_BOXED_TYPE(McbpmfApiKeyDef, mcbpmf_api_keydef,
                    mcbpmf_api_keydef_ref, mcbpmf_api_keydef_unref)

struct _McbpmfApiInputState {
  McBopomofo::InputState* body;
  gint ref_count;
  bool is_static;
};
G_DEFINE_BOXED_TYPE(McbpmfApiInputState, mcbpmf_api_input_state,
                    mcbpmf_api_input_state_ref, mcbpmf_api_input_state_unref)

#define __MSTATE_CAST_TO(x, t) (dynamic_cast<McBopomofo::InputStates::t*>(x))
#define __MSTATE_CASTABLE(x, t) (__MSTATE_CAST_TO(x, t) != nullptr)

static void mcbpmf_api_core_set_property(McbpmfApiCore*, McbpmfApiCoreProperty, const GValue*, GParamSpec*);
static void mcbpmf_api_core_get_property(McbpmfApiCore*, McbpmfApiCoreProperty, GValue*, GParamSpec*);
static void mcbpmf_api_core_finalize(GObject*);

static void mcbpmf_api_core_class_init(McbpmfApiCoreClass* klass) {
  GObjectClass* object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = reinterpret_cast<GObjectSetPropertyFunc>(mcbpmf_api_core_set_property);
  object_class->get_property = reinterpret_cast<GObjectGetPropertyFunc>(mcbpmf_api_core_get_property);
  object_class->finalize = mcbpmf_api_core_finalize;

  mcbpmf_api_core_properties[CORE_PROP_INPUT_MODE] = g_param_spec_enum(
    "input-mode", "Input mode",
    "The input mode to use (classic or plainBopomofo mode).",
    MCBPMF_API_TYPE_INPUT_MODE, MCBPMF_API_INPUT_MODE_CLASSIC,
    G_PARAM_READWRITE);
  static_assert(MCBPMF_API_INPUT_MODE_CLASSIC == static_cast<gint>(McBopomofo::InputMode::McBopomofo));

  // keyboardLayout
  mcbpmf_api_core_properties[CORE_PROP_KEYBOARD_LAYOUT] = g_param_spec_enum(
    "keyboard-layout", "Keyboard layout",
    "The keyboard layout to use for typing bopomofo.",
    MCBPMF_API_TYPE_KEYBOARD_LAYOUT, MCBPMF_API_KEYBOARD_LAYOUT_STANDARD,
    G_PARAM_WRITABLE);

  // selectPhraseAfterCursorAsCandidate
  mcbpmf_api_core_properties[CORE_PROP_SELECT_CAND_AFTER_CURSOR] = g_param_spec_boolean(
    "select-cand-after-cursor", "Select phrase after cursor as candidate",
    "When initiating a candidate selection, use the character right at cursor as the starting point (default is the one before).",
    false,
    G_PARAM_WRITABLE);

  // moveCursorAfterSelection
  mcbpmf_api_core_properties[CORE_PROP_AUTO_ADVANCE_CURSOR] = g_param_spec_boolean(
    "auto-advance-cursor", "Advance cursor automatically after selection",
    "Whether to advance cursor immediately after selecting a candidate.",
    false,
    G_PARAM_WRITABLE);

  // putLowercaseLettersToComposingBuffer
  mcbpmf_api_core_properties[CORE_PROP_PUT_LCASE_LETTERS_TO_BUFFER] = g_param_spec_boolean(
    "put-lcase-letters-to-buffer", "Put lowercase letters to composing buffer",
    "Allow lowercase letters (a-z) to be put into the composing buffer.",
    false,
    G_PARAM_WRITABLE);

  // escKeyClearsEntireComposingBuffer
  mcbpmf_api_core_properties[CORE_PROP_ESC_CLEARS_BUFFER] = g_param_spec_boolean(
    "esc-clears-buffer", "Esc key clears entire composing buffer",
    "When the composing buffer is non-empty, pressing esc clears it.",
    false,
    G_PARAM_WRITABLE);

  // ctrlEnterKey
  mcbpmf_api_core_properties[CORE_PROP_CTRL_ENTER_BEHAVIOR] = g_param_spec_enum(
    "ctrl-enter-behavior", "Behavior of Ctrl-Enter key",
    "What to do with the composing buffer when Ctrl-Enter is pressed.",
    MCBPMF_API_TYPE_CTRL_ENTER_BEHAVIOR, MCBPMF_API_CTRL_ENTER_NOOP,
    G_PARAM_WRITABLE);
  static_assert(MCBPMF_API_CTRL_ENTER_NOOP == static_cast<gint>(McBopomofo::KeyHandlerCtrlEnter::Disabled));

  g_object_class_install_properties(object_class,
    CORE_PROP_N,
    mcbpmf_api_core_properties);

  mcbpmf_api_core_signals[CORE_CUSTOM_PHRASE_ADDED] = g_signal_new(
    "custom-phrase-added",
    G_TYPE_FROM_CLASS(klass),
    G_SIGNAL_RUN_LAST,
    0,
    nullptr, nullptr,
    /* marshaller */ nullptr,
    G_TYPE_NONE,
    2,
    G_TYPE_STRING, G_TYPE_STRING);
}

static void mcbpmf_api_core_init(McbpmfApiCore* core) {
  new(core) McbpmfApiCore;
}

static auto _to_formosana_keyboard_layout(gint index) {
  #define _BPMF_KB_LAYOUT(x) (Formosa::Mandarin::BopomofoKeyboardLayout::x())
  static const Formosa::Mandarin::BopomofoKeyboardLayout* coll[] = {
    _BPMF_KB_LAYOUT(StandardLayout),
    _BPMF_KB_LAYOUT(ETenLayout),
    _BPMF_KB_LAYOUT(HsuLayout),
    _BPMF_KB_LAYOUT(ETen26Layout),
    _BPMF_KB_LAYOUT(HanyuPinyinLayout),
    _BPMF_KB_LAYOUT(IBMLayout),
  };
  #undef _BPMF_KB_LAYOUT
  return coll[index];
}

static void mcbpmf_api_core_set_property(
  McbpmfApiCore* core, McbpmfApiCoreProperty prop_id, const GValue* value, GParamSpec* pspec) {
  switch (prop_id) {
  case CORE_PROP_INPUT_MODE:
    core->keyhandler->setInputMode(static_cast<McBopomofo::InputMode>(g_value_get_enum(value)));
    break;
  case CORE_PROP_KEYBOARD_LAYOUT:
    core->keyhandler->setKeyboardLayout(_to_formosana_keyboard_layout(g_value_get_enum(value)));
    break;
  case CORE_PROP_SELECT_CAND_AFTER_CURSOR:
    core->keyhandler->setSelectPhraseAfterCursorAsCandidate(g_value_get_boolean(value));
    break;
  case CORE_PROP_AUTO_ADVANCE_CURSOR:
    core->keyhandler->setMoveCursorAfterSelection(g_value_get_boolean(value));
    break;
  case CORE_PROP_PUT_LCASE_LETTERS_TO_BUFFER:
    core->keyhandler->setPutLowercaseLettersToComposingBuffer(g_value_get_boolean(value));
    break;
  case CORE_PROP_ESC_CLEARS_BUFFER:
    core->keyhandler->setEscKeyClearsEntireComposingBuffer(g_value_get_boolean(value));
    break;
  case CORE_PROP_CTRL_ENTER_BEHAVIOR:
    core->keyhandler->setCtrlEnterKeyBehavior(static_cast<McBopomofo::KeyHandlerCtrlEnter>(g_value_get_enum(value)));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(core, prop_id, pspec);
  }
}

static void mcbpmf_api_core_get_property(
  McbpmfApiCore* core, McbpmfApiCoreProperty prop_id, GValue* value, GParamSpec* pspec) {
  switch (prop_id) {
  case CORE_PROP_INPUT_MODE:
    g_value_set_enum(value, static_cast<gint>(core->keyhandler->inputMode()));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(core, prop_id, pspec);
  }
}

void UserPhraseAdderProxy::addUserPhrase(const std::string_view& reading, const std::string_view& value) {
  auto sr = g_strndup(reading.data(), reading.length());
  auto sv = g_strndup(value.data(), value.length());
  g_signal_emit(this->core_, mcbpmf_api_core_signals[CORE_CUSTOM_PHRASE_ADDED],
    0, sr, sv);
  g_free(sr);
  g_free(sv);
}

McbpmfApiCore* mcbpmf_api_core_new(void) {
  auto core = reinterpret_cast<McbpmfApiCore*>(g_object_new(MCBPMF_API_TYPE_CORE, nullptr));

  std::shared_ptr<McBopomofo::McBopomofoLM> lm = std::make_shared<McBopomofo::McBopomofoLM>();
  std::shared_ptr<McBopomofo::UserPhraseAdder> upa = std::make_shared<UserPhraseAdderProxy>(core);
  std::unique_ptr<McBopomofo::KeyHandler::LocalizedStrings> lstr = std::make_unique<KeyHandlerLocalizedStrings>();

  // configure lm
  lm->setPhraseReplacementEnabled(false);
  lm->setExternalConverterEnabled(false);

  // configure keyhandler
  // -- TODO: make lmmodelloader
  auto keyhandler = new McBopomofo::KeyHandler(lm, upa, std::move(lstr));

  // TODO: set user config

  // -- TODO: chttrans
  keyhandler->setKeyboardLayout(Formosa::Mandarin::BopomofoKeyboardLayout::StandardLayout());

  // -- TODO: keyHandler::addScriptHook
  keyhandler->setOnAddNewPhrase([core]([[maybe_unused]] const std::string& phrase) {});

  // -- TODO: user phrases & excluded phrases

  // configure core
  core->model = lm.get();
  core->keyhandler = keyhandler;
  // -- from McBopomofoEngine::constructor, set as initial state
  core->state.reset(new McBopomofo::InputStates::Empty());
  return core;
}

gboolean mcbpmf_api_core_load_model(McbpmfApiCore* core, const char* lm_path) {
  // McBopomofoEngine::activate(entry, event)
  // TODO: detect engine_name to set to PlainBopomofo
  assert(core->keyhandler->inputMode() == McBopomofo::InputMode::McBopomofo);

  // -- languageModelLoader_->loadModelForMode(McBopomofo::InputMode::McBopomofo)
  core->model->loadLanguageModel(lm_path);

  // -- TODO: languageModelLoader_->reloadUserModelsIfNeeded();

  // TODO: needs to export error reason from upstream
  return core->model->isDataModelLoaded();
}

static void mcbpmf_api_core_finalize(GObject* _ptr) {
  auto *ptr = G_TYPE_CHECK_INSTANCE_CAST(_ptr, MCBPMF_API_TYPE_CORE, McbpmfApiCore);
  ptr->model = nullptr;
  delete ptr->keyhandler;
  ptr->~McbpmfApiCore();
  G_OBJECT_CLASS(mcbpmf_api_core_parent_class)->finalize(_ptr);
}

McbpmfApiInputState* mcbpmf_api_core_get_state(McbpmfApiCore* core) {
  auto p = g_slice_new0(McbpmfApiInputState);
  p->body = core->state.get();
  p->ref_count = 1;
  p->is_static = true;
  return p;
}

void mcbpmf_api_core_set_state(McbpmfApiCore* core, McbpmfApiInputState* _state) {
  _state->is_static = true;
  core->state.reset(_state->body);
  mcbpmf_api_input_state_unref(_state);
}

void mcbpmf_api_core_reset_state(McbpmfApiCore* core, gboolean full_reset = true) {
  core->state.reset(new McBopomofo::InputStates::Empty());
  if (full_reset) {
    core->keyhandler->reset();
  }
}

// this method is private
// size_t mcbpmf_api_key_handler_get_actual_cursor_pos(key_handler_t* khdl) {
//   return khdl->real->actualCandidateCursorIndex();
// }

// McBopomofoEngine::keyEvent
gboolean mcbpmf_api_core_handle_key(
  McbpmfApiCore* core, McbpmfApiKeyDef* key, McbpmfApiInputState** result, gboolean* has_error) {
  // NOTE: go to candidate phase if state is InputStates::ChoosingCandidate
  // use "handleCandidateKeyEvent"

  if (has_error) *has_error = false;

  bool accepted = core->keyhandler->handle(
    *key->body, core->state.get(),
    [&](std::unique_ptr<McBopomofo::InputState> next) {
      if (*result != nullptr) { mcbpmf_api_input_state_unref(*result); }
      *result = g_slice_new0(McbpmfApiInputState);
      (*result)->body = next.release();
      (*result)->ref_count = 1;
      // TODO: the callback function maybe called more than once
      // McBopomofoEngine::enterNewState(context, std::move(next));
    },
    [&]() {
      if (*result != nullptr) { mcbpmf_api_input_state_unref(*result); }
      *result = nullptr;
      if (has_error) *has_error = true;
    });

  return accepted;
}

// keyHandler::candidateSelected or keyHandler::candidatePanelCancelled
gboolean mcbpmf_api_core_select_candidate(
  McbpmfApiCore* core, int which, McbpmfApiInputState** result) {
  auto statePtr = __MSTATE_CAST_TO(core->state.get(), ChoosingCandidate);
  if (statePtr == nullptr) return false;

  auto cb = [&](std::unique_ptr<McBopomofo::InputState> next) {
    if (*result != nullptr) { mcbpmf_api_input_state_unref(*result); }
    *result = g_slice_new0(McbpmfApiInputState);
    (*result)->body = next.release();
    (*result)->ref_count = 1;
  };

  if (which < 0) {
    // candidate list is dismissed
    core->keyhandler->candidatePanelCancelled(cb);
  } else {
    auto& cands = statePtr->candidates;
    // which must be an index in the candidate list
    if (static_cast<size_t>(which) >= cands.size()) return false;
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

McbpmfApiKeyDef* mcbpmf_api_keydef_ref(McbpmfApiKeyDef* p) {
  g_return_val_if_fail(p != nullptr, nullptr);
  g_return_val_if_fail(p->ref_count > 0, nullptr);
  g_atomic_int_inc(&p->ref_count);
  return p;
}
void mcbpmf_api_keydef_unref(McbpmfApiKeyDef* p) {
  g_return_if_fail(p != nullptr);
  g_return_if_fail(p->ref_count > 0);
  if (g_atomic_int_dec_and_test(&p->ref_count)) {
    delete p->body;
    g_slice_free(McbpmfApiKeyDef, p);
  }
}

McbpmfApiKeyDef* mcbpmf_api_keydef_new_ascii(char c, unsigned char mods) {
  McbpmfApiKeyDef* p = g_slice_new0(McbpmfApiKeyDef);
  p->body = new auto(McBopomofo::Key::asciiKey(c,
    mods & MCBPMF_API_KEY_SHIFT_MASK,
    mods & MCBPMF_API_KEY_CTRL_MASK));
  p->ref_count = 1;
  return p;
}

McbpmfApiKeyDef* mcbpmf_api_keydef_new_named(int mkey, unsigned char mods) {
  g_return_val_if_fail(mkey > 0 && mkey < MKEY_LIST_LENGTH, nullptr);
  McbpmfApiKeyDef* p = g_slice_new0(McbpmfApiKeyDef);
  p->body = new auto(McBopomofo::Key::namedKey(MKEY_LIST[mkey],
    mods & MCBPMF_API_KEY_SHIFT_MASK,
    mods & MCBPMF_API_KEY_CTRL_MASK));
  p->ref_count = 1;
  return p;
}

McbpmfApiInputStateType mcbpmf_api_state_get_type(McbpmfApiInputState* _state) {
  auto ptr = _state->body;
  auto eclass = reinterpret_cast<GEnumClass*>(
    g_type_class_ref(mcbpmf_api_input_state_type_get_type()));
#define X(name, elt)                                   \
  if (__MSTATE_CASTABLE(ptr, elt)) {                   \
    auto t = McbpmfApiInputStateType(                  \
       g_enum_get_value_by_nick(eclass, #elt)->value); \
    g_type_class_unref(eclass);                        \
    return t;                                          \
  }
  _MSTATE_TYPE_LIST
#undef X
  g_type_class_unref(eclass);
  return MSTATE_TYPE_N;
}

McbpmfApiInputState* mcbpmf_api_input_state_ref(McbpmfApiInputState* p) {
  g_return_val_if_fail(p != nullptr, nullptr);
  g_return_val_if_fail(p->ref_count > 0, nullptr);
  g_atomic_int_inc(&p->ref_count);
  return p;
}

void mcbpmf_api_input_state_unref(McbpmfApiInputState* p) {
  g_return_if_fail(p != nullptr);
  g_return_if_fail(p->ref_count > 0);
  if (g_atomic_int_dec_and_test(&p->ref_count)) {
    if (!p->is_static) delete p->body;
    g_slice_free(McbpmfApiInputState, p);
  }
}

// FIXME: it is hard to decouple the input state implementation from the
// key handler, so we leave it an opaque pointer to the comsumer. Ideally
// it should be a boxed type in terms of GObject.
gboolean mcbpmf_api_state_is_empty(McbpmfApiInputState* _state) {
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
