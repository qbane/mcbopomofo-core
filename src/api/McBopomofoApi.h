#ifndef __MCBOPOMOFO_API_H_
#define __MCBOPOMOFO_API_H_

#include <glib-object.h>
#include <glib.h>

#define MCBPMF_API_TYPE_CORE (mcbpmf_api_core_get_type())
G_DECLARE_FINAL_TYPE(McbpmfApiCore, mcbpmf_api_core, MCBPMF_API, CORE, GObject)

#define MCBPMF_API_TYPE_KEYDEF (mcbpmf_api_keydef_get_type())
G_DECLARE_FINAL_TYPE(McbpmfApiKeyDef, mcbpmf_api_keydef, MCBPMF_API, KEYDEF, GObject)

#define MCBPMF_API_TYPE_INPUT_STATE (mcbpmf_api_input_state_get_type())
G_DECLARE_FINAL_TYPE(McbpmfApiInputState, mcbpmf_api_input_state, MCBPMF_API, INPUT_STATE, GObject)

G_BEGIN_DECLS

typedef struct _McbpmfApiCore McbpmfApiCore;
typedef struct _McbpmfApiKeyDef McbpmfApiKeyDef;
typedef struct _McbpmfApiInputState McbpmfApiInputState;

typedef enum {
  MKEY_SHIFT_MASK = 1 << 0,
  MKEY_CTRL_MASK = 1 << 1,
} McbpmfApiKeyModifiers;

McbpmfApiCore* mcbpmf_api_core_new(void);
void mcbpmf_api_core_init(McbpmfApiCore*, const char*);
void mcbpmf_api_core_destroy(McbpmfApiCore*);

McbpmfApiInputState* mcbpmf_api_core_get_state(McbpmfApiCore*);
void mcbpmf_api_core_set_state(McbpmfApiCore*, McbpmfApiInputState*);
void mcbpmf_api_core_reset_state(McbpmfApiCore*);
bool mcbpmf_api_core_handle_key(McbpmfApiCore*, McbpmfApiKeyDef*, McbpmfApiInputState**, bool*);
bool mcbpmf_api_core_select_candidate(McbpmfApiCore*, int, McbpmfApiInputState**);

McbpmfApiKeyDef* mcbpmf_api_keydef_new_ascii(char c, unsigned char mods);
McbpmfApiKeyDef* mcbpmf_api_keydef_new_named(int mkey, unsigned char mods);
void mcbpmf_api_keydef_destroy(McbpmfApiKeyDef*);

#define _MSTATE_TYPE_LIST          \
  X(EMPTY, Empty)                  \
  X(EIP, EmptyIgnoringPrevious)    \
  X(COMMITTING, Committing)        \
  X(INPUTTING, Inputting)          \
  X(CANDIDATES, ChoosingCandidate) \
  X(MARKING, Marking)

#define _MSTATE_TYPE_TO_ENUM(name) G_PASTE(MSTATE_TYPE_, name)

typedef enum {
#define X(name, _) _MSTATE_TYPE_TO_ENUM(name),
  _MSTATE_TYPE_LIST
    MSTATE_TYPE_N
#undef X
} McbpmfInputStateType;

McbpmfInputStateType mcbpmf_api_state_get_type(McbpmfApiInputState*);
bool mcbpmf_api_state_is_empty(McbpmfApiInputState*);
const char* mcbpmf_api_state_peek_buf(McbpmfApiInputState*);
int mcbpmf_api_state_peek_index(McbpmfApiInputState*);
GPtrArray* mcbpmf_api_state_get_candidates(McbpmfApiInputState*);

G_END_DECLS

#endif /* __MCBOPOMOFO_API_H_ */
