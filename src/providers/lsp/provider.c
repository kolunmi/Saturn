/* provider.c
 *
 * Copyright 2026 Eva M
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "SATURN::LSP-PROVIDER"

#include "config.h"
#include <glib/gi18n.h>

#include <ecl/ecl.h>

#include "provider.h"
#include "saturn-provider.h"

static GMutex global_mutex = { 0 };

struct _SaturnLspProvider
{
  GObject parent_instance;

  char *name;
  char *script_uri;

  gboolean loaded;
};

static void
provider_iface_init (SaturnProviderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SaturnLspProvider,
    saturn_lsp_provider,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (SATURN_TYPE_PROVIDER, provider_iface_init))

enum
{
  PROP_0,

  PROP_NAME,
  PROP_SCRIPT_URI,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
cl_object_to_gvalue (cl_object object,
                     GType     hint,
                     GValue   *value);

static void
ensure_lisp (SaturnLspProvider *self);

static void
ensure_gtk_types (void);

static void
saturn_lsp_provider_dispose (GObject *object)
{
  SaturnLspProvider *self = SATURN_LSP_PROVIDER (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->script_uri, g_free);

  G_OBJECT_CLASS (saturn_lsp_provider_parent_class)->dispose (object);
}

static void
saturn_lsp_provider_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  SaturnLspProvider *self = SATURN_LSP_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case PROP_SCRIPT_URI:
      g_value_set_string (value, self->script_uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_lsp_provider_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  SaturnLspProvider *self = SATURN_LSP_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_clear_pointer (&self->name, g_free);
      self->name = g_value_dup_string (value);
      break;
    case PROP_SCRIPT_URI:
      g_clear_pointer (&self->script_uri, g_free);
      self->script_uri = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_lsp_provider_constructed (GObject *object)
{
  SaturnLspProvider *self = SATURN_LSP_PROVIDER (object);

  g_assert (self->name != NULL &&
            self->script_uri != NULL);
}

static void
saturn_lsp_provider_class_init (SaturnLspProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = saturn_lsp_provider_constructed;
  object_class->set_property = saturn_lsp_provider_set_property;
  object_class->get_property = saturn_lsp_provider_get_property;
  object_class->dispose      = saturn_lsp_provider_dispose;

  props[PROP_NAME] =
      g_param_spec_string (
          "name",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_SCRIPT_URI] =
      g_param_spec_string (
          "script-uri",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
saturn_lsp_provider_init (SaturnLspProvider *self)
{
}

static cl_object
cl_g_object_unref (cl_object arg_object)
{
  GObject *object = NULL;

  object = ecl_to_pointer (arg_object);

  g_object_unref (object);
  return ECL_T;
}

static cl_object
cl_g_object_new (cl_object arg_class_name)
{
  const char *class_name = NULL;
  GType       type       = G_TYPE_INVALID;
  GObject    *object     = NULL;
  cl_object   pointer    = NULL;

  class_name = ecl_base_string_pointer_safe (si_coerce_to_base_string (arg_class_name));
  type       = g_type_from_name (class_name);
  if (!G_TYPE_IS_INSTANTIATABLE (type))
    return ECL_NIL;

  object = g_object_new (type, NULL);
  if (g_type_is_a (type, G_TYPE_INITIALLY_UNOWNED))
    g_object_ref_sink (object);

  pointer = ecl_make_pointer (object);
  ecl_set_finalizer_unprotected (
      pointer,
      ecl_read_from_cstring ("saturn:unsafe-g-object-unref"));

  return pointer;
}

static cl_object
cl_g_object_set (cl_object arg_object,
                 cl_object arg_prop,
                 cl_object arg_val)
{
  GObject    *object           = NULL;
  const char *prop             = NULL;
  g_autoptr (GTypeClass) class = NULL;
  GParamSpec *pspec            = NULL;
  GValue      value            = { 0 };

  object = ecl_to_pointer (arg_object);
  prop   = ecl_base_string_pointer_safe (si_coerce_to_base_string (arg_prop));

  class = g_type_class_ref (G_OBJECT_TYPE (object));
  pspec = g_object_class_find_property (G_OBJECT_CLASS (class), prop);
  if (pspec == NULL)
    {
      g_critical ("Property %s does not exist on type %s!",
                  prop, G_OBJECT_TYPE_NAME (object));
      return ECL_NIL;
    }

  cl_object_to_gvalue (arg_val, pspec->value_type, &value);
  g_object_set_property (object, prop, &value);
  g_value_unset (&value);

  return ECL_T;
}

static DexFuture *
provider_init_global (SaturnProvider *provider)
{
  SaturnLspProvider *self         = SATURN_LSP_PROVIDER (provider);
  g_autoptr (GMutexLocker) locker = NULL;
  g_autoptr (GBytes) bytes        = NULL;
  gconstpointer    data           = NULL;
  gsize            size           = 0;
  g_autofree char *wrapped        = NULL;

  g_mutex_locker_new (&global_mutex);
  cl_eval (ecl_read_from_cstring ("(defpackage :saturn "
                                  "  (:use :cl) "
                                  "  (:export #:unsafe-g-object-unref #:gobj-new #:gobj-set))"));
  cl_eval (ecl_read_from_cstring ("(in-package :saturn)"));

#define DEFUN(name, fun, args)                            \
  G_STMT_START                                            \
  {                                                       \
    char buf[128];                                        \
                                                          \
    ecl_def_c_function (c_string_to_object (name),        \
                        (cl_objectfn_fixed) (fun),        \
                        (args));                          \
    g_snprintf (buf, sizeof (buf), "(export '%s)", name); \
    cl_eval (ecl_read_from_cstring (buf));                \
  }                                                       \
  G_STMT_END

  DEFUN ("unsafe-g-object-unref", cl_g_object_unref, 1);
  DEFUN ("gobj-new", cl_g_object_new, 1);
  DEFUN ("gobj-set", cl_g_object_set, 3);

#undef DEFUN

  bytes = g_resources_lookup_data (
      "/io/github/kolunmi/Saturn/internal.lsp",
      G_RESOURCE_LOOKUP_FLAGS_NONE,
      NULL);
  data = g_bytes_get_data (bytes, &size);

  /* `g_resources_lookup_data` makes the data 0 terminated */
  wrapped = g_strdup_printf ("(progn %s)", (const char *) data);
  cl_eval (ecl_read_from_cstring (wrapped));

  cl_eval (ecl_read_from_cstring ("(in-package \"CL-USER\")"));

  ensure_lisp (self);
  return dex_future_new_true ();
}

