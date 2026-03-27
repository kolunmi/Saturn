/* saturn-cl-completion-provider.h
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

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define SATURN_TYPE_CL_COMPLETION_PROVIDER (saturn_cl_completion_provider_get_type ())
G_DECLARE_FINAL_TYPE (SaturnClCompletionProvider, saturn_cl_completion_provider, SATURN, CL_COMPLETION_PROVIDER, GObject)

SaturnClCompletionProvider *
saturn_cl_completion_provider_new (void);

GListModel *
saturn_cl_completion_provider_get_model (SaturnClCompletionProvider *self);

int
saturn_cl_completion_provider_get_priority (SaturnClCompletionProvider *self);

const char *
saturn_cl_completion_provider_get_title (SaturnClCompletionProvider *self);

void
saturn_cl_completion_provider_set_model (SaturnClCompletionProvider *self,
                                         GListModel                 *model);

void
saturn_cl_completion_provider_set_priority (SaturnClCompletionProvider *self,
                                            int                         priority);

void
saturn_cl_completion_provider_set_title (SaturnClCompletionProvider *self,
                                         const char                 *title);

void
saturn_cl_completion_provider_set_title_take (SaturnClCompletionProvider *self,
                                              char                       *title);

#define saturn_cl_completion_provider_set_title_take_printf(self, ...) saturn_cl_completion_provider_set_title_take (self, g_strdup_printf (__VA_ARGS__))

G_END_DECLS

/* End of saturn-cl-completion-provider.h */
