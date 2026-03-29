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

#include <ecl/ecl.h>

#include "saturn-application.h"

static void
init_ecl_thread (SaturnApplication *app);

static gboolean
init_ecl_done_idle_cb (SaturnApplication *app);

int
main (int   argc,
      char *argv[])
{
  g_autoptr (SaturnApplication) app = NULL;
  g_autoptr (GThread) init_ecl      = NULL;
  int ret                           = 0;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  ecl_set_option (ECL_OPT_TRAP_SIGFPE, 0);
  ecl_set_option (ECL_OPT_TRAP_SIGINT, 0);
  ecl_set_option (ECL_OPT_TRAP_SIGILL, 0);
  ecl_set_option (ECL_OPT_TRAP_SIGBUS, 0);
  ecl_set_option (ECL_OPT_TRAP_SIGPIPE, 0);
  ecl_set_option (ECL_OPT_TRAP_INTERRUPT_SIGNAL, 0);
  ecl_set_option (ECL_OPT_TRAP_SIGSEGV, 0);
  ecl_set_option (ECL_OPT_SIGNAL_HANDLING_THREAD, 0);
  g_assert (cl_boot (argc, argv) != 0);

  app      = saturn_application_new ("net.kolunmi.Saturn", G_APPLICATION_DEFAULT_FLAGS);
  init_ecl = g_thread_new ("Init ECL", (GThreadFunc) init_ecl_thread, g_object_ref (app));
  ret      = g_application_run (G_APPLICATION (app), argc, argv);

  cl_eval (ecl_read_from_cstring ("(mapcar #'bordeaux-threads:destroy-thread "
                                  "(remove (bordeaux-threads:current-thread) "
                                  "(bordeaux-threads:all-threads)))"));
  cl_shutdown ();
  return ret;
}

extern void init_lib_SATURN_CL_DEPS (cl_object);

static void
init_ecl_thread (SaturnApplication *app)
{
  ecl_import_current_thread (ECL_NIL, ECL_NIL);

  cl_eval (ecl_read_from_cstring ("(require :asdf)"));
  /* This is needed since apparently `init_lib_CL_CFFI_GTK4` references itself
     from inside */
  cl_eval (ecl_read_from_cstring ("(asdf:defsystem :cl-cffi-gtk4 :name \"cl-cffi-gtk4\" :version \"0.9.0\")"));
  ecl_init_module (NULL, init_lib_SATURN_CL_DEPS);
  cl_eval (ecl_read_from_cstring ("(in-package \"CL-USER\")"));

  ecl_release_current_thread ();

  g_idle_add_full (
      G_PRIORITY_DEFAULT,
      (GSourceFunc) init_ecl_done_idle_cb,
      app,
      g_object_unref);
}

static gboolean
init_ecl_done_idle_cb (SaturnApplication *app)
{
  g_object_set (app, "initializing", FALSE, NULL);
  return G_SOURCE_REMOVE;
}