static DexChannel *
provider_query (SaturnProvider *provider,
                GObject        *object)
{
  SaturnLspProvider *self        = SATURN_LSP_PROVIDER (provider);
  g_autoptr (DexChannel) channel = NULL;

  ensure_lisp (self);

  channel = dex_channel_new (32);
  if (self->loaded &&
      GTK_IS_STRING_OBJECT (object))
    {
      const char      *string = NULL;
      g_autofree char *fun    = NULL;

      string = gtk_string_object_get_string (GTK_STRING_OBJECT (object));
      fun    = g_strdup_printf ("%s:query", self->name);
      cl_eval (cl_list (
          2,
          ecl_read_from_cstring (fun),
          ecl_make_constant_base_string (string, -1)));
    }
  else
    dex_channel_close_send (channel);

  return g_steal_pointer (&channel);
}

static gsize
provider_score (SaturnProvider *self,
                gpointer        item,
                GObject        *query)
{
  return 0;
}

static gboolean
provider_select (SaturnProvider *self,
                 gpointer        item,
                 GObject        *query,
                 GError        **error)
{
  return FALSE;
}

static void
provider_bind_list_item (SaturnProvider *self,
                         gpointer        object,
                         AdwBin         *list_item)
{
}

static void
provider_bind_preview (SaturnProvider *self,
                       gpointer        object,
                       AdwBin         *preview)
{
}

static void
provider_iface_init (SaturnProviderInterface *iface)
{
  iface->init_global    = provider_init_global;
  iface->query          = provider_query;
  iface->score          = provider_score;
  iface->select         = provider_select;
  iface->bind_list_item = provider_bind_list_item;
  iface->bind_preview   = provider_bind_preview;
}

static void
cl_object_to_gvalue (cl_object object,
                     GType     hint,
                     GValue   *value)
{
  if (ECL_SYMBOLP (object) &&
      g_type_is_a (hint, G_TYPE_ENUM))
    {
      const char *symbol_name      = NULL;
      g_autoptr (GTypeClass) class = NULL;
      GEnumValue *enum_value       = NULL;

      symbol_name = ecl_base_string_pointer_safe (
          si_coerce_to_base_string (
              ecl_symbol_name (object)));

      class      = g_type_class_ref (hint);
      enum_value = g_enum_get_value_by_nick (G_ENUM_CLASS (class), symbol_name);

      if (enum_value != NULL)
        g_value_set_enum (
            g_value_init (value, hint),
            enum_value->value);
      else
        g_value_set_boolean (
            g_value_init (value, G_TYPE_BOOLEAN),
            ecl_to_bool (object));
    }
  else if (ECL_FOREIGN_DATA_P (object))
    g_value_set_object (
        g_value_init (value, G_TYPE_OBJECT),
        ecl_to_pointer (object));
  else if (ECL_STRINGP (object))
    g_value_set_string (
        g_value_init (value, G_TYPE_STRING),
        ecl_base_string_pointer_safe (si_coerce_to_base_string (object)));
  else if (ECL_SINGLE_FLOAT_P (object))
    g_value_set_double (
        g_value_init (value, G_TYPE_DOUBLE),
        ecl_to_float (object));
  else if (ECL_DOUBLE_FLOAT_P (object))
    g_value_set_double (
        g_value_init (value, G_TYPE_DOUBLE),
        ecl_to_double (object));
  else if (ECL_LONG_FLOAT_P (object))
    g_value_set_double (
        g_value_init (value, G_TYPE_DOUBLE),
        ecl_to_long_double (object));
  else if (ECL_FIXNUMP (object))
    {
      switch (hint)
        {
        case G_TYPE_INT:
          g_value_set_int (
              g_value_init (value, G_TYPE_INT),
              ecl_to_fix (object));
          break;
        case G_TYPE_INT64:
          g_value_set_int64 (
              g_value_init (value, G_TYPE_INT64),
              ecl_to_fix (object));
          break;
        case G_TYPE_UINT:
          g_value_set_uint (
              g_value_init (value, G_TYPE_UINT),
              ecl_to_fix (object));
          break;
        case G_TYPE_UINT64:
          g_value_set_uint64 (
              g_value_init (value, G_TYPE_UINT64),
              ecl_to_fix (object));
          break;
        default:
          g_value_set_boolean (
              g_value_init (value, G_TYPE_BOOLEAN),
              ecl_to_bool (object));
          break;
        }
    }
  else
    g_value_set_boolean (
        g_value_init (value, G_TYPE_BOOLEAN),
        ecl_to_bool (object));
}

