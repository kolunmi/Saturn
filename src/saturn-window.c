/* saturn-window.c
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

#include "saturn-window.h"

struct _SaturnWindow
{
  AdwApplicationWindow parent_instance;

  /* Template widgets */
  GtkLabel *label;
};

G_DEFINE_FINAL_TYPE (SaturnWindow, saturn_window, ADW_TYPE_APPLICATION_WINDOW)

static void
saturn_window_class_init (SaturnWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Saturn/saturn-window.ui");
  gtk_widget_class_bind_template_child (widget_class, SaturnWindow, label);
}

static void
saturn_window_init (SaturnWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
