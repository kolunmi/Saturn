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

enum
{
  UNKNOWN,
  TEXT,
  IMAGE,
};

typedef struct
{
  int        type;
  GPtrArray *children;
  char       component[];
} FsNode;

static void
destroy_fs_node (gpointer ptr);

SATURN_DEFINE_DATA (
    work,
    Work,
    {
      gboolean    active;
      GMutex      mutex;
      GHashTable *paths;
      DexChannel *channel;
      char       *query;
    },
    g_mutex_clear (&self->mutex);
    SATURN_RELEASE_DATA (paths, g_hash_table_unref);
    SATURN_RELEASE_DATA (channel, dex_unref);
    SATURN_RELEASE_DATA (query, g_free))

static DexFuture *
work_fiber (WorkData *data);

static void
work_recurse (WorkData  *data,
              GFile     *file,
              GPtrArray *parent);

struct _SaturnFileSystemProvider
{
  GObject parent_instance;

  DexFuture *work;
  WorkData  *data;
};

static void
provider_iface_init (SaturnProviderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SaturnFileSystemProvider,
    saturn_file_system_provider,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (SATURN_TYPE_PROVIDER, provider_iface_init))

SATURN_DEFINE_DATA (
    query,
    Query,
    {
      GWeakRef    self;
      WorkData   *work_data;
      gpointer    object;
      DexChannel *channel;
    },
    g_weak_ref_clear (&self->self);
    SATURN_RELEASE_DATA (work_data, work_data_unref);
    SATURN_RELEASE_DATA (object, g_object_unref);
    SATURN_RELEASE_DATA (channel, dex_unref))

static DexFuture *
query_fiber (QueryData *data);

static gboolean
query_recurse (DexChannel *channel,
               GWeakRef   *wr,
               FsNode     *node,
               GPtrArray  *components,
               const char *query);

static void
saturn_file_system_provider_dispose (GObject *object)
{
  SaturnFileSystemProvider *self = SATURN_FILE_SYSTEM_PROVIDER (object);

  dex_clear (&self->work);
  g_clear_pointer (&self->data, work_data_unref);

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
  g_autoptr (WorkData) data = NULL;

  data = work_data_new ();
  g_mutex_init (&data->mutex);
  data->paths = g_hash_table_new_full (
      g_str_hash,
      g_str_equal,
      g_free,
      (GDestroyNotify) g_ptr_array_unref);

  self->data = g_steal_pointer (&data);
}

static DexFuture *
provider_init_global (SaturnProvider *provider)
{
  SaturnFileSystemProvider *self = SATURN_FILE_SYSTEM_PROVIDER (provider);

  g_mutex_lock (&self->data->mutex);
  self->data->active = TRUE;
  g_mutex_unlock (&self->data->mutex);

  dex_clear (&self->work);
  self->work = dex_scheduler_spawn (
      dex_thread_pool_scheduler_get_default (),
      0, (DexFiberFunc) work_fiber,
      work_data_ref (self->data), work_data_unref);

  return dex_future_new_true ();
}

