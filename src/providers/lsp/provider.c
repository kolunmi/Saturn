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
  SaturnLspProvider *self         = SATURN_LSP_PROVIDER (object);
  g_autoptr (GMutexLocker) locker = NULL;
  g_autoptr (GError) local_error  = NULL;
  gboolean         result         = FALSE;
  g_autofree char *contents       = NULL;
  gsize            length         = 0;

  if (self->name == NULL ||
      self->script_uri == NULL)
    return;

  locker = g_mutex_locker_new (&global_mutex);

  result = g_file_get_contents (
      self->script_uri, &contents, &length, &local_error);
  if (!result)
    {
      g_critical ("Failed to load script at %s: %s",
                  self->script_uri, local_error->message);
      return;
    }

  cl_eval (ecl_read_from_cstring (contents));
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

static DexFuture *
provider_init_global (SaturnProvider *provider)
{
  return dex_future_new_true ();
}

static DexChannel *
provider_query (SaturnProvider *provider,
                GObject        *object)
{
  SaturnLspProvider *self        = SATURN_LSP_PROVIDER (provider);
  g_autoptr (DexChannel) channel = NULL;

  channel = dex_channel_new (32);
  if (GTK_IS_STRING_OBJECT (object))
    {
      const char *string          = NULL;
      g_autoptr (GString) escaped = NULL;
      g_autofree char *form       = NULL;

      string  = gtk_string_object_get_string (GTK_STRING_OBJECT (object));
      escaped = g_string_new (string);
      g_string_replace (escaped, "\"", "\\\"", 0);

      form = g_strdup_printf ("(%s-query \"%s\")", self->name, escaped->str);
      cl_eval (ecl_read_from_cstring (form));
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
