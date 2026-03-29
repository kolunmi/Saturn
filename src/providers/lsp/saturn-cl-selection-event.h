/* saturn-cl-selection-event.h
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

#include <glib-object.h>

#include "saturn-provider.h"

G_BEGIN_DECLS

#define SATURN_TYPE_CL_SELECTION_EVENT (saturn_cl_selection_event_get_type ())
G_DECLARE_FINAL_TYPE (SaturnClSelectionEvent, saturn_cl_selection_event, SATURN, CL_SELECTION_EVENT, GObject)

SaturnClSelectionEvent *
saturn_cl_selection_event_new (void);

SaturnSelectKind
saturn_cl_selection_event_get_kind (SaturnClSelectionEvent *self);

const char *
saturn_cl_selection_event_get_selected_text (SaturnClSelectionEvent *self);

void
saturn_cl_selection_event_set_kind (SaturnClSelectionEvent *self,
                                    SaturnSelectKind        kind);

void
saturn_cl_selection_event_set_selected_text (SaturnClSelectionEvent *self,
                                             const char             *selected_text);

void
saturn_cl_selection_event_set_selected_text_take (SaturnClSelectionEvent *self,
                                                  char                   *selected_text);

#define saturn_cl_selection_event_set_selected_text_take_printf(self, ...) saturn_cl_selection_event_set_selected_text_take (self, g_strdup_printf (__VA_ARGS__))

G_END_DECLS

/* End of saturn-cl-selection-event.h */
