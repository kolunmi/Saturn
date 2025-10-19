/* provider.c
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

#define G_LOG_DOMAIN "SATURN::FILE-SYSTEM-PROVIDER"

#include "config.h"

#include "provider.h"
#include "saturn-provider.h"
#include "util.h"

struct _SaturnAppInfoProvider
{
  GObject parent_instance;

  GMutex     mutex;
  DexFuture *init;
  GList     *infos;
};

static void
provider_iface_init (SaturnProviderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SaturnAppInfoProvider,
    saturn_app_info_provider,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (SATURN_TYPE_PROVIDER, provider_iface_init))

SATURN_DEFINE_DATA (
    init,
    Init,
    {
      GWeakRef self;
    },
    g_weak_ref_clear (&self->self);)

static DexFuture *
init_fiber (InitData *data);

SATURN_DEFINE_DATA (
    query,
    Query,
    {
      GWeakRef    self;
      gpointer    object;
      DexChannel *channel;
    },
    g_weak_ref_clear (&self->self);
    SATURN_RELEASE_DATA (object, g_object_unref);
    SATURN_RELEASE_DATA (channel, dex_unref))

static DexFuture *
query_fiber (QueryData *data);

static void
saturn_app_info_provider_dispose (GObject *object)
{
  SaturnAppInfoProvider *self = SATURN_APP_INFO_PROVIDER (object);

  dex_clear (&self->init);
  g_mutex_clear (&self->mutex);
  g_list_free_full (self->infos, g_object_unref);

  G_OBJECT_CLASS (saturn_app_info_provider_parent_class)->dispose (object);
}

static void
saturn_app_info_provider_class_init (SaturnAppInfoProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = saturn_app_info_provider_dispose;
}

static void
saturn_app_info_provider_init (SaturnAppInfoProvider *self)
{
}

static DexFuture *
provider_init_global (SaturnProvider *provider)
{
  SaturnAppInfoProvider *self = SATURN_APP_INFO_PROVIDER (provider);
  g_autoptr (InitData) data   = NULL;

  data = init_data_new ();
  g_weak_ref_init (&data->self, self);

  dex_clear (&self->init);
  self->init = dex_scheduler_spawn (
      dex_thread_pool_scheduler_get_default (),
      0, (DexFiberFunc) init_fiber,
      init_data_ref (data), init_data_unref);

  return dex_future_new_true ();
}

static DexChannel *
provider_query (SaturnProvider *provider,
                GObject        *object)
{
  SaturnAppInfoProvider *self    = SATURN_APP_INFO_PROVIDER (provider);
  g_autoptr (DexChannel) channel = NULL;

  channel = dex_channel_new (32);
  if (GTK_IS_STRING_OBJECT (object))
    {
      g_autoptr (QueryData) data = NULL;

      data = query_data_new ();
      g_weak_ref_init (&data->self, self);
      data->object  = g_object_ref (object);
      data->channel = dex_ref (channel);

      dex_future_disown (dex_scheduler_spawn (
          dex_thread_pool_scheduler_get_default (),
          0, (DexFiberFunc) query_fiber,
          query_data_ref (data), query_data_unref));
    }
  else
    dex_channel_close_send (channel);

  return g_steal_pointer (&channel);
}

static gsize
provider_score (SaturnProvider *self,
                gpointer        item,
                GObject        *query)
{
  const char *search     = NULL;
  const char *id         = NULL;
  const char *name       = NULL;
  gsize       id_score   = 0;
  gsize       name_score = 0;
  gsize       score      = 0;

  if (!GTK_IS_STRING_OBJECT (query))
    return 0;

  search = gtk_string_object_get_string (GTK_STRING_OBJECT (query));
  id     = g_app_info_get_id (G_APP_INFO (item));
  name   = g_app_info_get_name (G_APP_INFO (item));

  if (id != NULL && strcasestr (id, search))
    id_score = 1 + (gsize) (SATURN_PROVIDER_MAX_SCORE_DOUBLE *
                            ((double) strlen (search) /
                             (double) strlen (id)));
  if (name != NULL && strcasestr (name, search))
    name_score = 1 + (gsize) (SATURN_PROVIDER_MAX_SCORE_DOUBLE *
                              ((double) strlen (search) /
                               (double) strlen (name)));

  score = MAX (id_score, name_score);
  g_object_set_qdata (
      G_OBJECT (item),
      SATURN_PROVIDER_SCORE_QUARK,
      GSIZE_TO_POINTER (score));

  return score;
}

static void
launch_finish (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  DexPromise *promise            = user_data;
  g_autoptr (GError) local_error = NULL;
  gboolean success               = FALSE;

  success = g_app_info_launch_uris_finish (G_APP_INFO (object), result, &local_error);
  if (success)
    dex_promise_resolve_boolean (promise, TRUE);
  else
    dex_promise_reject (promise, g_steal_pointer (&local_error));

  dex_unref (promise);
}

static gboolean
provider_select (SaturnProvider *self,
                 gpointer        item,
                 GObject        *query,
                 GError        **error)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (DexPromise) promise = NULL;
  gboolean result                = FALSE;

  promise = dex_promise_new_cancellable ();

  g_app_info_launch_uris_async (
      G_APP_INFO (item),
      NULL, NULL,
      dex_promise_get_cancellable (promise),
      launch_finish,
      dex_ref (promise));

  result = dex_await_boolean (
      (DexFuture *) g_steal_pointer (&promise),
      &local_error);
  if (!result)
    {
      const char *id = NULL;

      id = g_app_info_get_id (G_APP_INFO (item));

      g_critical ("Could not launch id %s: %s", id, local_error->message);
      g_propagate_error (error, g_steal_pointer (&local_error));
    }

  return result;
}

static void
provider_bind_list_item (SaturnProvider *self,
                         gpointer        object,
                         AdwBin         *list_item)
{
  const char *id          = NULL;
  const char *name        = NULL;
  GIcon      *icon        = NULL;
  GtkWidget  *image       = NULL;
  GtkWidget  *left_label  = NULL;
  GtkWidget  *right_label = NULL;
  GtkWidget  *box         = NULL;

  id   = g_app_info_get_id (G_APP_INFO (object));
  name = g_app_info_get_name (G_APP_INFO (object));
  icon = g_app_info_get_icon (G_APP_INFO (object));

  image = gtk_image_new_from_gicon (icon);
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);

  left_label = gtk_label_new (name);
  gtk_label_set_xalign (GTK_LABEL (left_label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (left_label), PANGO_ELLIPSIZE_END);
  gtk_widget_add_css_class (left_label, "title-4");

  right_label = gtk_label_new (id);
  gtk_widget_set_hexpand (right_label, TRUE);
  gtk_label_set_xalign (GTK_LABEL (right_label), 1.0);
  gtk_label_set_ellipsize (GTK_LABEL (right_label), PANGO_ELLIPSIZE_END);
  gtk_widget_add_css_class (right_label, "dimmed");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (box), image);
  gtk_box_append (GTK_BOX (box), left_label);
  gtk_box_append (GTK_BOX (box), right_label);

  adw_bin_set_child (list_item, box);
}

static void
provider_bind_preview (SaturnProvider *self,
                       gpointer        object,
                       AdwBin         *preview)
{
}

static void
provider_iface_init (SaturnProviderInterface *iface)
{
  iface->init_global    = provider_init_global;
  iface->query          = provider_query;
  iface->score          = provider_score;
  iface->select         = provider_select;
  iface->bind_list_item = provider_bind_list_item;
  iface->bind_preview   = provider_bind_preview;
}

static DexFuture *
init_fiber (InitData *data)
{
  g_autoptr (SaturnAppInfoProvider) self = NULL;

  self = g_weak_ref_get (&data->self);
  if (self == NULL)
    return NULL;

  self->infos = g_app_info_get_all ();

  return dex_future_new_true ();
}

static DexFuture *
query_fiber (QueryData *data)
{
  g_autoptr (SaturnAppInfoProvider) self = NULL;
  const char *query                      = NULL;
  g_autoptr (GPtrArray) ret              = NULL;
  g_autoptr (GMutexLocker) locker        = NULL;
  gboolean result                        = FALSE;

  self = g_weak_ref_get (&data->self);
  if (self == NULL)
    {
      dex_channel_close_send (data->channel);
      return NULL;
    }

  query = gtk_string_object_get_string (GTK_STRING_OBJECT (data->object));
  ret   = g_ptr_array_new_with_free_func (g_object_unref);

  if (self->init != NULL &&
      !dex_await (dex_ref (self->init), NULL))
    {
      dex_channel_close_send (data->channel);
      return dex_future_new_true ();
    }

  locker = g_mutex_locker_new (&self->mutex);

  for (GList *list = self->infos; list != NULL; list = list->next)
    {
      GAppInfo   *info = list->data;
      const char *id   = NULL;
      const char *name = NULL;

      id   = g_app_info_get_id (info);
      name = g_app_info_get_name (info);

      if ((id != NULL && strcasestr (id, query)) ||
          (name != NULL && strcasestr (name, query)))
        {
          g_autoptr (GAppInfo) dup = NULL;

          dup = g_app_info_dup (info);
          g_object_set_qdata_full (
              G_OBJECT (dup),
              SATURN_PROVIDER_QUARK,
              g_object_ref (self),
              g_object_unref);

          g_ptr_array_add (ret, g_steal_pointer (&dup));
        }
    }

  result = dex_await (
      dex_channel_send (
          data->channel,
          dex_future_new_take_boxed (
              G_TYPE_PTR_ARRAY,
              g_steal_pointer (&ret))),
      NULL);
  if (!result)
    {
      dex_channel_close_send (data->channel);
      return dex_future_new_true ();
    }

  dex_channel_close_send (data->channel);
  return dex_future_new_true ();
}
