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
#include "saturn-generic-result.h"
#include "saturn-provider.h"
#include "saturn-signal-widget.h"
#include "saturn-threadsafe-list-store.h"

struct _SaturnLspProvider
{
  GObject parent_instance;

  char *name;
  char *script_uri;

  GType list_bind_type;

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

static cl_object
gobject_to_cl (gpointer object);
static gpointer
cl_to_gobject (cl_object object);

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

  g_type_ensure (SATURN_TYPE_GENERIC_RESULT);
  g_type_ensure (SATURN_TYPE_SIGNAL_WIDGET);
}

static void
saturn_lsp_provider_init (SaturnLspProvider *self)
{
  self->list_bind_type = G_TYPE_NONE;
}

static cl_object
cl_submit_result (cl_object cl_result,
                  cl_object cl_store,
                  cl_object cl_provider)
{
  GObject                   *result   = NULL;
  SaturnThreadsafeListStore *store    = NULL;
  SaturnLspProvider         *provider = NULL;

  result   = cl_to_gobject (cl_result);
  store    = cl_to_gobject (cl_store);
  provider = cl_to_gobject (cl_provider);

  g_object_set_qdata_full (
      G_OBJECT (result),
      SATURN_PROVIDER_QUARK,
      g_object_ref (provider),
      g_object_unref);

  return ecl_make_bool (saturn_threadsafe_list_store_append (store, result));
}

static void
provider_init_global (SaturnProvider *provider)
{
  SaturnLspProvider *self  = SATURN_LSP_PROVIDER (provider);
  g_autoptr (GBytes) bytes = NULL;
  gconstpointer    data    = NULL;
  gsize            size    = 0;
  g_autofree char *wrapped = NULL;

  cl_eval (ecl_read_from_cstring ("(defpackage :saturn "
                                  "  (:use :cl)) "));
  cl_eval (ecl_read_from_cstring ("(in-package :saturn)"));

#define DEFUN(name, fun, args)                     \
  G_STMT_START                                     \
  {                                                \
    g_autofree char *_tmp = NULL;                  \
                                                   \
    ecl_def_c_function (c_string_to_object (name), \
                        (cl_objectfn_fixed) (fun), \
                        (args));                   \
    _tmp = g_strdup_printf ("(export '%s)", name); \
    cl_eval (ecl_read_from_cstring (_tmp));        \
  }                                                \
  G_STMT_END

  DEFUN ("submit-result", cl_submit_result, 3);

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
}

static void
provider_deinit_global (SaturnProvider *provider)
{
  SaturnLspProvider *self     = SATURN_LSP_PROVIDER (provider);
  char               fun[256] = { 0 };

  g_snprintf (fun, sizeof (fun), "%s:deinit-global", self->name);
  cl_eval (cl_list (1, ecl_read_from_cstring (fun)));
}

static void
provider_query (SaturnProvider            *provider,
                GObject                   *object,
                SaturnThreadsafeListStore *store)
{
  SaturnLspProvider *self     = SATURN_LSP_PROVIDER (provider);
  char               fun[256] = { 0 };

  g_snprintf (fun, sizeof (fun), "%s:query", self->name);
  cl_eval (cl_list (
      4,
      ecl_read_from_cstring (fun),
      gobject_to_cl (provider),
      gobject_to_cl (object),
      gobject_to_cl (store)));
}

static gsize
provider_score (SaturnProvider *provider,
                gpointer        item,
                GObject        *query)
{
  SaturnLspProvider *self     = SATURN_LSP_PROVIDER (provider);
  char               fun[256] = { 0 };
  cl_object          result   = NULL;
  gsize              score    = 0;

  g_snprintf (fun, sizeof (fun), "%s:score", self->name);
  result = cl_eval (cl_list (
      4,
      ecl_read_from_cstring (fun),
      gobject_to_cl (provider),
      gobject_to_cl (item),
      gobject_to_cl (query)));

  score = ecl_to_ulong (result);
  g_object_set_qdata (
      G_OBJECT (item),
      SATURN_PROVIDER_SCORE_QUARK,
      GSIZE_TO_POINTER (score));

  return score;
}

