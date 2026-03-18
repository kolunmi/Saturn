/* saturn-threadsafe-list-store.c
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

#include "saturn-threadsafe-list-store.h"
#include "util.h"

struct _SaturnThreadsafeListStore
{
  GObject parent_instance;

  GCompareDataFunc sort_func;
  gpointer         user_data;
  GDestroyNotify   destroy_user_data;

  GMutex buildup_mutex;

  GListStore *store;
  gboolean    cancelled;

  GPtrArray *buildup;
  guint      update_timeout;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SaturnThreadsafeListStore,
    saturn_threadsafe_list_store,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init));

enum
{
  PROP_0,

  PROP_N_ITEMS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

SATURN_DEFINE_DATA (
    idle_modify,
    IdleModify,
    {
      GWeakRef *self;
      GObject  *item;
    },
    SATURN_RELEASE_DATA (self, saturn_weak_release);
    SATURN_RELEASE_DATA (item, g_object_unref);)

static void
store_changed (SaturnThreadsafeListStore *self,
               guint                      position,
               guint                      removed,
               guint                      added,
               GListModel                *model);

static gboolean
idle_update_cb (GWeakRef *wr);

static void
saturn_threadsafe_list_store_dispose (GObject *object)
{
  SaturnThreadsafeListStore *self = SATURN_THREADSAFE_LIST_STORE (object);

  g_mutex_clear (&self->buildup_mutex);
  g_clear_pointer (&self->buildup, g_ptr_array_unref);
  g_clear_object (&self->store);
  g_clear_handle_id (&self->update_timeout, g_source_remove);

  if (self->user_data != NULL &&
      self->destroy_user_data != NULL)
    self->destroy_user_data (self->user_data);

  G_OBJECT_CLASS (saturn_threadsafe_list_store_parent_class)->dispose (object);
}

static void
saturn_threadsafe_list_store_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  SaturnThreadsafeListStore *self = SATURN_THREADSAFE_LIST_STORE (object);

  switch (prop_id)
    {
    case PROP_N_ITEMS:
      g_value_set_uint (value, g_list_model_get_n_items (G_LIST_MODEL (self)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_threadsafe_list_store_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  SaturnThreadsafeListStore *self = SATURN_THREADSAFE_LIST_STORE (object);

  (void) self;

  switch (prop_id)
    {
    case PROP_N_ITEMS:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_threadsafe_list_store_class_init (SaturnThreadsafeListStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = saturn_threadsafe_list_store_set_property;
  object_class->get_property = saturn_threadsafe_list_store_get_property;
  object_class->dispose      = saturn_threadsafe_list_store_dispose;

  props[PROP_N_ITEMS] =
      g_param_spec_uint (
          "n-items",
          NULL, NULL,
          0, G_MAXUINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
saturn_threadsafe_list_store_init (SaturnThreadsafeListStore *self)
{
  g_mutex_init (&self->buildup_mutex);
  self->buildup = g_ptr_array_new_with_free_func (g_object_unref);

  self->store = g_list_store_new (G_TYPE_OBJECT);
  g_signal_connect_swapped (
      self->store,
      "items-changed",
      G_CALLBACK (store_changed),
      self);

  self->update_timeout = g_timeout_add_full (
      G_PRIORITY_DEFAULT_IDLE,
      10,
      (GSourceFunc) idle_update_cb,
      saturn_track_weak (self),
      saturn_weak_release);
}

static GType
list_model_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
list_model_get_n_items (GListModel *list)
{
  SaturnThreadsafeListStore *self = SATURN_THREADSAFE_LIST_STORE (list);
  g_autoptr (GMutexLocker) locker = NULL;

  return g_list_model_get_n_items (G_LIST_MODEL (self->store));
}

static gpointer
list_model_get_item (GListModel *list,
                     guint       position)
{
  SaturnThreadsafeListStore *self = SATURN_THREADSAFE_LIST_STORE (list);
  g_autoptr (GMutexLocker) locker = NULL;

  return g_list_model_get_item (G_LIST_MODEL (self->store), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = list_model_get_item_type;
  iface->get_n_items   = list_model_get_n_items;
  iface->get_item      = list_model_get_item;
}

SaturnThreadsafeListStore *
saturn_threadsafe_list_store_new (GCompareDataFunc sort_func,
                                  gpointer         user_data,
                                  GDestroyNotify   destroy_user_data)
{
  SaturnThreadsafeListStore *object = NULL;

  object                    = g_object_new (SATURN_TYPE_THREADSAFE_LIST_STORE, NULL);
  object->sort_func         = sort_func;
  object->user_data         = user_data;
  object->destroy_user_data = destroy_user_data;

  return object;
}

gboolean
saturn_threadsafe_list_store_append (SaturnThreadsafeListStore *self,
                                     gpointer                   item)
{
  g_autoptr (GMutexLocker) locker = NULL;

  g_return_val_if_fail (SATURN_THREADSAFE_LIST_STORE (self), FALSE);
  g_return_val_if_fail (G_IS_OBJECT (item), FALSE);

  locker = g_mutex_locker_new (&self->buildup_mutex);
  if (self->cancelled)
    return FALSE;
  else
    {
      g_ptr_array_add (self->buildup, g_object_ref (item));
      return TRUE;
    }
}

void
saturn_threadsafe_list_store_cancel (SaturnThreadsafeListStore *self)
{
  g_autoptr (GMutexLocker) locker = NULL;

  g_return_if_fail (SATURN_THREADSAFE_LIST_STORE (self));

  locker          = g_mutex_locker_new (&self->buildup_mutex);
  self->cancelled = TRUE;
}

static void
store_changed (SaturnThreadsafeListStore *self,
               guint                      position,
               guint                      removed,
               guint                      added,
               GListModel                *model)
{
  g_list_model_items_changed (
      G_LIST_MODEL (self), position, removed, added);
}

static gboolean
idle_update_cb (GWeakRef *wr)
{
  g_autoptr (SaturnThreadsafeListStore) self = NULL;
  guint position                             = 0;
  guint added                                = 0;

  self = g_weak_ref_get (wr);
  if (self == NULL)
    return G_SOURCE_REMOVE;

  g_mutex_lock (&self->buildup_mutex);

#define MAX_INSERTS 512

  position = g_list_model_get_n_items (G_LIST_MODEL (self->store));
  added    = MIN (self->buildup->len, MAX_INSERTS);

  if (self->sort_func != NULL)
    {
      for (guint i = 0; i < added; i++)
        {
          gpointer item = NULL;

          item = g_ptr_array_index (self->buildup, i);
          g_list_store_insert_sorted (self->store, item, self->sort_func, self->user_data);
        }
    }
  else
    g_list_store_splice (
        self->store,
        position,
        0,
        self->buildup->pdata,
        added);

  g_ptr_array_remove_range (self->buildup, 0, added);
  g_mutex_unlock (&self->buildup_mutex);

  return G_SOURCE_CONTINUE;
}

/* End of saturn-threadsafe-list-store.c */
