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
#include <glib/gi18n.h>

#include <libdex.h>

#include "saturn-provider.h"
#include "saturn-window.h"
#include "util.h"

SATURN_DEFINE_DATA (
    query,
    Query,
    {
      SaturnWindow *self;
      gpointer      query;
    },
    SATURN_RELEASE_DATA (query, g_object_unref));

static DexFuture *
query_fiber (QueryData *data);

struct _SaturnWindow
{
  AdwApplicationWindow parent_instance;

  GListModel *providers;

  GListStore     *model;
  DexFuture      *task;
  QueryData      *task_data;
  DexCancellable *cancel;

  guint      debounce;
  DexFuture *make_preview;

  gboolean explicit_select;

  /* Template widgets */
  GtkLabel           *status_label;
  GtkSingleSelection *selection;
  GtkListView        *list_view;
  AdwBin             *preview_bin;
};

G_DEFINE_FINAL_TYPE (SaturnWindow, saturn_window, ADW_TYPE_APPLICATION_WINDOW)

enum
{
  PROP_0,

  PROP_PROVIDERS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static gint
cmp_item (GObject *a,
          GObject *b,
          GObject *query);

static void
saturn_window_dispose (GObject *object)
{
  SaturnWindow *self = SATURN_WINDOW (object);

  g_clear_object (&self->providers);

  dex_clear (&self->task);
  dex_clear (&self->cancel);
  dex_clear (&self->make_preview);

  g_clear_object (&self->model);
  g_clear_pointer (&self->task_data, query_data_unref);

  g_clear_handle_id (&self->debounce, g_source_remove);

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

  g_clear_pointer (&self->task_data, query_data_unref);

  if (self->cancel != NULL)
    dex_cancellable_cancel (self->cancel);

  text = gtk_editable_get_text (editable);
  if (text != NULL && *text != '\0')
    string = gtk_string_object_new (text);

  data        = query_data_new ();
  data->self  = self;
  data->query = string != NULL ? g_object_ref (string) : NULL;

  self->task_data = query_data_ref (data);
  dex_clear (&self->cancel);
  self->cancel = dex_cancellable_new ();

  dex_clear (&self->task);
  self->task = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      0, (DexFiberFunc) query_fiber,
      query_data_ref (data), query_data_unref);
}

static DexFuture *
make_preview_fiber (SaturnWindow *self)
{
  gpointer        item     = NULL;
  SaturnProvider *provider = NULL;

  item = gtk_single_selection_get_selected_item (self->selection);
  if (item == NULL)
    return dex_future_new_true ();

  provider = g_object_get_qdata (G_OBJECT (item), SATURN_PROVIDER_QUARK);
  if (provider == NULL)
    return dex_future_new_true ();

  saturn_provider_bind_preview (provider, item, self->preview_bin);
  return dex_future_new_true ();
}

static void
debounce_timeout (SaturnWindow *self)
{
  self->debounce = 0;
  dex_clear (&self->make_preview);

  self->make_preview = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      0, (DexFiberFunc) make_preview_fiber,
      self, NULL);
}

static void
selected_changed_cb (SaturnWindow       *self,
                     GParamSpec         *pspec,
                     GtkSingleSelection *selection)
{
  g_clear_handle_id (&self->debounce, g_source_remove);
  dex_clear (&self->make_preview);

  self->debounce = g_timeout_add_once (
      150,
      (GSourceOnceFunc) debounce_timeout,
      self);
}

static void
selected_item_changed_cb (SaturnWindow       *self,
                          GParamSpec         *pspec,
                          GtkSingleSelection *selection)
{
  self->explicit_select = TRUE;
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
  saturn_provider_bind_list_item (provider, item, ADW_BIN (bin));
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
  saturn_provider_unbind_list_item (provider, item, ADW_BIN (bin));
}