static DexChannel *
provider_query (SaturnProvider *provider,
                GObject        *object)
{
  SaturnFileSystemProvider *self = SATURN_FILE_SYSTEM_PROVIDER (provider);
  g_autoptr (DexChannel) channel = NULL;

  channel = dex_channel_new (1);
  if (GTK_IS_STRING_OBJECT (object))
    {
      g_autoptr (QueryData) data = NULL;

      data = query_data_new ();
      g_weak_ref_init (&data->self, self);
      data->work_data = work_data_ref (self->data);
      data->channel   = dex_ref (channel);
      data->object    = g_object_ref (object);

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
  const char      *search   = NULL;
  g_autofree char *basename = NULL;
  gsize            score    = 0;

  if (!GTK_IS_STRING_OBJECT (query))
    return 0;

  search   = gtk_string_object_get_string (GTK_STRING_OBJECT (query));
  basename = g_file_get_basename (G_FILE (item));

  score = G_MAXSIZE / strlen (basename) * strlen (search);
  g_object_set_qdata (
      G_OBJECT (item),
      SATURN_PROVIDER_SCORE_QUARK,
      GSIZE_TO_POINTER (score));

  return score;
}

static void
provider_bind_list_item (SaturnProvider *self,
                         gpointer        object,
                         AdwBin         *list_item)
{
  g_autofree char *basename    = NULL;
  g_autoptr (GFile) parent     = NULL;
  g_autofree char *parent_path = NULL;
  GtkWidget       *left_label  = NULL;
  GtkWidget       *right_label = NULL;
  GtkWidget       *box         = NULL;

  basename = g_file_get_basename (G_FILE (object));

  parent      = g_file_get_parent (G_FILE (object));
  parent_path = g_file_get_path (parent);

  left_label = gtk_label_new (basename);
  gtk_label_set_xalign (GTK_LABEL (left_label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (left_label), PANGO_ELLIPSIZE_END);

  right_label = gtk_label_new (parent_path);
  gtk_widget_set_hexpand (right_label, TRUE);
  gtk_label_set_xalign (GTK_LABEL (right_label), 1.0);
  gtk_label_set_ellipsize (GTK_LABEL (right_label), PANGO_ELLIPSIZE_END);
  gtk_widget_add_css_class (right_label, "dimmed");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (box), left_label);
  gtk_box_append (GTK_BOX (box), right_label);

  adw_bin_set_child (list_item, box);
}

static void
provider_bind_preview (SaturnProvider *self,
                       gpointer        object,
                       AdwBin         *preview)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GBytes) bytes       = NULL;

  bytes = dex_await_boxed (
      dex_file_load_contents_bytes (G_FILE (object)),
      &local_error);
  if (bytes != NULL)
    {
      g_autoptr (GtkTextBuffer) buffer = NULL;
      GtkWidget *view                  = NULL;
      GtkWidget *window                = NULL;

      buffer = gtk_text_buffer_new (NULL);
      gtk_text_buffer_set_text (
          buffer,
          g_bytes_get_data (bytes, NULL), -1);

      view = gtk_text_view_new_with_buffer (buffer);
      gtk_widget_add_css_class (view, "monospace");

      window = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (window), view);

      adw_bin_set_child (preview, window);
    }
  else
    {
      GtkWidget *label = NULL;

      label = gtk_label_new (local_error->message);
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_widget_add_css_class (label, "error");
      gtk_widget_add_css_class (label, "title-4");

      adw_bin_set_child (preview, label);
    }
}

static void
provider_iface_init (SaturnProviderInterface *iface)
{
  iface->init_global    = provider_init_global;
  iface->query          = provider_query;
  iface->score          = provider_score;
  iface->bind_list_item = provider_bind_list_item;
  iface->bind_preview   = provider_bind_preview;
}

static void
destroy_fs_node (gpointer ptr)
{
  FsNode *node = ptr;

  g_clear_pointer (&node->children, g_ptr_array_unref);
  g_free (node);
}

static DexFuture *
work_fiber (WorkData *data)
{
  const char *home            = NULL;
  g_autoptr (GFile) file      = NULL;
  g_autoptr (GPtrArray) array = NULL;

  home  = g_get_home_dir ();
  file  = g_file_new_for_path (home);
  array = g_ptr_array_new_with_free_func (destroy_fs_node);

  g_hash_table_replace (data->paths, g_strdup (home), g_ptr_array_ref (array));
  work_recurse (data, file, array);

  g_mutex_lock (&data->mutex);
  if (data->channel != NULL)
    {
      dex_channel_close_send (data->channel);
      dex_clear (&data->channel);
      g_clear_pointer (&data->query, g_free);
    }
  data->active = FALSE;
  g_mutex_unlock (&data->mutex);

  return dex_future_new_true ();
}

static void
work_recurse (WorkData  *data,
              GFile     *file,
              GPtrArray *parent)
{
  g_autoptr (GError) local_error         = NULL;
  g_autofree gchar *uri                  = NULL;
  g_autoptr (GFileEnumerator) enumerator = NULL;

  uri        = g_file_get_uri (file);
  enumerator = g_file_enumerate_children (
      file,
      G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK
      "," G_FILE_ATTRIBUTE_STANDARD_NAME
      "," G_FILE_ATTRIBUTE_STANDARD_TYPE
      "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE
      "," G_FILE_ATTRIBUTE_TIME_MODIFIED,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
      NULL,
      &local_error);
  if (enumerator == NULL)
    return;

  for (;;)
    {
      g_autoptr (GFileInfo) info    = NULL;
      g_autoptr (GFile) child       = NULL;
      GFileType        file_type    = G_FILE_TYPE_UNKNOWN;
      const char      *content_type = NULL;
      g_autofree char *basename     = NULL;
      FsNode          *node         = NULL;

      info = g_file_enumerator_next_file (enumerator, NULL, &local_error);
      if (info == NULL)
        {
          if (local_error != NULL)
            g_warning ("Failed to enumerate directory '%s': %s", uri, local_error->message);
          g_clear_pointer (&local_error, g_error_free);
          break;
        }

      child        = g_file_enumerator_get_child (enumerator, info);
      file_type    = g_file_info_get_file_type (info);
      content_type = g_file_info_get_content_type (info);
      basename     = g_file_get_basename (child);

      if (g_strcmp0 (basename, ".config") != 0 &&
          g_str_has_prefix (basename, "."))
        continue;

      node = g_malloc0 (sizeof (FsNode) + strlen (basename) + 1);
      if (g_content_type_is_a (content_type, "image"))
        node->type = IMAGE;
      else if (g_content_type_is_a (content_type, "text"))
        node->type = TEXT;
      else
        node->type = UNKNOWN;
      if (file_type == G_FILE_TYPE_DIRECTORY)
        node->children = g_ptr_array_new_with_free_func (destroy_fs_node);
      strcpy (node->component, basename);

      g_mutex_lock (&data->mutex);
      g_ptr_array_add (parent, node);
      if (file_type != G_FILE_TYPE_DIRECTORY &&
          data->channel != NULL &&
          strcasestr (node->component, data->query) != NULL)
        {
          gboolean result = FALSE;

          result = dex_await (
              dex_channel_send (
                  data->channel,
                  dex_future_new_for_object (child)),
              NULL);
          if (!result)
            {
              dex_channel_close_send (data->channel);
              dex_clear (&data->channel);
              g_clear_pointer (&data->query, g_free);
            }
        }
      g_mutex_unlock (&data->mutex);

      if (file_type == G_FILE_TYPE_DIRECTORY)
        work_recurse (data, child, node->children);
    }
}

