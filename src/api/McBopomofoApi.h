#ifndef __MCBOPOMOFO_API_H_
#define __MCBOPOMOFO_API_H_

#include <glib-object.h>
#include <glib.h>
#include "McBopomofoApiEnums.h"

#define MCBPMF_API_TYPE_CORE (mcbpmf_api_core_get_type())
G_DECLARE_FINAL_TYPE(McbpmfApiCore, mcbpmf_api_core, MCBPMF_API, CORE, GObject)

#define MCBPMF_API_TYPE_KEYDEF (mcbpmf_api_keydef_get_type())
#define MCBPMF_API_TYPE_INPUT_STATE (mcbpmf_api_input_state_get_type())

G_BEGIN_DECLS

typedef struct _McbpmfApiCore McbpmfApiCore;
typedef struct _McbpmfApiKeyDef McbpmfApiKeyDef;
typedef struct _McbpmfApiInputState McbpmfApiInputState;

McbpmfApiCore* mcbpmf_api_core_new(void);
gboolean mcbpmf_api_core_load_model(McbpmfApiCore*, const char* lm_path);

McbpmfApiInputState* mcbpmf_api_core_get_state(McbpmfApiCore*);
void mcbpmf_api_core_set_state(McbpmfApiCore*, McbpmfApiInputState*);
void mcbpmf_api_core_reset_state(McbpmfApiCore*, gboolean full_reset);
gboolean mcbpmf_api_core_handle_key(McbpmfApiCore*, McbpmfApiKeyDef*, McbpmfApiInputState**, gboolean*);
gboolean mcbpmf_api_core_select_candidate(McbpmfApiCore*, int, McbpmfApiInputState**);

McbpmfApiKeyDef* mcbpmf_api_keydef_ref(McbpmfApiKeyDef* p);
void mcbpmf_api_keydef_unref(McbpmfApiKeyDef* p);
McbpmfApiKeyDef* mcbpmf_api_keydef_new_ascii(char c, unsigned char mods);
McbpmfApiKeyDef* mcbpmf_api_keydef_new_named(int mkey, unsigned char mods);

McbpmfApiInputState* mcbpmf_api_input_state_ref(McbpmfApiInputState* p);
void mcbpmf_api_input_state_unref(McbpmfApiInputState* p);

McbpmfApiInputStateType mcbpmf_api_state_get_type(McbpmfApiInputState*);
gboolean mcbpmf_api_state_is_empty(McbpmfApiInputState*);
const char* mcbpmf_api_state_peek_buf(McbpmfApiInputState*);
int mcbpmf_api_state_peek_index(McbpmfApiInputState*);
const char* mcbpmf_api_state_peek_tooltip(McbpmfApiInputState*);
GPtrArray* mcbpmf_api_state_get_candidates(McbpmfApiInputState*);

G_END_DECLS

#endif /* __MCBOPOMOFO_API_H_ */
