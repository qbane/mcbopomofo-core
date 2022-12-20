#ifndef __MCBOPOMOFO_API_ENUMS_H_
#define __MCBOPOMOFO_API_ENUMS_H_

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
  MCBPMF_API_KEY_SHIFT_MASK = 1 << 0,
  MCBPMF_API_KEY_CTRL_MASK = 1 << 1,
} McbpmfApiKeyModifiers;

GType mcbpmf_api_key_modifiers_get_type(void);
#define MCBPMF_API_TYPE_KEY_MODIFIERS (mcbpmf_api_key_modifiers_get_type())

#define _MSTATE_TYPE_LIST          \
  X(EMPTY, Empty)                  \
  X(EIP, EmptyIgnoringPrevious)    \
  X(COMMITTING, Committing)        \
  X(INPUTTING, Inputting)          \
  X(CANDIDATES, ChoosingCandidate) \
  X(MARKING, Marking)

typedef enum {
#define X(name, _) MSTATE_TYPE_ ## name,
  _MSTATE_TYPE_LIST
    MSTATE_TYPE_N
#undef X
} McbpmfApiInputStateType;

GType mcbpmf_api_input_state_type_get_type(void);
#define MCBPMF_API_TYPE_INPUT_STATE_TYPE (mcbpmf_api_input_state_type_get_type())

typedef enum {
  MCBPMF_API_CTRL_ENTER_NOOP,
  MCBPMF_API_CTRL_ENTER_OUTPUT_BPMF_READINGS,
  MCBPMF_API_CTRL_ENTER_OUTPUT_HTML_RUBY_TEXT,
} McbpmfApiCtrlEnterBehavior;

GType mcbpmf_api_ctrl_enter_behavior_get_type(void);
#define MCBPMF_API_TYPE_CTRL_ENTER_BEHAVIOR (mcbpmf_api_ctrl_enter_behavior_get_type())


G_END_DECLS

#endif  /* __MCBOPOMOFO_API_ENUMS_H_ */
