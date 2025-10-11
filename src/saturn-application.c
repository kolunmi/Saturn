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

#include "providers/appinfo/provider.h"
#include "providers/fs/provider.h"

struct _SaturnApplication
{
  AdwApplication parent_instance;

  GListStore *providers;
  GPtrArray  *inits;
};

G_DEFINE_FINAL_TYPE (SaturnApplication, saturn_application, ADW_TYPE_APPLICATION)

SaturnApplication *
saturn_application_new (const char       *application_id,
                        GApplicationFlags flags)
{
  g_return_val_if_fail (application_id != NULL, NULL);

  return g_object_new (SATURN_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       "resource-base-path", "/io/github/kolunmi/Saturn",
                       NULL);
}

static void
saturn_application_activate (GApplication *app)
{
  SaturnApplication *self   = NULL;
  GtkWindow         *window = NULL;

  g_assert (SATURN_IS_APPLICATION (app));

  self   = SATURN_APPLICATION (app);
  window = gtk_application_get_active_window (GTK_APPLICATION (app));

  if (window == NULL)
    window = g_object_new (SATURN_TYPE_WINDOW,
                           "application", app,
                           "providers", self->providers,
                           NULL);

  gtk_window_present (window);
}

static void
saturn_application_class_init (SaturnApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  app_class->activate = saturn_application_activate;
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
                         "application-icon", "io.github.kolunmi.Saturn",
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
  g_autoptr (SaturnFileSystemProvider) fs   = NULL;
  g_autoptr (SaturnAppInfoProvider) appinfo = NULL;
  guint n_providers                         = 0;

  g_action_map_add_action_entries (
      G_ACTION_MAP (self),
      app_actions,
      G_N_ELEMENTS (app_actions),
      self);
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.quit",
      (const char *[]) { "<primary>q", NULL });

  fs      = g_object_new (SATURN_TYPE_FILE_SYSTEM_PROVIDER, NULL);
  appinfo = g_object_new (SATURN_TYPE_APP_INFO_PROVIDER, NULL);

  self->providers = g_list_store_new (SATURN_TYPE_PROVIDER);
  g_list_store_append (self->providers, fs);
  g_list_store_append (self->providers, appinfo);

  self->inits = g_ptr_array_new_with_free_func (dex_unref);
  n_providers = g_list_model_get_n_items (G_LIST_MODEL (self->providers));
  for (guint i = 0; i < n_providers; i++)
    {
      g_autoptr (SaturnProvider) provider = NULL;

      provider = g_list_model_get_item (G_LIST_MODEL (self->providers), i);
      g_ptr_array_add (self->inits, saturn_provider_init_global (provider));
    }
}
