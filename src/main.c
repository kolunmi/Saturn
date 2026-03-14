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

extern void init_lib_SPLIT_SEQUENCE (cl_object);
extern void init_lib_CLOSER_MOP (cl_object);
extern void init_lib_TRIVIAL_GARBAGE (cl_object);
extern void init_lib_GLOBAL_VARS (cl_object);
extern void init_lib_BORDEAUX_THREADS (cl_object);
extern void init_lib_ITERATE (cl_object);
extern void init_lib_BABEL (cl_object);
extern void init_lib_ALEXANDRIA (cl_object);
extern void init_lib_TRIVIAL_FEATURES (cl_object);
extern void init_lib_CFFI (cl_object);
extern void init_lib_CL_CFFI_GTK4 (cl_object);
extern void init_lib_CL_CFFI_GTK4_INIT (cl_object);
extern void init_lib_CL_CFFI_CAIRO (cl_object);
extern void init_lib_CL_CFFI_GLIB (cl_object);
extern void init_lib_CL_CFFI_GLIB_INIT (cl_object);
extern void init_lib_CL_CFFI_PANGO (cl_object);
extern void init_lib_CL_CFFI_GRAPHENE (cl_object);
extern void init_lib_CL_CFFI_GDK_PIXBUF (cl_object);

int
main (int   argc,
      char *argv[])
{
  g_autoptr (SaturnApplication) app = NULL;
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

  cl_eval (ecl_read_from_cstring ("(require :asdf)"));
  ecl_init_module (NULL, init_lib_ALEXANDRIA);
  ecl_init_module (NULL, init_lib_SPLIT_SEQUENCE);
  ecl_init_module (NULL, init_lib_CLOSER_MOP);
  ecl_init_module (NULL, init_lib_TRIVIAL_GARBAGE);
  ecl_init_module (NULL, init_lib_GLOBAL_VARS);
  ecl_init_module (NULL, init_lib_BORDEAUX_THREADS);
  ecl_init_module (NULL, init_lib_ITERATE);
  ecl_init_module (NULL, init_lib_BABEL);
  ecl_init_module (NULL, init_lib_TRIVIAL_FEATURES);
  ecl_init_module (NULL, init_lib_CFFI);
  ecl_init_module (NULL, init_lib_CL_CFFI_GLIB_INIT);
  ecl_init_module (NULL, init_lib_CL_CFFI_GTK4_INIT);
  ecl_init_module (NULL, init_lib_CL_CFFI_GLIB);
  ecl_init_module (NULL, init_lib_CL_CFFI_CAIRO);
  ecl_init_module (NULL, init_lib_CL_CFFI_PANGO);
  ecl_init_module (NULL, init_lib_CL_CFFI_GRAPHENE);
  ecl_init_module (NULL, init_lib_CL_CFFI_GDK_PIXBUF);

  cl_eval (ecl_read_from_cstring ("(in-package \"CL-USER\")"));

  /* This is needed since apparently `init_lib_CL_CFFI_GTK4` references itself
     from inside */
  cl_eval (ecl_read_from_cstring ("(asdf:defsystem :cl-cffi-gtk4 :name \"cl-cffi-gtk4\" :version \"0.9.0\")"));
  ecl_init_module (NULL, init_lib_CL_CFFI_GTK4);
  cl_eval (ecl_read_from_cstring ("(in-package \"CL-USER\")"));

  app = saturn_application_new ("io.github.kolunmi.Saturn", G_APPLICATION_DEFAULT_FLAGS);
  ret = g_application_run (G_APPLICATION (app), argc, argv);

  return ret;
}
