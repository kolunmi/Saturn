/* saturn-threadsafe-list-store.h
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define SATURN_TYPE_THREADSAFE_LIST_STORE (saturn_threadsafe_list_store_get_type ())
G_DECLARE_FINAL_TYPE (SaturnThreadsafeListStore, saturn_threadsafe_list_store, SATURN, THREADSAFE_LIST_STORE, GObject)

SaturnThreadsafeListStore *
saturn_threadsafe_list_store_new (GCompareDataFunc sort_cmp,
                                  gpointer         sort_data,
                                  GDestroyNotify   sort_destroy_data);

void
saturn_threadsafe_list_store_insert_sorted (SaturnThreadsafeListStore *self,
                                            gpointer                   item);

void
saturn_threadsafe_list_store_clear_all (SaturnThreadsafeListStore *self);

G_END_DECLS

/* End of saturn-threadsafe-list-store.h */
