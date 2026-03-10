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
      contents = g_strdup (g_bytes_get_data (bytes, &size));
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