// static void
// hold_cl_gobject (gpointer object)
// {
//   cl_eval (cl_list (2,
//                     ecl_read_from_cstring ("saturn:hold"),
//                     object));
// }

static void
ensure_lisp (SaturnLspProvider *self)
{
  g_autoptr (GError) local_error    = NULL;
  gboolean         result           = FALSE;
  g_autofree char *contents         = NULL;
  gsize            length           = 0;
  g_autofree char *contents_wrapped = NULL;
  g_autofree char *eval_before      = NULL;
  g_autofree char *eval_after       = NULL;

  if (self->loaded)
    return;

  /* We want providers to be able to reference widget names */
  ensure_gtk_types ();

  if (g_str_has_prefix (self->script_uri, "resource://"))
    {
      g_autoptr (GBytes) bytes = NULL;
      gsize size               = 0;

      bytes = g_resources_lookup_data (
          self->script_uri + strlen ("resource://"),
          G_RESOURCE_LOOKUP_FLAGS_NONE,
          &local_error);
      if (bytes == NULL)
        {
          g_critical ("Failed to load script at %s: %s",
                      self->script_uri, local_error->message);
          return;
        }
      contents = g_bytes_unref_to_data (
          g_steal_pointer (&bytes), &size);
    }
  else
    {
      result = g_file_get_contents (
          self->script_uri, &contents, &length, &local_error);
      if (!result)
        {
          g_critical ("Failed to load script at %s: %s",
                      self->script_uri, local_error->message);
          return;
        }
    }

  eval_before = g_strdup_printf ("(progn (defpackage :%s "
                                 "  (:use :cl :saturn) "
                                 "  (:export :query)) "
                                 "(in-package :%s))",
                                 self->name, self->name);
  cl_eval (ecl_read_from_cstring (eval_before));

  contents_wrapped = g_strdup_printf ("(progn %s)", contents);
  cl_eval (ecl_read_from_cstring (contents_wrapped));

  /* Return to CL-USER package */
  eval_after = g_strdup_printf ("(progn (in-package \"CL-USER\") (use-package :%s))",
                                self->name);
  cl_eval (ecl_read_from_cstring (eval_after));

  self->loaded = TRUE;
}

