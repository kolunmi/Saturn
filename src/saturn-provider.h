/* saturn-provider.h
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

#pragma once

#include <adwaita.h>

#include "saturn-threadsafe-list-store.h"

G_BEGIN_DECLS

#define SATURN_PROVIDER_QUARK (saturn_provider_quark ())
GQuark saturn_provider_quark (void);

#define SATURN_PROVIDER_SCORE_QUARK (saturn_provider_score_quark ())
GQuark saturn_provider_score_quark (void);

#define SATURN_PROVIDER_MAX_SCORE_DOUBLE 100000.0

#define SATURN_TYPE_PROVIDER (saturn_provider_get_type ())
G_DECLARE_INTERFACE (SaturnProvider, saturn_provider, SATURN, PROVIDER, GObject)

struct _SaturnProviderInterface
{
  GTypeInterface parent_iface;

  void (*init_global) (SaturnProvider *self);
  void (*deinit_global) (SaturnProvider *self);

  /* Provider must close sending side of channel when done */
  void (*query) (SaturnProvider            *self,
                 GObject                   *object,
                 SaturnThreadsafeListStore *store);
  gsize (*score) (SaturnProvider *self,
                  gpointer        item,
                  GObject        *query);

  /* `SELECT` SHOULD BE RAN INSIDE OF A FIBER */
  gboolean (*select) (SaturnProvider *self,
                      gpointer        item,
                      GObject        *query,
                      GError        **error);

  void (*setup_list_item) (SaturnProvider *self,
                           AdwBin         *list_item);
  void (*teardown_list_item) (SaturnProvider *self,
                              AdwBin         *list_item);
  void (*bind_list_item) (SaturnProvider *self,
                          gpointer        object,
                          AdwBin         *list_item);
  void (*unbind_list_item) (SaturnProvider *self,
                            gpointer        object,
                            AdwBin         *list_item);

  /* PREVIEW STUFF SHOULD BE RUN INSIDE OF FIBERS */
  void (*setup_preview) (SaturnProvider *self,
                         gpointer        object,
                         AdwBin         *preview);
  void (*teardown_preview) (SaturnProvider *self,
                            gpointer        object,
                            AdwBin         *preview);
  void (*bind_preview) (SaturnProvider *self,
                        gpointer        object,
                        AdwBin         *preview);
  void (*unbind_preview) (SaturnProvider *self,
                          gpointer        object,
                          AdwBin         *preview);
};

void
saturn_provider_init_global (SaturnProvider *self);

void
saturn_provider_deinit_global (SaturnProvider *self);

void
saturn_provider_query (SaturnProvider            *self,
                       GObject                   *object,
                       SaturnThreadsafeListStore *store);

gsize
saturn_provider_score (SaturnProvider *self,
                       gpointer        item,
                       GObject        *query);

gboolean
saturn_provider_select (SaturnProvider *self,
                        gpointer        item,
                        GObject        *query,
                        GError        **error);

void
saturn_provider_setup_list_item (SaturnProvider *self,
                                 gpointer        object,
                                 AdwBin         *list_item);
void
saturn_provider_teardown_list_item (SaturnProvider *self,
                                    gpointer        object,
                                    AdwBin         *list_item);
void
saturn_provider_bind_list_item (SaturnProvider *self,
                                gpointer        object,
                                AdwBin         *list_item);
void
saturn_provider_unbind_list_item (SaturnProvider *self,
                                  gpointer        object,
                                  AdwBin         *list_item);

void
saturn_provider_setup_preview (SaturnProvider *self,
                               gpointer        object,
                               AdwBin         *preview);
void
saturn_provider_teardown_preview (SaturnProvider *self,
                                  gpointer        object,
                                  AdwBin         *preview);
void
saturn_provider_bind_preview (SaturnProvider *self,
                              gpointer        object,
                              AdwBin         *preview);
void
saturn_provider_unbind_preview (SaturnProvider *self,
                                gpointer        object,
                                AdwBin         *preview);

G_END_DECLS
