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

#include "provider.h"
#include "saturn-provider.h"

struct _SaturnFileSystemProvider
{
  GObject parent_instance;
};

static void
provider_iface_init (SaturnProviderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SaturnFileSystemProvider,
    saturn_file_system_provider,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (SATURN_TYPE_PROVIDER, provider_iface_init))

static void
saturn_file_system_provider_dispose (GObject *object)
{
  SaturnFileSystemProvider *self = SATURN_FILE_SYSTEM_PROVIDER (object);

  G_OBJECT_CLASS (saturn_file_system_provider_parent_class)->dispose (object);
}

static void
saturn_file_system_provider_class_init (SaturnFileSystemProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = saturn_file_system_provider_dispose;
}

static void
saturn_file_system_provider_init (SaturnFileSystemProvider *self)
{
}

static DexChannel *
provider_query (SaturnProvider *self,
                GObject        *object)
{
  g_autoptr (DexChannel) channel = NULL;

  channel = dex_channel_new (0);
  dex_channel_close_send (channel);
  return g_steal_pointer (&channel);
}

static void
provider_iface_init (SaturnProviderInterface *iface)
{
  iface->query = provider_query;
}