static void
action_move (GtkWidget  *widget,
             const char *action_name,
             GVariant   *parameter)
{
  SaturnWindow *self         = SATURN_WINDOW (widget);
  guint         selected     = 0;
  guint         n_items      = 0;
  guint         new_selected = 0;

  selected = gtk_single_selection_get_selected (self->selection);
  n_items  = g_list_model_get_n_items (G_LIST_MODEL (self->selection));

  if (selected == GTK_INVALID_LIST_POSITION)
    new_selected = 0;
  else
    {
      int offset = 0;

      offset = g_variant_get_int32 (parameter);
      if (offset < 0 && ABS (offset) > selected)
        new_selected = n_items + (offset % -(int) n_items);
      else
        new_selected = (selected + offset) % n_items;
    }

  gtk_list_view_scroll_to (
      self->list_view,
      new_selected,
      GTK_LIST_SCROLL_SELECT,
      NULL);
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
  gtk_widget_class_bind_template_child (widget_class, SaturnWindow, status_label);
  gtk_widget_class_bind_template_child (widget_class, SaturnWindow, selection);
  gtk_widget_class_bind_template_child (widget_class, SaturnWindow, list_view);
  gtk_widget_class_bind_template_child (widget_class, SaturnWindow, preview_bin);
  gtk_widget_class_bind_template_callback (widget_class, text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, selected_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, selected_item_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_setup_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_teardown_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_bind_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_unbind_cb);
  gtk_widget_class_install_action (widget_class, "move", "i", action_move);
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
  SaturnWindow *self             = data->self;
  g_autoptr (GError) local_error = NULL;
  guint n_providers              = 0;
  g_autoptr (GPtrArray) channels = NULL;

  g_list_store_remove_all (self->model);
  self->explicit_select = FALSE;

  if (data->query == NULL)
    {
      gtk_label_set_label (self->status_label, _ ("Waiting"));
      return dex_future_new_true ();
    }

  n_providers = g_list_model_get_n_items (self->providers);
  channels    = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_providers; i++)
    {
      g_autoptr (SaturnProvider) provider = NULL;
      g_autoptr (DexChannel) channel      = NULL;

      provider = g_list_model_get_item (self->providers, i);
      channel  = saturn_provider_query (provider, data->query);

      if (channel != NULL)
        g_ptr_array_add (channels, dex_ref (channel));
    }

  for (;;)
    {
      g_autoptr (GPtrArray) futures = NULL;
      g_autoptr (GObject) object    = NULL;

      futures = g_ptr_array_new_with_free_func (dex_unref);
      g_ptr_array_add (futures, dex_ref (self->cancel));

      for (guint i = 0; i < channels->len; i++)
        {
          DexChannel *channel = NULL;

          channel = g_ptr_array_index (channels, i);
          g_ptr_array_add (futures, dex_channel_receive (channel));
        }

      object = dex_await_object (
          dex_future_anyv (
              (DexFuture *const *) futures->pdata,
              futures->len),
          NULL);
      if (data != self->task_data)
        break;

      if (object != NULL)
        {
          guint            n_items     = 0;
          g_autofree char *status_text = NULL;

          g_signal_handlers_block_by_func (
              self->selection,
              selected_item_changed_cb,
              self);

          g_list_store_insert_sorted (
              self->model,
              object,
              (GCompareDataFunc) cmp_item,
              data->query);

          if (!self->explicit_select)
            gtk_list_view_scroll_to (
                self->list_view,
                0,
                GTK_LIST_SCROLL_SELECT,
                NULL);

          g_signal_handlers_unblock_by_func (
              self->selection,
              selected_item_changed_cb,
              self);

          n_items     = g_list_model_get_n_items (G_LIST_MODEL (self->model));
          status_text = g_strdup_printf (_ ("%'d"), n_items);
          gtk_label_set_label (self->status_label, status_text);
        }
      else
        break;

      dex_await (dex_timeout_new_usec (1), NULL);
      if (data != self->task_data)
        break;
    }

  for (guint i = 0; i < channels->len; i++)
    {
      DexChannel *channel = NULL;

      channel = g_ptr_array_index (channels, i);
      dex_channel_close_receive (channel);
    }

  return dex_future_new_true ();
}

static gint
cmp_item (GObject *a,
          GObject *b,
          GObject *query)
{
  gsize a_score = 0;
  gsize b_score = 0;

  a_score = GPOINTER_TO_SIZE (g_object_get_qdata (a, SATURN_PROVIDER_SCORE_QUARK));
  b_score = GPOINTER_TO_SIZE (g_object_get_qdata (b, SATURN_PROVIDER_SCORE_QUARK));

  /* TODO: if same provider, have a special cmp impl func? */

  if (a_score == 0)
    {
      SaturnProvider *provider = NULL;

      provider = g_object_get_qdata (a, SATURN_PROVIDER_QUARK);
      a_score  = saturn_provider_score (provider, a, query);
    }
  if (b_score == 0)
    {
      SaturnProvider *provider = NULL;

      provider = g_object_get_qdata (b, SATURN_PROVIDER_QUARK);
      b_score  = saturn_provider_score (provider, b, query);
    }

  return a_score > b_score ? -1 : 1;
}
