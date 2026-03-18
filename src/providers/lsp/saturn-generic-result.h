/* saturn-generic-result.h
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

G_BEGIN_DECLS

#define SATURN_TYPE_GENERIC_RESULT (saturn_generic_result_get_type ())
G_DECLARE_DERIVABLE_TYPE (SaturnGenericResult, saturn_generic_result, SATURN, GENERIC_RESULT, GObject)

struct _SaturnGenericResultClass
{
  GObjectClass parent_class;
};

SaturnGenericResult *
saturn_generic_result_new (void);

GObject *
saturn_generic_result_get_obj0 (SaturnGenericResult *self);

GObject *
saturn_generic_result_get_obj1 (SaturnGenericResult *self);

GObject *
saturn_generic_result_get_obj2 (SaturnGenericResult *self);

GObject *
saturn_generic_result_get_obj3 (SaturnGenericResult *self);

void
saturn_generic_result_set_obj0 (SaturnGenericResult *self,
                                GObject             *obj0);

void
saturn_generic_result_set_obj1 (SaturnGenericResult *self,
                                GObject             *obj1);

void
saturn_generic_result_set_obj2 (SaturnGenericResult *self,
                                GObject             *obj2);

void
saturn_generic_result_set_obj3 (SaturnGenericResult *self,
                                GObject             *obj3);

G_END_DECLS

/* End of saturn-generic-result.h */
