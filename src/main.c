/* main.c
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

int
main (int   argc,
      char *argv[])
{
  const char *xdg_data_dirs_envvar  = NULL;
  g_autoptr (SaturnApplication) app = NULL;
  int ret                           = 0;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* Ensure the appinfo provider detects flatpak apps */
  xdg_data_dirs_envvar = g_getenv ("XDG_DATA_DIRS");
  if (xdg_data_dirs_envvar != NULL)
    {
      g_autoptr (GStrvBuilder) builder = NULL;
      g_auto (GStrv) xdg_data_dirs     = NULL;
      g_autofree char *tmp             = NULL;
      g_auto (GStrv) new_xdg_data_dirs = NULL;
      g_autofree char *joined          = NULL;

      builder       = g_strv_builder_new ();
      xdg_data_dirs = g_strsplit (xdg_data_dirs_envvar, ":", -1);
      g_strv_builder_addv (builder, (const char **) xdg_data_dirs);

      if (!g_strv_contains (
              (const gchar *const *) xdg_data_dirs,
              "/var/lib/flatpak/exports/share"))
        g_strv_builder_add (builder, "/var/lib/flatpak/exports/share");

      tmp = g_build_filename (
          g_get_home_dir (),
          ".local/share/flatpak/exports/share",
          NULL);
      if (!g_strv_contains (
              (const gchar *const *) xdg_data_dirs,
              tmp))
        g_strv_builder_add (builder, tmp);

      new_xdg_data_dirs = g_strv_builder_end (builder);
      joined            = g_strjoinv (":", new_xdg_data_dirs);
      g_setenv ("XDG_DATA_DIRS", joined, TRUE);
    }

  app = saturn_application_new ("io.github.kolunmi.Saturn", G_APPLICATION_DEFAULT_FLAGS);
  ret = g_application_run (G_APPLICATION (app), argc, argv);

  return ret;
}
