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
  gpointer         sort_data;
  GDestroyNotify   sort_destroy_data;

  GMutex      api_mutex;
  GListStore *store;
  guint       update_timeout;
  GMutex      buildup_mutex;
  GPtrArray  *buildup;
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
idle_cb (IdleModifyData *data);

static gboolean
idle_update_cb (GWeakRef *wr);

static void
saturn_threadsafe_list_store_dispose (GObject *object)
{
  SaturnThreadsafeListStore *self = SATURN_THREADSAFE_LIST_STORE (object);

  g_mutex_clear (&self->api_mutex);
  g_mutex_clear (&self->buildup_mutex);
  g_clear_pointer (&self->buildup, g_ptr_array_unref);
  g_clear_object (&self->store);
  g_clear_handle_id (&self->update_timeout, g_source_remove);

  if (self->sort_data != NULL &&
      self->sort_destroy_data != NULL)
    self->sort_destroy_data (self->sort_data);

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
  g_mutex_init (&self->api_mutex);
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
      100,
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
  SaturnThreadsafeListStore *self    = SATURN_THREADSAFE_LIST_STORE (list);
  gboolean                   locked  = FALSE;
  guint                      n_items = 0;

  /* It's fine to get this data if we are already locked, we just want to
     prevent the underlying store from being mutated while retrieving the
     data */
  locked  = g_mutex_trylock (&self->api_mutex);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->store));
  if (locked)
    g_mutex_unlock (&self->api_mutex);

  return n_items;
}

static gpointer
list_model_get_item (GListModel *list,
                     guint       position)
{
  SaturnThreadsafeListStore *self   = SATURN_THREADSAFE_LIST_STORE (list);
  gboolean                   locked = FALSE;
  g_autoptr (GObject) item          = NULL;

  locked = g_mutex_trylock (&self->api_mutex);
  item   = g_list_model_get_item (G_LIST_MODEL (self->store), position);
  if (locked)
    g_mutex_unlock (&self->api_mutex);

  return g_steal_pointer (&item);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = list_model_get_item_type;
  iface->get_n_items   = list_model_get_n_items;
  iface->get_item      = list_model_get_item;
}

SaturnThreadsafeListStore *
saturn_threadsafe_list_store_new (GCompareDataFunc sort_cmp,
                                  gpointer         sort_data,
                                  GDestroyNotify   sort_destroy_data)
{
  SaturnThreadsafeListStore *object = NULL;

  object                    = g_object_new (SATURN_TYPE_THREADSAFE_LIST_STORE, NULL);
  object->sort_func         = sort_cmp;
  object->sort_data         = sort_data;
  object->sort_destroy_data = sort_destroy_data;

  return object;
}

void
saturn_threadsafe_list_store_insert_sorted (SaturnThreadsafeListStore *self,
                                            gpointer                   item)
{
  g_autoptr (GMutexLocker) locker = NULL;

  g_return_if_fail (SATURN_THREADSAFE_LIST_STORE (self));
  g_return_if_fail (self->sort_func != NULL);
  g_return_if_fail (G_IS_OBJECT (item));

  locker = g_mutex_locker_new (&self->buildup_mutex);
  g_ptr_array_add (self->buildup, g_object_ref (item));
}

void
saturn_threadsafe_list_store_clear_all (SaturnThreadsafeListStore *self)
{
  g_autoptr (IdleModifyData) data = NULL;

  g_return_if_fail (SATURN_THREADSAFE_LIST_STORE (self));

  data       = idle_modify_data_new ();
  data->self = saturn_track_weak (self);

  g_idle_add_full (
      G_PRIORITY_DEFAULT_IDLE,
      (GSourceFunc) idle_cb,
      idle_modify_data_ref (data),
      idle_modify_data_unref);
}

static void
store_changed (SaturnThreadsafeListStore *self,
               guint                      position,
               guint                      removed,
               guint                      added,
               GListModel                *model)
{
  /* Our internal implementation _must_ lock the mutex before the
     "items-changed" signal can be emitted */
  g_list_model_items_changed (
      G_LIST_MODEL (self), position, removed, added);
}

static gboolean
idle_cb (IdleModifyData *data)
{
  g_autoptr (SaturnThreadsafeListStore) self = NULL;
  g_autoptr (GMutexLocker) locker            = NULL;

  self = g_weak_ref_get (data->self);
  if (self == NULL)
    goto done;

  locker = g_mutex_locker_new (&self->api_mutex);
  if (data->item != NULL)
    g_list_store_insert_sorted (self->store, data->item, self->sort_func, self->sort_data);
  else
    /* No item means to clear */
    g_list_store_remove_all (self->store);

done:
  return G_SOURCE_REMOVE;
}

static gboolean
idle_update_cb (GWeakRef *wr)
{
  g_autoptr (SaturnThreadsafeListStore) self = NULL;
  g_autoptr (GMutexLocker) buildup_locker    = NULL;
  g_autoptr (GMutexLocker) api_locker        = NULL;
  guint i                                    = 0;

  self = g_weak_ref_get (wr);
  if (self == NULL)
    {
      self->update_timeout = 0;
      return G_SOURCE_REMOVE;
    }

#define MAX_INSERTS 1024

  buildup_locker = g_mutex_locker_new (&self->buildup_mutex);
  api_locker     = g_mutex_locker_new (&self->api_mutex);

  for (i = 0; i < MIN (self->buildup->len, MAX_INSERTS); i++)
    {
      gpointer item = NULL;

      item = g_ptr_array_index (self->buildup, i);
      g_list_store_insert_sorted (self->store, item, self->sort_func, self->sort_data);
    }
  g_ptr_array_remove_range (self->buildup, 0, i);

  return G_SOURCE_CONTINUE;
}

/* End of saturn-threadsafe-list-store.c */
