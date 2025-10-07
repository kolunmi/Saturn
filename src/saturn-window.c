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

#include <libdex.h>

#include "saturn-provider.h"
#include "saturn-window.h"
#include "util.h"

struct _SaturnWindow
{
  AdwApplicationWindow parent_instance;

  GListModel *providers;

  GListStore *model;
  DexFuture  *task;

  /* Template widgets */
  GtkSingleSelection *selection;
};

G_DEFINE_FINAL_TYPE (SaturnWindow, saturn_window, ADW_TYPE_APPLICATION_WINDOW)

enum
{
  PROP_0,

  PROP_PROVIDERS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

SATURN_DEFINE_DATA (
    query,
    Query,
    {
      GWeakRef self;
      gpointer query;
    },
    g_weak_ref_clear (&self->self);
    SATURN_RELEASE_DATA (query, g_object_unref))

static DexFuture *
query_fiber (QueryData *data);

static void
saturn_window_dispose (GObject *object)
{
  SaturnWindow *self = SATURN_WINDOW (object);

  g_clear_object (&self->providers);

  g_clear_object (&self->model);
  dex_clear (&self->task);

  G_OBJECT_CLASS (saturn_window_parent_class)->dispose (object);
}

static void
saturn_window_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  SaturnWindow *self = SATURN_WINDOW (object);

  switch (prop_id)
    {
    case PROP_PROVIDERS:
      g_value_set_object (value, saturn_window_get_providers (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_window_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  SaturnWindow *self = SATURN_WINDOW (object);

  switch (prop_id)
    {
    case PROP_PROVIDERS:
      saturn_window_set_providers (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
text_changed_cb (SaturnWindow *self,
                 GtkEditable  *editable)
{
  const char *text                   = NULL;
  g_autoptr (GtkStringObject) string = NULL;
  g_autoptr (QueryData) data         = NULL;

  dex_clear (&self->task);
  g_list_store_remove_all (self->model);

  text = gtk_editable_get_text (editable);
  if (text == NULL || *text == '\0')
    return;
  string = gtk_string_object_new (text);

  data = query_data_new ();
  g_weak_ref_init (&data->self, self);
  data->query = g_object_ref (string);

  self->task = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      0, (DexFiberFunc) query_fiber,
      query_data_ref (data), query_data_unref);
}

static void
list_item_setup_cb (SaturnWindow             *self,
                    GtkListItem              *list_item,
                    GtkSignalListItemFactory *factory)
{
  gtk_list_item_set_child (list_item, adw_bin_new ());
}

static void
list_item_teardown_cb (SaturnWindow             *self,
                       GtkListItem              *list_item,
                       GtkSignalListItemFactory *factory)
{
  gtk_list_item_set_child (list_item, NULL);
}

static void
list_item_bind_cb (SaturnWindow             *self,
                   GtkListItem              *list_item,
                   GtkSignalListItemFactory *factory)
{
  gpointer        item     = NULL;
  SaturnProvider *provider = NULL;
  GtkWidget      *bin      = NULL;

  item     = gtk_list_item_get_item (list_item);
  provider = g_object_get_qdata (G_OBJECT (item), SATURN_PROVIDER_QUARK);
  if (provider == NULL)
    return;

  bin = gtk_list_item_get_child (list_item);
  saturn_provider_bind_list_item (provider, ADW_BIN (bin));
}

static void
list_item_unbind_cb (SaturnWindow             *self,
                     GtkListItem              *list_item,
                     GtkSignalListItemFactory *factory)
{
  gpointer        item     = NULL;
  SaturnProvider *provider = NULL;
  GtkWidget      *bin      = NULL;

  item     = gtk_list_item_get_item (list_item);
  provider = g_object_get_qdata (G_OBJECT (item), SATURN_PROVIDER_QUARK);
  if (provider == NULL)
    return;

  bin = gtk_list_item_get_child (list_item);
  saturn_provider_unbind_list_item (provider, ADW_BIN (bin));
}

static void
saturn_window_class_init (SaturnWindowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = saturn_window_dispose;
  object_class->get_property = saturn_window_get_property;
  object_class->set_property = saturn_window_set_property;

  props[PROP_PROVIDERS] =
      g_param_spec_object (
          "providers",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Saturn/saturn-window.ui");
  gtk_widget_class_bind_template_child (widget_class, SaturnWindow, selection);
  gtk_widget_class_bind_template_callback (widget_class, text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_setup_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_teardown_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_bind_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_unbind_cb);
}

static void
saturn_window_init (SaturnWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->model = g_list_store_new (G_TYPE_OBJECT);
  gtk_single_selection_set_model (self->selection, G_LIST_MODEL (self->model));
}

void
saturn_window_set_providers (SaturnWindow *self,
                             GListModel   *providers)
{
  g_return_if_fail (SATURN_IS_WINDOW (self));
  g_return_if_fail (providers == NULL || G_IS_LIST_MODEL (providers));

  g_clear_object (&self->providers);
  if (providers != NULL)
    self->providers = g_object_ref (providers);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PROVIDERS]);
}

GListModel *
saturn_window_get_providers (SaturnWindow *self)
{
  g_return_val_if_fail (SATURN_IS_WINDOW (self), NULL);
  return self->providers;
}

static DexFuture *
query_fiber (QueryData *data)
{
  g_autoptr (SaturnWindow) self  = NULL;
  gpointer query                 = data->query;
  guint    n_providers           = 0;
  g_autoptr (GPtrArray) channels = NULL;

  self = g_weak_ref_get (&data->self);
  if (self == NULL || self->providers == NULL)
    return NULL;

  n_providers = g_list_model_get_n_items (self->providers);
  channels    = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_providers; i++)
    {
      g_autoptr (SaturnProvider) provider = NULL;
      g_autoptr (DexChannel) channel      = NULL;

      provider = g_list_model_get_item (self->providers, i);
      channel  = saturn_provider_query (provider, query);

      if (channel != NULL)
        g_ptr_array_add (channels, g_steal_pointer (&channel));
    }

  for (;;)
    {
      g_autoptr (GPtrArray) futures = NULL;
      g_autoptr (GObject) object    = NULL;

      futures = g_ptr_array_new_with_free_func (dex_unref);
      for (guint i = 0; i < channels->len;)
        {
          DexChannel *channel = NULL;

          channel = g_ptr_array_index (channels, i);
          if (dex_channel_can_receive (channel))
            {
              g_ptr_array_add (futures, dex_channel_receive (channel));
              i++;
            }
          else
            g_ptr_array_remove_index (channels, i);
        }
      if (futures->len == 0)
        break;

      object = dex_await_object (
          dex_future_anyv (
              (DexFuture *const *) futures->pdata,
              futures->len),
          NULL);
      if (object == NULL)
        break;

      g_list_store_append (self->model, object);
    }

  return dex_future_new_true ();
}
