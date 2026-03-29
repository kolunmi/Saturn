/* saturn-application.c
 *
 * Copyright 2025 Eva
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

#include "config.h"
#include <glib/gi18n.h>

#include "saturn-application.h"
#include "saturn-provider.h"
#include "saturn-window.h"

#include "providers/lsp/provider.h"

struct _SaturnApplication
{
  AdwApplication parent_instance;

  gboolean initializing;
  char    *selected_text;

  GListStore *providers;
};

G_DEFINE_FINAL_TYPE (SaturnApplication, saturn_application, ADW_TYPE_APPLICATION)

enum
{
  PROP_0,

  PROP_INITIALIZING,
  PROP_SELECTED_TEXT,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
ensure_providers (SaturnApplication *self);

static void
saturn_application_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  SaturnApplication *self = SATURN_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_INITIALIZING:
      g_value_set_boolean (value, self->initializing);
      break;
    case PROP_SELECTED_TEXT:
      g_value_set_string (value, self->selected_text);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_application_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  SaturnApplication *self = SATURN_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_INITIALIZING:
      self->initializing = g_value_get_boolean (value);
      if (!self->initializing)
        ensure_providers (self);
      break;
    case PROP_SELECTED_TEXT:
      g_clear_pointer (&self->selected_text, g_free);
      self->selected_text = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

SaturnApplication *
saturn_application_new (const char       *application_id,
                        GApplicationFlags flags)
{
  g_return_val_if_fail (application_id != NULL, NULL);

  return g_object_new (SATURN_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       "resource-base-path", "/net/kolunmi/Saturn",
                       "initializing", TRUE,
                       NULL);
}

static void
saturn_application_activate (GApplication *app)
{
  SaturnApplication *self   = NULL;
  GtkWindow         *window = NULL;

  g_assert (SATURN_IS_APPLICATION (app));

  self = SATURN_APPLICATION (app);

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  if (window == NULL)
    {
      window = g_object_new (SATURN_TYPE_WINDOW,
                             "application", app,
                             "providers", self->providers,
                             NULL);
      g_object_bind_property (
          self, "initializing",
          window, "initializing",
          G_BINDING_SYNC_CREATE);
    }

  gtk_window_present (window);
}

static void
saturn_application_shutdown (GApplication *app)
{
  SaturnApplication *self        = NULL;
  guint              n_providers = 0;

  g_assert (SATURN_IS_APPLICATION (app));

  self = SATURN_APPLICATION (app);

  n_providers = g_list_model_get_n_items (G_LIST_MODEL (self->providers));
  for (guint i = 0; i < n_providers; i++)
    {
      g_autoptr (SaturnProvider) provider = NULL;

      provider = g_list_model_get_item (G_LIST_MODEL (self->providers), i);
      saturn_provider_deinit_global (provider, self->selected_text);
    }

  G_APPLICATION_CLASS (saturn_application_parent_class)->shutdown (app);
}

static void
saturn_application_class_init (SaturnApplicationClass *klass)
{
  GObjectClass      *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class    = G_APPLICATION_CLASS (klass);

  object_class->set_property = saturn_application_set_property;
  object_class->get_property = saturn_application_get_property;

  props[PROP_INITIALIZING] =
      g_param_spec_boolean (
          "initializing",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_SELECTED_TEXT] =
      g_param_spec_string (
          "selected-text",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  app_class->activate = saturn_application_activate;
  app_class->shutdown = saturn_application_shutdown;
}

static void
saturn_application_about_action (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  static const char *developers[] = { "Eva", NULL };
  SaturnApplication *self         = user_data;
  GtkWindow         *window       = NULL;

  g_assert (SATURN_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  adw_show_about_dialog (GTK_WIDGET (window),
                         "application-name", "saturn",
                         "application-icon", "net.kolunmi.Saturn",
                         "developer-name", "Eva",
                         "translator-credits", _ ("translator-credits"),
                         "version", "0.1.0",
                         "developers", developers,
                         "copyright", "© 2025 Eva",
                         NULL);
}

static void
saturn_application_quit_action (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  SaturnApplication *self = user_data;

  g_assert (SATURN_IS_APPLICATION (self));

  g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
  {  "quit",  saturn_application_quit_action },
  { "about", saturn_application_about_action },
};

static void
saturn_application_init (SaturnApplication *self)
{
  self->providers = g_list_store_new (SATURN_TYPE_PROVIDER);

  g_action_map_add_action_entries (
      G_ACTION_MAP (self),
      app_actions,
      G_N_ELEMENTS (app_actions),
      self);
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.quit",
      (const char *[]) { "<primary>q", NULL });
}

static void
ensure_providers (SaturnApplication *self)
{
  guint n_providers = 0;

  n_providers = g_list_model_get_n_items (G_LIST_MODEL (self->providers));
  if (n_providers > 0)
    return;

#define APPEND_PROVIDER(...)                     \
  G_STMT_START                                   \
  {                                              \
    g_autoptr (GObject) _obj = NULL;             \
                                                 \
    _obj = g_object_new (__VA_ARGS__, NULL);     \
    g_list_store_append (self->providers, _obj); \
  }                                              \
  G_STMT_END

  APPEND_PROVIDER (SATURN_TYPE_LSP_PROVIDER,
                   "name", "history",
                   "script-uri", "resource:///net/kolunmi/Saturn/history.lsp");
  APPEND_PROVIDER (SATURN_TYPE_LSP_PROVIDER,
                   "name", "eval",
                   "script-uri", "resource:///net/kolunmi/Saturn/eval.lsp");
  APPEND_PROVIDER (SATURN_TYPE_LSP_PROVIDER,
                   "name", "calc",
                   "script-uri", "resource:///net/kolunmi/Saturn/calc.lsp");
  APPEND_PROVIDER (SATURN_TYPE_LSP_PROVIDER,
                   "name", "emoji",
                   "script-uri", "resource:///net/kolunmi/Saturn/emoji.lsp");
  APPEND_PROVIDER (SATURN_TYPE_LSP_PROVIDER,
                   "name", "appinfo",
                   "script-uri", "resource:///net/kolunmi/Saturn/appinfo.lsp");
  APPEND_PROVIDER (SATURN_TYPE_LSP_PROVIDER,
                   "name", "fs",
                   "script-uri", "resource:///net/kolunmi/Saturn/fs.lsp");
  APPEND_PROVIDER (SATURN_TYPE_LSP_PROVIDER,
                   "name", "color",
                   "script-uri", "resource:///net/kolunmi/Saturn/color.lsp");
  APPEND_PROVIDER (SATURN_TYPE_LSP_PROVIDER,
                   "name", "grep",
                   "script-uri", "resource:///net/kolunmi/Saturn/grep.lsp");
  APPEND_PROVIDER (SATURN_TYPE_LSP_PROVIDER,
                   "name", "enchant",
                   "script-uri", "resource:///net/kolunmi/Saturn/enchant.lsp");
  APPEND_PROVIDER (SATURN_TYPE_LSP_PROVIDER,
                   "name", "brew",
                   "script-uri", "resource:///net/kolunmi/Saturn/brew.lsp");

#undef APPEND_PROVIDER

  n_providers = g_list_model_get_n_items (G_LIST_MODEL (self->providers));
  for (guint i = 0; i < n_providers; i++)
    {
      g_autoptr (SaturnProvider) provider = NULL;

      provider = g_list_model_get_item (G_LIST_MODEL (self->providers), i);
      saturn_provider_init_global (provider);
    }
}
