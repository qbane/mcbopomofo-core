
#include "McBopomofoApiEnums.h"

// Hopefully they will be available in genums.h after glib 2.74+
// See https://github.com/endlessm/xapian-glib/blob/7b50cb8f0fdad8ad3297e4c37e321ed254212a43/xapian-glib/xapian-enums.cc
// for an early implementation.
#ifndef G_DEFINE_ENUM_VALUE

#define G_DEFINE_ENUM_VALUE(EnumValue, EnumNick) \
  { EnumValue, #EnumValue, EnumNick }

#define G_DEFINE_ENUM_TYPE(TypeName, type_name, ...) \
GType \
type_name ## _get_type (void) { \
  static gsize g_define_type__static = 0; \
  if (g_once_init_enter (&g_define_type__static)) { \
    static const GEnumValue enum_values[] = { \
      __VA_ARGS__ , \
      { 0, NULL, NULL }, \
    }; \
    GType g_define_type = g_enum_register_static (g_intern_static_string (#TypeName), enum_values); \
    g_once_init_leave (&g_define_type__static, g_define_type); \
  } \
  return g_define_type__static; \
}

#define G_DEFINE_FLAGS_TYPE(TypeName, type_name, ...) \
GType \
type_name ## _get_type (void) { \
  static gsize g_define_type__static = 0; \
  if (g_once_init_enter (&g_define_type__static)) { \
    static const GFlagsValue flags_values[] = { \
      __VA_ARGS__ , \
      { 0, NULL, NULL }, \
    }; \
    GType g_define_type = g_flags_register_static (g_intern_static_string (#TypeName), flags_values); \
    g_once_init_leave (&g_define_type__static, g_define_type); \
  } \
  return g_define_type__static; \
}

#endif  /* G_DEFINE_ENUM_VALUE */

G_DEFINE_ENUM_TYPE(McbpmfApiInputMode, mcbpmf_api_input_mode,
  G_DEFINE_ENUM_VALUE(MCBPMF_API_KEY_SHIFT_MASK, "mc-bopomofo"),
  G_DEFINE_ENUM_VALUE(MCBPMF_API_KEY_CTRL_MASK, "plain-bopomofo"))

G_DEFINE_FLAGS_TYPE(McbpmfApiKeyModifiers, mcbpmf_api_key_modifiers,
  G_DEFINE_ENUM_VALUE(MCBPMF_API_KEY_SHIFT_MASK, "shift"),
  G_DEFINE_ENUM_VALUE(MCBPMF_API_KEY_CTRL_MASK, "ctrl"))

#define X(name, nick) G_DEFINE_ENUM_VALUE(MSTATE_TYPE_ ## name, #nick),
G_DEFINE_ENUM_TYPE(McbpmfApiInputStateType, mcbpmf_api_input_state_type,
  _MSTATE_TYPE_LIST
  G_DEFINE_ENUM_VALUE(MSTATE_TYPE_N, "type-last"))
#undef X

G_DEFINE_ENUM_TYPE(McbpmfApiCtrlEnterBehavior, mcbpmf_api_ctrl_enter_behavior,
  G_DEFINE_ENUM_VALUE(MCBPMF_API_CTRL_ENTER_NOOP, "noop"),
  G_DEFINE_ENUM_VALUE(MCBPMF_API_CTRL_ENTER_OUTPUT_BPMF_READINGS, "output-bpmf-readings"),
  G_DEFINE_ENUM_VALUE(MCBPMF_API_CTRL_ENTER_OUTPUT_HTML_RUBY_TEXT, "output-html-ruby-text"))
