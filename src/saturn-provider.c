/* saturn-provider.c
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

#include "saturn-provider.h"

/* clang-format off */
G_DEFINE_QUARK (saturn-provider-quark, saturn_provider);
G_DEFINE_QUARK (saturn-provider-score-quark, saturn_provider_score);
/* clang-format on */

G_DEFINE_INTERFACE (SaturnProvider, saturn_provider, G_TYPE_OBJECT)

static DexFuture *
saturn_provider_real_init_global (SaturnProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
saturn_provider_real_deinit_global (SaturnProvider *self)
{
  return dex_future_new_true ();
}

static DexChannel *
saturn_provider_real_query (SaturnProvider *self,
                            GObject        *object)
{
  g_autoptr (DexChannel) channel = NULL;

  channel = dex_channel_new (0);
  dex_channel_close_send (channel);
  return g_steal_pointer (&channel);
}

static gsize
saturn_provider_real_score (SaturnProvider *self,
                            gpointer        item,
                            GObject        *query)
{
  return 0;
}

static void
saturn_provider_real_setup_list_item (SaturnProvider *self,
                                      AdwBin         *list_item)
{
}

static void
saturn_provider_real_teardown_list_item (SaturnProvider *self,
                                         AdwBin         *list_item)
{
}

static void
saturn_provider_real_bind_list_item (SaturnProvider *self,
                                     gpointer        object,
                                     AdwBin         *list_item)
{
}

static void
saturn_provider_real_unbind_list_item (SaturnProvider *self,
                                       gpointer        object,
                                       AdwBin         *list_item)
{
}

static void
saturn_provider_real_setup_preview (SaturnProvider *self,
                                    gpointer        object,
                                    AdwBin         *preview)
{
}

static void
saturn_provider_real_teardown_preview (SaturnProvider *self,
                                       gpointer        object,
                                       AdwBin         *preview)
{
}

static void
saturn_provider_real_bind_preview (SaturnProvider *self,
                                   gpointer        object,
                                   AdwBin         *preview)
{
}

static void
saturn_provider_real_unbind_preview (SaturnProvider *self,
                                     gpointer        object,
                                     AdwBin         *preview)
{
}

static void
saturn_provider_default_init (SaturnProviderInterface *iface)
{
  iface->init_global        = saturn_provider_real_init_global;
  iface->deinit_global      = saturn_provider_real_deinit_global;
  iface->query              = saturn_provider_real_query;
  iface->score              = saturn_provider_real_score;
  iface->setup_list_item    = saturn_provider_real_setup_list_item;
  iface->teardown_list_item = saturn_provider_real_teardown_list_item;
  iface->bind_list_item     = saturn_provider_real_bind_list_item;
  iface->unbind_list_item   = saturn_provider_real_unbind_list_item;
  iface->setup_preview      = saturn_provider_real_setup_preview;
  iface->teardown_preview   = saturn_provider_real_teardown_preview;
  iface->bind_preview       = saturn_provider_real_bind_preview;
  iface->unbind_preview     = saturn_provider_real_unbind_preview;
}

DexFuture *
saturn_provider_init_global (SaturnProvider *self)
{
  dex_return_error_if_fail (SATURN_IS_PROVIDER (self));

  return SATURN_PROVIDER_GET_IFACE (self)->init_global (self);
}

DexFuture *
saturn_provider_deinit_global (SaturnProvider *self)
{
  dex_return_error_if_fail (SATURN_IS_PROVIDER (self));

  return SATURN_PROVIDER_GET_IFACE (self)->deinit_global (self);
}

DexChannel *
saturn_provider_query (SaturnProvider *self,
                       GObject        *object)
{
  g_return_val_if_fail (SATURN_IS_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  return SATURN_PROVIDER_GET_IFACE (self)->query (self, object);
}

gsize
saturn_provider_score (SaturnProvider *self,
                       gpointer        item,
                       GObject        *query)
{
  g_return_val_if_fail (SATURN_IS_PROVIDER (self), 0);
  g_return_val_if_fail (G_IS_OBJECT (item), 0);
  g_return_val_if_fail (G_IS_OBJECT (query), 0);

  return SATURN_PROVIDER_GET_IFACE (self)->score (self,
                                                  item,
                                                  query);
}

void
saturn_provider_setup_list_item (SaturnProvider *self,
                                 gpointer        object,
                                 AdwBin         *list_item)
{
  g_return_if_fail (SATURN_IS_PROVIDER (self));
  g_return_if_fail (ADW_IS_BIN (list_item));

  SATURN_PROVIDER_GET_IFACE (self)->setup_list_item (self,
                                                     list_item);
}

void
saturn_provider_teardown_list_item (SaturnProvider *self,
                                    gpointer        object,
                                    AdwBin         *list_item)
{
  g_return_if_fail (SATURN_IS_PROVIDER (self));
  g_return_if_fail (ADW_IS_BIN (list_item));

  SATURN_PROVIDER_GET_IFACE (self)->teardown_list_item (self,
                                                        list_item);
}

void
saturn_provider_bind_list_item (SaturnProvider *self,
                                gpointer        object,
                                AdwBin         *list_item)
{
  g_return_if_fail (SATURN_IS_PROVIDER (self));
  g_return_if_fail (ADW_IS_BIN (list_item));

  SATURN_PROVIDER_GET_IFACE (self)->bind_list_item (self,
                                                    object,
                                                    list_item);
}

void
saturn_provider_unbind_list_item (SaturnProvider *self,
                                  gpointer        object,
                                  AdwBin         *list_item)
{
  g_return_if_fail (SATURN_IS_PROVIDER (self));
  g_return_if_fail (ADW_IS_BIN (list_item));

  SATURN_PROVIDER_GET_IFACE (self)->unbind_list_item (self,
                                                      object,
                                                      list_item);
}

void
saturn_provider_setup_preview (SaturnProvider *self,
                               gpointer        object,
                               AdwBin         *preview)
{
  g_return_if_fail (SATURN_IS_PROVIDER (self));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (ADW_IS_BIN (object));

  SATURN_PROVIDER_GET_IFACE (self)->setup_preview (self,
                                                   object,
                                                   preview);
}

void
saturn_provider_teardown_preview (SaturnProvider *self,
                                  gpointer        object,
                                  AdwBin         *preview)
{
  g_return_if_fail (SATURN_IS_PROVIDER (self));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (ADW_IS_BIN (object));

  SATURN_PROVIDER_GET_IFACE (self)->teardown_preview (self,
                                                      object,
                                                      preview);
}

void
saturn_provider_bind_preview (SaturnProvider *self,
                              gpointer        object,
                              AdwBin         *preview)
{
  g_return_if_fail (SATURN_IS_PROVIDER (self));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (ADW_IS_BIN (object));

  SATURN_PROVIDER_GET_IFACE (self)->bind_preview (self,
                                                  object,
                                                  preview);
}

void
saturn_provider_unbind_preview (SaturnProvider *self,
                                gpointer        object,
                                AdwBin         *preview)
{
  g_return_if_fail (SATURN_IS_PROVIDER (self));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (ADW_IS_BIN (object));

  SATURN_PROVIDER_GET_IFACE (self)->unbind_preview (self,
                                                    object,
                                                    preview);
}