static DexFuture *
query_fiber (QueryData *data)
{
  const char *query               = NULL;
  g_autoptr (GMutexLocker) locker = NULL;
  GHashTableIter iter             = { 0 };

  query = gtk_string_object_get_string (GTK_STRING_OBJECT (data->object));

  locker = g_mutex_locker_new (&data->work_data->mutex);

  if (data->work_data->channel != NULL)
    dex_channel_close_send (data->work_data->channel);
  dex_clear (&data->work_data->channel);
  g_clear_pointer (&data->work_data->query, g_free);

  g_hash_table_iter_init (&iter, data->work_data->paths);
  for (;;)
    {
      char      *prefix                = NULL;
      GPtrArray *array                 = NULL;
      gboolean   result                = FALSE;
      g_autoptr (GPtrArray) components = NULL;

      result = g_hash_table_iter_next (
          &iter,
          (gpointer *) &prefix,
          (gpointer *) &array);
      if (!result)
        break;

      components = g_ptr_array_new ();
      g_ptr_array_add (components, prefix);

      for (guint i = 0; i < array->len; i++)
        {
          FsNode *node = NULL;

          node = g_ptr_array_index (array, i);

          g_ptr_array_add (components, node->component);
          result = query_recurse (data->channel, &data->self, node, components, query);
          g_ptr_array_remove_index (components, components->len - 1);

          if (!result)
            return dex_future_new_false ();
        }
    }

  if (data->work_data->active)
    {
      data->work_data->channel = dex_ref (data->channel);
      data->work_data->query   = g_strdup (query);
    }
  else
    dex_channel_close_send (data->channel);

  return dex_future_new_true ();
}

static gboolean
query_recurse (DexChannel *channel,
               GWeakRef   *wr,
               FsNode     *node,
               GPtrArray  *components,
               const char *query)
{
  gboolean result = FALSE;

  if (node->children != NULL)
    {
      for (guint i = 0; i < node->children->len; i++)
        {
          FsNode *child = NULL;

          child = g_ptr_array_index (node->children, i);

          g_ptr_array_add (components, child->component);
          result = query_recurse (channel, wr, child, components, query);
          g_ptr_array_remove_index (components, components->len - 1);

          if (!result)
            return FALSE;
        }
    }
  else if (strcasestr (node->component, query) != NULL)
    {
      g_autoptr (GString) path                  = NULL;
      g_autoptr (GFile) file                    = NULL;
      g_autoptr (SaturnFileSystemProvider) self = NULL;

      path = g_string_new (g_ptr_array_index (components, 0));
      for (guint i = 1; i < components->len; i++)
        g_string_append_printf (
            path,
            "/%s",
            (char *) g_ptr_array_index (components, i));

      file = g_file_new_for_path (path->str);
      self = g_weak_ref_get (wr);
      g_object_set_qdata_full (
          G_OBJECT (file),
          SATURN_PROVIDER_QUARK,
          g_steal_pointer (&self),
          g_object_unref);

      result = dex_await (
          dex_channel_send (
              channel,
              dex_future_new_for_object (file)),
          NULL);
      if (!result)
        {
          dex_channel_close_send (channel);
          return FALSE;
        }
    }

  return TRUE;
}
