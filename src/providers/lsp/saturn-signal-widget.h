/* saturn-signal-widget.h
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SATURN_TYPE_SIGNAL_WIDGET (saturn_signal_widget_get_type ())
G_DECLARE_DERIVABLE_TYPE (SaturnSignalWidget, saturn_signal_widget, SATURN, SIGNAL_WIDGET, GtkWidget)

struct _SaturnSignalWidgetClass
{
  GtkWidgetClass parent_class;
};

SaturnSignalWidget *
saturn_signal_widget_new (void);

GtkWidget *
saturn_signal_widget_get_child (SaturnSignalWidget *self);

void
saturn_signal_widget_set_child (SaturnSignalWidget *self,
                                GtkWidget          *child);

G_END_DECLS

/* End of saturn-signal-widget.h */