static gboolean
provider_select (SaturnProvider *provider,
                 gpointer        item,
                 GObject        *query,
                 GError        **error)
{
  SaturnLspProvider *self     = SATURN_LSP_PROVIDER (provider);
  char               fun[256] = { 0 };
  cl_object          result   = NULL;
  gboolean           ret      = FALSE;

  g_snprintf (fun, sizeof (fun), "%s:select", self->name);
  result = cl_eval (cl_list (
      4,
      ecl_read_from_cstring (fun),
      gobject_to_cl (provider),
      gobject_to_cl (item),
      gobject_to_cl (query)));
  ret    = ecl_to_bool (result);

  return ret;
}

static void
provider_bind_list_item (SaturnProvider *provider,
                         gpointer        object,
                         AdwBin         *list_item)
{
  SaturnLspProvider *self     = SATURN_LSP_PROVIDER (provider);
  char               fun[256] = { 0 };
  cl_object          result   = NULL;

  /* Here to give providers the opportunity to avoid calling lisp in quick
     succession and slowing down the UI */
  if (self->list_bind_type == G_TYPE_NONE)
    {
      g_autofree char *list_bind_gtype = NULL;

      list_bind_gtype = g_strdup_printf ("(let ((gtype (ignore-errors %s:+list-bind-gtype+)))"
                                         "(if gtype gtype \"GTypeNone\"))",
                                         self->name);

      self->list_bind_type = g_type_from_name (
          ecl_base_string_pointer_safe (
              si_coerce_to_base_string (
                  cl_eval (ecl_read_from_cstring (list_bind_gtype)))));
      if (!g_type_is_a (self->list_bind_type, GTK_TYPE_WIDGET))
        self->list_bind_type = G_TYPE_INVALID;
    }

  if (self->list_bind_type > 0)
    {
      GtkWidget *widget = NULL;

      widget = g_object_new (self->list_bind_type, "item", object, NULL);
      adw_bin_set_child (list_item, widget);
      return;
    }

  g_snprintf (fun, sizeof (fun), "%s:bind-list-item", self->name);
  result = cl_eval (cl_list (
      3,
      ecl_read_from_cstring (fun),
      gobject_to_cl (provider),
      gobject_to_cl (object)));
  if (ecl_to_bool (result))
    adw_bin_set_child (
        list_item,
        GTK_WIDGET (cl_to_gobject (result)));
}

static void
provider_bind_preview (SaturnProvider *provider,
                       gpointer        object,
                       AdwBin         *preview)
{
  SaturnLspProvider *self     = SATURN_LSP_PROVIDER (provider);
  char               fun[256] = { 0 };
  cl_object          result   = NULL;

  g_snprintf (fun, sizeof (fun), "%s:bind-preview", self->name);
  result = cl_eval (cl_list (
      3,
      ecl_read_from_cstring (fun),
      gobject_to_cl (provider),
      gobject_to_cl (object)));
  if (ecl_to_bool (result))
    adw_bin_set_child (
        preview,
        GTK_WIDGET (cl_to_gobject (result)));
}

static void
provider_iface_init (SaturnProviderInterface *iface)
{
  iface->init_global    = provider_init_global;
  iface->deinit_global  = provider_deinit_global;
  iface->query          = provider_query;
  iface->score          = provider_score;
  iface->select         = provider_select;
  iface->bind_list_item = provider_bind_list_item;
  iface->bind_preview   = provider_bind_preview;
}

static cl_object
gobject_to_cl (gpointer object)
{
  return cl_eval (cl_list (
      2,
      ecl_read_from_cstring ("saturn:make-object-for-lisp"),
      ecl_make_pointer (g_object_ref (object))));
}

static gpointer
cl_to_gobject (cl_object object)
{
  return ecl_to_pointer (cl_eval (cl_list (
      2,
      ecl_read_from_cstring ("saturn:make-object-for-c"),
      object)));
}

static void
ensure_lisp (SaturnLspProvider *self)
{
  g_autoptr (GError) local_error    = NULL;
  gboolean         result           = FALSE;
  g_autofree char *contents         = NULL;
  gsize            length           = 0;
  g_autofree char *contents_wrapped = NULL;
  g_autofree char *eval_before      = NULL;

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
                                 "  (:use :cl) "
                                 "  (:export :+list-bind-gtype+ :deinit-global :query :score :select :bind-list-item :bind-preview)) "
                                 "(in-package :%s))",
                                 self->name, self->name);
  cl_eval (ecl_read_from_cstring (eval_before));

  contents_wrapped = g_strdup_printf ("(progn %s)", contents);
  cl_eval (ecl_read_from_cstring (contents_wrapped));
  cl_eval (ecl_read_from_cstring ("(in-package \"CL-USER\")"));

  self->loaded = TRUE;
}