static void
ensure_gtk_types (void)
{
  g_type_ensure (gtk_at_context_get_type ());
  g_type_ensure (gtk_about_dialog_get_type ());
  g_type_ensure (gtk_accessible_get_type ());
  g_type_ensure (gtk_accessible_announcement_priority_get_type ());
  g_type_ensure (gtk_accessible_autocomplete_get_type ());
  g_type_ensure (gtk_accessible_invalid_state_get_type ());
  g_type_ensure (gtk_accessible_list_get_type ());
  g_type_ensure (gtk_accessible_platform_state_get_type ());
  g_type_ensure (gtk_accessible_property_get_type ());
  g_type_ensure (gtk_accessible_range_get_type ());
  g_type_ensure (gtk_accessible_relation_get_type ());
  g_type_ensure (gtk_accessible_role_get_type ());
  g_type_ensure (gtk_accessible_sort_get_type ());
  g_type_ensure (gtk_accessible_state_get_type ());
  g_type_ensure (gtk_accessible_text_get_type ());
  g_type_ensure (gtk_accessible_text_content_change_get_type ());
  g_type_ensure (gtk_accessible_text_granularity_get_type ());
  g_type_ensure (gtk_accessible_tristate_get_type ());
  g_type_ensure (gtk_action_bar_get_type ());
  g_type_ensure (gtk_actionable_get_type ());
  g_type_ensure (gtk_activate_action_get_type ());
  g_type_ensure (gtk_adjustment_get_type ());
  g_type_ensure (gtk_alert_dialog_get_type ());
  g_type_ensure (gtk_align_get_type ());
  g_type_ensure (gtk_alternative_trigger_get_type ());
  g_type_ensure (gtk_any_filter_get_type ());
  g_type_ensure (gtk_app_chooser_get_type ());
  g_type_ensure (gtk_app_chooser_button_get_type ());
  g_type_ensure (gtk_app_chooser_dialog_get_type ());
  g_type_ensure (gtk_app_chooser_widget_get_type ());
  g_type_ensure (gtk_application_get_type ());
  g_type_ensure (gtk_application_inhibit_flags_get_type ());
  g_type_ensure (gtk_application_window_get_type ());
  g_type_ensure (gtk_arrow_type_get_type ());
  g_type_ensure (gtk_aspect_frame_get_type ());
  g_type_ensure (gtk_assistant_get_type ());
  g_type_ensure (gtk_assistant_page_get_type ());
  g_type_ensure (gtk_assistant_page_type_get_type ());
  g_type_ensure (gtk_baseline_position_get_type ());
  g_type_ensure (gtk_bin_layout_get_type ());
  g_type_ensure (gtk_bitset_get_type ());
  g_type_ensure (gtk_bitset_iter_get_type ());
  g_type_ensure (gtk_bookmark_list_get_type ());
  g_type_ensure (gtk_bool_filter_get_type ());
  g_type_ensure (gtk_border_get_type ());
  g_type_ensure (gtk_border_style_get_type ());
  g_type_ensure (gtk_box_get_type ());
  g_type_ensure (gtk_box_layout_get_type ());
  g_type_ensure (gtk_buildable_get_type ());
  g_type_ensure (gtk_builder_get_type ());
  g_type_ensure (gtk_builder_cscope_get_type ());
  g_type_ensure (gtk_builder_closure_flags_get_type ());
  g_type_ensure (gtk_builder_error_get_type ());
  g_type_ensure (gtk_builder_list_item_factory_get_type ());
  g_type_ensure (gtk_builder_scope_get_type ());
  g_type_ensure (gtk_button_get_type ());
  g_type_ensure (gtk_buttons_type_get_type ());
  g_type_ensure (gtk_cclosure_expression_get_type ());
  g_type_ensure (gtk_calendar_get_type ());
  g_type_ensure (gtk_callback_action_get_type ());
  g_type_ensure (gtk_cell_area_get_type ());
  g_type_ensure (gtk_cell_area_box_get_type ());
  g_type_ensure (gtk_cell_area_context_get_type ());
  g_type_ensure (gtk_cell_editable_get_type ());
  g_type_ensure (gtk_cell_layout_get_type ());
  g_type_ensure (gtk_cell_renderer_get_type ());
  g_type_ensure (gtk_cell_renderer_accel_get_type ());
  g_type_ensure (gtk_cell_renderer_accel_mode_get_type ());
  g_type_ensure (gtk_cell_renderer_combo_get_type ());
  g_type_ensure (gtk_cell_renderer_mode_get_type ());
  g_type_ensure (gtk_cell_renderer_pixbuf_get_type ());
  g_type_ensure (gtk_cell_renderer_progress_get_type ());
  g_type_ensure (gtk_cell_renderer_spin_get_type ());
  g_type_ensure (gtk_cell_renderer_spinner_get_type ());
  g_type_ensure (gtk_cell_renderer_state_get_type ());
  g_type_ensure (gtk_cell_renderer_text_get_type ());
  g_type_ensure (gtk_cell_renderer_toggle_get_type ());
  g_type_ensure (gtk_cell_view_get_type ());
  g_type_ensure (gtk_center_box_get_type ());
  g_type_ensure (gtk_center_layout_get_type ());
  g_type_ensure (gtk_check_button_get_type ());
  g_type_ensure (gtk_closure_expression_get_type ());
  g_type_ensure (gtk_collation_get_type ());
  g_type_ensure (gtk_color_button_get_type ());
  g_type_ensure (gtk_color_chooser_get_type ());
  g_type_ensure (gtk_color_chooser_dialog_get_type ());
  g_type_ensure (gtk_color_chooser_widget_get_type ());
  g_type_ensure (gtk_color_dialog_get_type ());
  g_type_ensure (gtk_color_dialog_button_get_type ());
  g_type_ensure (gtk_column_view_get_type ());
  g_type_ensure (gtk_column_view_cell_get_type ());
  g_type_ensure (gtk_column_view_column_get_type ());
  g_type_ensure (gtk_column_view_row_get_type ());
  g_type_ensure (gtk_column_view_sorter_get_type ());
  g_type_ensure (gtk_combo_box_get_type ());
  g_type_ensure (gtk_combo_box_text_get_type ());
  g_type_ensure (gtk_constant_expression_get_type ());
  g_type_ensure (gtk_constraint_get_type ());
  g_type_ensure (gtk_constraint_attribute_get_type ());
  g_type_ensure (gtk_constraint_guide_get_type ());
  g_type_ensure (gtk_constraint_layout_get_type ());
  g_type_ensure (gtk_constraint_layout_child_get_type ());
  g_type_ensure (gtk_constraint_relation_get_type ());
  g_type_ensure (gtk_constraint_strength_get_type ());
  g_type_ensure (gtk_constraint_target_get_type ());
  g_type_ensure (gtk_constraint_vfl_parser_error_get_type ());
  g_type_ensure (gtk_content_fit_get_type ());
  g_type_ensure (gtk_corner_type_get_type ());
  g_type_ensure (gtk_css_provider_get_type ());
  g_type_ensure (gtk_css_section_get_type ());
  g_type_ensure (gtk_custom_filter_get_type ());
  g_type_ensure (gtk_custom_layout_get_type ());
  g_type_ensure (gtk_custom_sorter_get_type ());
  g_type_ensure (gtk_debug_flags_get_type ());
  g_type_ensure (gtk_delete_type_get_type ());
  g_type_ensure (gtk_dialog_get_type ());
  g_type_ensure (gtk_dialog_error_get_type ());
  g_type_ensure (gtk_dialog_flags_get_type ());
  g_type_ensure (gtk_direction_type_get_type ());
  g_type_ensure (gtk_directory_list_get_type ());
  g_type_ensure (gtk_drag_icon_get_type ());
  g_type_ensure (gtk_drag_source_get_type ());
  g_type_ensure (gtk_drawing_area_get_type ());
  g_type_ensure (gtk_drop_controller_motion_get_type ());
  g_type_ensure (gtk_drop_down_get_type ());
  g_type_ensure (gtk_drop_target_get_type ());
  g_type_ensure (gtk_drop_target_async_get_type ());
  g_type_ensure (gtk_editable_get_type ());
  g_type_ensure (gtk_editable_label_get_type ());
  g_type_ensure (gtk_editable_properties_get_type ());
  g_type_ensure (gtk_emoji_chooser_get_type ());
  g_type_ensure (gtk_entry_get_type ());
  g_type_ensure (gtk_entry_buffer_get_type ());
  g_type_ensure (gtk_entry_completion_get_type ());
  g_type_ensure (gtk_entry_icon_position_get_type ());
  g_type_ensure (gtk_event_controller_get_type ());
  g_type_ensure (gtk_event_controller_focus_get_type ());
  g_type_ensure (gtk_event_controller_key_get_type ());
  g_type_ensure (gtk_event_controller_legacy_get_type ());
  g_type_ensure (gtk_event_controller_motion_get_type ());
  g_type_ensure (gtk_event_controller_scroll_get_type ());
  g_type_ensure (gtk_event_controller_scroll_flags_get_type ());
  g_type_ensure (gtk_event_sequence_state_get_type ());
  g_type_ensure (gtk_every_filter_get_type ());
  g_type_ensure (gtk_expander_get_type ());
  g_type_ensure (gtk_expression_get_type ());
  g_type_ensure (gtk_expression_watch_get_type ());
  g_type_ensure (gtk_file_chooser_get_type ());
  g_type_ensure (gtk_file_chooser_action_get_type ());
  g_type_ensure (gtk_file_chooser_dialog_get_type ());
  g_type_ensure (gtk_file_chooser_error_get_type ());
  g_type_ensure (gtk_file_chooser_native_get_type ());
  g_type_ensure (gtk_file_chooser_widget_get_type ());
  g_type_ensure (gtk_file_dialog_get_type ());
  g_type_ensure (gtk_file_filter_get_type ());
  g_type_ensure (gtk_file_launcher_get_type ());
  g_type_ensure (gtk_filter_get_type ());
  g_type_ensure (gtk_filter_change_get_type ());
  g_type_ensure (gtk_filter_list_model_get_type ());
  g_type_ensure (gtk_filter_match_get_type ());
  g_type_ensure (gtk_fixed_get_type ());
  g_type_ensure (gtk_fixed_layout_get_type ());
  g_type_ensure (gtk_fixed_layout_child_get_type ());
  g_type_ensure (gtk_flatten_list_model_get_type ());
  g_type_ensure (gtk_flow_box_get_type ());
  g_type_ensure (gtk_flow_box_child_get_type ());
  g_type_ensure (gtk_font_button_get_type ());
  g_type_ensure (gtk_font_chooser_get_type ());
  g_type_ensure (gtk_font_chooser_dialog_get_type ());
  g_type_ensure (gtk_font_chooser_level_get_type ());
  g_type_ensure (gtk_font_chooser_widget_get_type ());
  g_type_ensure (gtk_font_dialog_get_type ());
  g_type_ensure (gtk_font_dialog_button_get_type ());
  g_type_ensure (gtk_font_level_get_type ());
  g_type_ensure (gtk_font_rendering_get_type ());
  g_type_ensure (gtk_frame_get_type ());
  g_type_ensure (gtk_gl_area_get_type ());
  g_type_ensure (gtk_gesture_get_type ());
  g_type_ensure (gtk_gesture_click_get_type ());
  g_type_ensure (gtk_gesture_drag_get_type ());
  g_type_ensure (gtk_gesture_long_press_get_type ());
  g_type_ensure (gtk_gesture_pan_get_type ());
  g_type_ensure (gtk_gesture_rotate_get_type ());
  g_type_ensure (gtk_gesture_single_get_type ());
  g_type_ensure (gtk_gesture_stylus_get_type ());
  g_type_ensure (gtk_gesture_swipe_get_type ());
  g_type_ensure (gtk_gesture_zoom_get_type ());
  g_type_ensure (gtk_graphics_offload_get_type ());
  g_type_ensure (gtk_graphics_offload_enabled_get_type ());
  g_type_ensure (gtk_grid_get_type ());
  g_type_ensure (gtk_grid_layout_get_type ());
  g_type_ensure (gtk_grid_layout_child_get_type ());
  g_type_ensure (gtk_grid_view_get_type ());
  g_type_ensure (gtk_header_bar_get_type ());
  g_type_ensure (gtk_im_context_get_type ());
  g_type_ensure (gtk_im_context_simple_get_type ());
  g_type_ensure (gtk_im_multicontext_get_type ());
  g_type_ensure (gtk_icon_lookup_flags_get_type ());
  g_type_ensure (gtk_icon_paintable_get_type ());
  g_type_ensure (gtk_icon_size_get_type ());
  g_type_ensure (gtk_icon_theme_get_type ());
  g_type_ensure (gtk_icon_theme_error_get_type ());
  g_type_ensure (gtk_icon_view_get_type ());
  g_type_ensure (gtk_icon_view_drop_position_get_type ());
  g_type_ensure (gtk_image_get_type ());
  g_type_ensure (gtk_image_type_get_type ());
  g_type_ensure (gtk_info_bar_get_type ());
  g_type_ensure (gtk_input_hints_get_type ());
  g_type_ensure (gtk_input_purpose_get_type ());
  g_type_ensure (gtk_inscription_get_type ());
  g_type_ensure (gtk_inscription_overflow_get_type ());
  g_type_ensure (gtk_interface_color_scheme_get_type ());
  g_type_ensure (gtk_interface_contrast_get_type ());
  g_type_ensure (gtk_justification_get_type ());
  g_type_ensure (gtk_keyval_trigger_get_type ());
  g_type_ensure (gtk_label_get_type ());
  g_type_ensure (gtk_layout_child_get_type ());
  g_type_ensure (gtk_layout_manager_get_type ());
  g_type_ensure (gtk_level_bar_get_type ());
  g_type_ensure (gtk_level_bar_mode_get_type ());
  g_type_ensure (gtk_license_get_type ());
  g_type_ensure (gtk_link_button_get_type ());
  g_type_ensure (gtk_list_base_get_type ());
  g_type_ensure (gtk_list_box_get_type ());
  g_type_ensure (gtk_list_box_row_get_type ());
  g_type_ensure (gtk_list_header_get_type ());
  g_type_ensure (gtk_list_item_get_type ());
  g_type_ensure (gtk_list_item_factory_get_type ());
  g_type_ensure (gtk_list_scroll_flags_get_type ());
  g_type_ensure (gtk_list_store_get_type ());
  g_type_ensure (gtk_list_tab_behavior_get_type ());
  g_type_ensure (gtk_list_view_get_type ());
  g_type_ensure (gtk_lock_button_get_type ());
  g_type_ensure (gtk_map_list_model_get_type ());
  g_type_ensure (gtk_media_controls_get_type ());
  g_type_ensure (gtk_media_file_get_type ());
  g_type_ensure (gtk_media_stream_get_type ());
  g_type_ensure (gtk_menu_button_get_type ());
  g_type_ensure (gtk_message_dialog_get_type ());
  g_type_ensure (gtk_message_type_get_type ());
  g_type_ensure (gtk_mnemonic_action_get_type ());
  g_type_ensure (gtk_mnemonic_trigger_get_type ());
  g_type_ensure (gtk_mount_operation_get_type ());
  g_type_ensure (gtk_movement_step_get_type ());
  g_type_ensure (gtk_multi_filter_get_type ());
  g_type_ensure (gtk_multi_selection_get_type ());
  g_type_ensure (gtk_multi_sorter_get_type ());
  g_type_ensure (gtk_named_action_get_type ());
  g_type_ensure (gtk_native_get_type ());
  g_type_ensure (gtk_native_dialog_get_type ());
  g_type_ensure (gtk_natural_wrap_mode_get_type ());
  g_type_ensure (gtk_never_trigger_get_type ());
  g_type_ensure (gtk_no_selection_get_type ());
  g_type_ensure (gtk_notebook_get_type ());
  g_type_ensure (gtk_notebook_page_get_type ());
  g_type_ensure (gtk_notebook_tab_get_type ());
  g_type_ensure (gtk_nothing_action_get_type ());
  g_type_ensure (gtk_number_up_layout_get_type ());
  g_type_ensure (gtk_numeric_sorter_get_type ());
  g_type_ensure (gtk_object_expression_get_type ());
  g_type_ensure (gtk_ordering_get_type ());
  g_type_ensure (gtk_orientable_get_type ());
  g_type_ensure (gtk_orientation_get_type ());
  g_type_ensure (gtk_overflow_get_type ());
  g_type_ensure (gtk_overlay_get_type ());
  g_type_ensure (gtk_overlay_layout_get_type ());
  g_type_ensure (gtk_overlay_layout_child_get_type ());
  g_type_ensure (gtk_pack_type_get_type ());
  g_type_ensure (gtk_pad_action_type_get_type ());
  g_type_ensure (gtk_pad_controller_get_type ());
  g_type_ensure (gtk_page_orientation_get_type ());
  g_type_ensure (gtk_page_set_get_type ());
  g_type_ensure (gtk_page_setup_get_type ());
  g_type_ensure (gtk_pan_direction_get_type ());
  g_type_ensure (gtk_paned_get_type ());
  g_type_ensure (gtk_paper_size_get_type ());
  g_type_ensure (gtk_param_expression_get_type ());
  g_type_ensure (gtk_password_entry_get_type ());
  g_type_ensure (gtk_password_entry_buffer_get_type ());
  g_type_ensure (gtk_pick_flags_get_type ());
  g_type_ensure (gtk_picture_get_type ());
  g_type_ensure (gtk_policy_type_get_type ());
  g_type_ensure (gtk_popover_get_type ());
  g_type_ensure (gtk_popover_menu_get_type ());
  g_type_ensure (gtk_popover_menu_bar_get_type ());
  g_type_ensure (gtk_popover_menu_flags_get_type ());
  g_type_ensure (gtk_position_type_get_type ());
  g_type_ensure (gtk_print_context_get_type ());
  g_type_ensure (gtk_print_dialog_get_type ());
  g_type_ensure (gtk_print_duplex_get_type ());
  g_type_ensure (gtk_print_error_get_type ());
  g_type_ensure (gtk_print_operation_get_type ());
  g_type_ensure (gtk_print_operation_action_get_type ());
  g_type_ensure (gtk_print_operation_preview_get_type ());
  g_type_ensure (gtk_print_operation_result_get_type ());
  g_type_ensure (gtk_print_pages_get_type ());
  g_type_ensure (gtk_print_quality_get_type ());
  g_type_ensure (gtk_print_settings_get_type ());
  g_type_ensure (gtk_print_setup_get_type ());
  g_type_ensure (gtk_print_status_get_type ());
  g_type_ensure (gtk_progress_bar_get_type ());
  g_type_ensure (gtk_propagation_limit_get_type ());
  g_type_ensure (gtk_propagation_phase_get_type ());
  g_type_ensure (gtk_property_expression_get_type ());
  g_type_ensure (gtk_range_get_type ());
  g_type_ensure (gtk_recent_info_get_type ());
  g_type_ensure (gtk_recent_manager_get_type ());
  g_type_ensure (gtk_recent_manager_error_get_type ());
  g_type_ensure (gtk_requisition_get_type ());
  g_type_ensure (gtk_response_type_get_type ());
  g_type_ensure (gtk_revealer_get_type ());
  g_type_ensure (gtk_revealer_transition_type_get_type ());
  g_type_ensure (gtk_root_get_type ());
  g_type_ensure (gtk_scale_get_type ());
  g_type_ensure (gtk_scale_button_get_type ());
  g_type_ensure (gtk_scroll_info_get_type ());
  g_type_ensure (gtk_scroll_step_get_type ());
  g_type_ensure (gtk_scroll_type_get_type ());
  g_type_ensure (gtk_scrollable_get_type ());
  g_type_ensure (gtk_scrollable_policy_get_type ());
  g_type_ensure (gtk_scrollbar_get_type ());
  g_type_ensure (gtk_scrolled_window_get_type ());
  g_type_ensure (gtk_search_bar_get_type ());
  g_type_ensure (gtk_search_entry_get_type ());
  g_type_ensure (gtk_section_model_get_type ());
  g_type_ensure (gtk_selection_filter_model_get_type ());
  g_type_ensure (gtk_selection_mode_get_type ());
  g_type_ensure (gtk_selection_model_get_type ());
  g_type_ensure (gtk_sensitivity_type_get_type ());
  g_type_ensure (gtk_separator_get_type ());
  g_type_ensure (gtk_settings_get_type ());
  g_type_ensure (gtk_shortcut_get_type ());
  g_type_ensure (gtk_shortcut_action_get_type ());
  g_type_ensure (gtk_shortcut_action_flags_get_type ());
  g_type_ensure (gtk_shortcut_controller_get_type ());
  g_type_ensure (gtk_shortcut_label_get_type ());
  g_type_ensure (gtk_shortcut_manager_get_type ());
  g_type_ensure (gtk_shortcut_scope_get_type ());
  g_type_ensure (gtk_shortcut_trigger_get_type ());
  g_type_ensure (gtk_shortcut_type_get_type ());
  g_type_ensure (gtk_shortcuts_group_get_type ());
  g_type_ensure (gtk_shortcuts_section_get_type ());
  g_type_ensure (gtk_shortcuts_shortcut_get_type ());
  g_type_ensure (gtk_shortcuts_window_get_type ());
  g_type_ensure (gtk_signal_action_get_type ());
  g_type_ensure (gtk_signal_list_item_factory_get_type ());
  g_type_ensure (gtk_single_selection_get_type ());
  g_type_ensure (gtk_size_group_get_type ());
  g_type_ensure (gtk_size_group_mode_get_type ());
  g_type_ensure (gtk_size_request_mode_get_type ());
  g_type_ensure (gtk_slice_list_model_get_type ());
  g_type_ensure (gtk_snapshot_get_type ());
  g_type_ensure (gtk_sort_list_model_get_type ());
  g_type_ensure (gtk_sort_type_get_type ());
  g_type_ensure (gtk_sorter_get_type ());
  g_type_ensure (gtk_sorter_change_get_type ());
  g_type_ensure (gtk_sorter_order_get_type ());
  g_type_ensure (gtk_spin_button_get_type ());
  g_type_ensure (gtk_spin_button_update_policy_get_type ());
  g_type_ensure (gtk_spin_type_get_type ());
  g_type_ensure (gtk_spinner_get_type ());
  g_type_ensure (gtk_stack_get_type ());
  g_type_ensure (gtk_stack_page_get_type ());
  g_type_ensure (gtk_stack_sidebar_get_type ());
  g_type_ensure (gtk_stack_switcher_get_type ());
  g_type_ensure (gtk_stack_transition_type_get_type ());
  g_type_ensure (gtk_state_flags_get_type ());
  g_type_ensure (gtk_statusbar_get_type ());
  g_type_ensure (gtk_string_filter_get_type ());
  g_type_ensure (gtk_string_filter_match_mode_get_type ());
  g_type_ensure (gtk_string_list_get_type ());
  g_type_ensure (gtk_string_object_get_type ());
  g_type_ensure (gtk_string_sorter_get_type ());
  g_type_ensure (gtk_style_context_get_type ());
  g_type_ensure (gtk_style_context_print_flags_get_type ());
  g_type_ensure (gtk_style_provider_get_type ());
  g_type_ensure (gtk_switch_get_type ());
  g_type_ensure (gtk_symbolic_color_get_type ());
  g_type_ensure (gtk_symbolic_paintable_get_type ());
  g_type_ensure (gtk_system_setting_get_type ());
  g_type_ensure (gtk_text_get_type ());
  g_type_ensure (gtk_text_buffer_get_type ());
  g_type_ensure (gtk_text_buffer_notify_flags_get_type ());
  g_type_ensure (gtk_text_child_anchor_get_type ());
  g_type_ensure (gtk_text_direction_get_type ());
  g_type_ensure (gtk_text_extend_selection_get_type ());
  g_type_ensure (gtk_text_iter_get_type ());
  g_type_ensure (gtk_text_mark_get_type ());
  g_type_ensure (gtk_text_search_flags_get_type ());
  g_type_ensure (gtk_text_tag_get_type ());
  g_type_ensure (gtk_text_tag_table_get_type ());
  g_type_ensure (gtk_text_view_get_type ());
  g_type_ensure (gtk_text_view_layer_get_type ());
  g_type_ensure (gtk_text_window_type_get_type ());
  g_type_ensure (gtk_toggle_button_get_type ());
  g_type_ensure (gtk_tooltip_get_type ());
  g_type_ensure (gtk_tree_expander_get_type ());
  g_type_ensure (gtk_tree_iter_get_type ());
  g_type_ensure (gtk_tree_list_model_get_type ());
  g_type_ensure (gtk_tree_list_row_get_type ());
  g_type_ensure (gtk_tree_list_row_sorter_get_type ());
  g_type_ensure (gtk_tree_model_get_type ());
  g_type_ensure (gtk_tree_model_filter_get_type ());
  g_type_ensure (gtk_tree_model_flags_get_type ());
  g_type_ensure (gtk_tree_model_sort_get_type ());
  g_type_ensure (gtk_tree_path_get_type ());
  g_type_ensure (gtk_tree_row_reference_get_type ());
  g_type_ensure (gtk_tree_selection_get_type ());
  g_type_ensure (gtk_tree_sortable_get_type ());
  g_type_ensure (gtk_tree_store_get_type ());
  g_type_ensure (gtk_tree_view_get_type ());
  g_type_ensure (gtk_tree_view_column_get_type ());
  g_type_ensure (gtk_tree_view_column_sizing_get_type ());
  g_type_ensure (gtk_tree_view_drop_position_get_type ());
  g_type_ensure (gtk_tree_view_grid_lines_get_type ());
  g_type_ensure (gtk_unit_get_type ());
  g_type_ensure (gtk_uri_launcher_get_type ());
  g_type_ensure (gtk_video_get_type ());
  g_type_ensure (gtk_viewport_get_type ());
  g_type_ensure (gtk_volume_button_get_type ());
  g_type_ensure (gtk_widget_get_type ());
  g_type_ensure (gtk_widget_paintable_get_type ());
  g_type_ensure (gtk_window_get_type ());
  g_type_ensure (gtk_window_controls_get_type ());
  g_type_ensure (gtk_window_gravity_get_type ());
  g_type_ensure (gtk_window_group_get_type ());
  g_type_ensure (gtk_window_handle_get_type ());
  g_type_ensure (gtk_wrap_mode_get_type ());
}
