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

static void
start_query (SaturnWindow *self,
             gpointer      search_object);

static DexFuture *
query_then_loop (DexFuture    *future,
                 SaturnWindow *self);

static DexFuture *
timeout_finally (DexFuture    *future,
                 SaturnWindow *self);

static DexFuture *
make_receive_future (SaturnWindow *self);

static void
cancel_query (SaturnWindow *self);

struct _SaturnWindow
{
  AdwApplicationWindow parent_instance;

  GListModel *providers;

  GListStore *model;

  DexFuture *query;
  GPtrArray *channels;
  GPtrArray *futures;
  gpointer   search_object;

  guint      debounce;
  DexFuture *make_preview;

  gboolean explicit_selection;

  DexFuture *select;
  gpointer   selected_item;

  /* Template widgets */
  GtkEditable        *entry;
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

  cancel_query (self);
  dex_clear (&self->make_preview);
  dex_clear (&self->select);
  g_clear_object (&self->selected_item);
  g_clear_object (&self->model);
  g_clear_handle_id (&self->debounce, g_source_remove);

  g_clear_object (&self->providers);

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

  text = gtk_editable_get_text (editable);
  if (text != NULL && *text != '\0')
    string = gtk_string_object_new (text);

  start_query (self, string);
}

static void
text_activated_cb (SaturnWindow *self,
                   GtkEditable  *editable)
{
  gtk_widget_activate_action (
      GTK_WIDGET (self),
      "select-candidate",
      "i", -1);
}

static void
list_view_activated_cb (SaturnWindow *self,
                        guint         position,
                        GtkEditable  *editable)
{
  gtk_widget_activate_action (
      GTK_WIDGET (self),
      "select-candidate",
      "i", position);
}

static DexFuture *
make_preview_fiber (SaturnWindow *self)
{
  guint selected           = 0;
  g_autoptr (GObject) item = NULL;
  SaturnProvider *provider = NULL;

  selected = gtk_single_selection_get_selected (self->selection);
  if (selected == GTK_INVALID_LIST_POSITION)
    {
      dex_clear (&self->select);
      return NULL;
    }

  item     = g_list_model_get_item (G_LIST_MODEL (self->selection), selected);
  provider = g_object_get_qdata (item, SATURN_PROVIDER_QUARK);
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
      50,
      (GSourceOnceFunc) debounce_timeout,
      self);
}

static void
selected_item_changed_cb (SaturnWindow       *self,
                          GParamSpec         *pspec,
                          GtkSingleSelection *selection)
{
  self->explicit_selection = TRUE;
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

static DexFuture *
select_fiber (SaturnWindow *self)
{
  g_autoptr (GError) local_error     = NULL;
  g_autoptr (GObject) item           = NULL;
  SaturnProvider *provider           = NULL;
  const char     *text               = NULL;
  g_autoptr (GtkStringObject) string = NULL;
  gboolean result                    = FALSE;

  item     = g_steal_pointer (&self->selected_item);
  provider = g_object_get_qdata (item, SATURN_PROVIDER_QUARK);
  if (provider == NULL)
    {
      dex_clear (&self->select);
      return dex_future_new_true ();
    }

  text = gtk_editable_get_text (self->entry);
  if (text != NULL && *text != '\0')
    string = gtk_string_object_new (text);

  result = saturn_provider_select (
      provider,
      item,
      G_OBJECT (string),
      &local_error);
  if (!result)
    {
      /* TODO: show `local_error` in UI */

      dex_clear (&self->select);
      return dex_future_new_true ();
    }

  gtk_window_close (GTK_WINDOW (self));
  return dex_future_new_true ();
}

static void
action_select_candidate (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *parameter)
{
  SaturnWindow *self       = SATURN_WINDOW (widget);
  int           selected   = 0;
  g_autoptr (GObject) item = NULL;

  if (self->select != NULL)
    return;

  selected = g_variant_get_int32 (parameter);
  if (selected < 0)
    {
      selected = gtk_single_selection_get_selected (self->selection);
      if (selected == GTK_INVALID_LIST_POSITION)
        return;
    }

  g_clear_object (&self->selected_item);
  self->selected_item = g_list_model_get_item (G_LIST_MODEL (self->selection), selected);

  self->select = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      0, (DexFiberFunc) select_fiber,
      self, NULL);
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
  gtk_widget_class_bind_template_child (widget_class, SaturnWindow, entry);
  gtk_widget_class_bind_template_child (widget_class, SaturnWindow, status_label);
  gtk_widget_class_bind_template_child (widget_class, SaturnWindow, selection);
  gtk_widget_class_bind_template_child (widget_class, SaturnWindow, list_view);
  gtk_widget_class_bind_template_child (widget_class, SaturnWindow, preview_bin);
  gtk_widget_class_bind_template_callback (widget_class, text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, text_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, selected_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, selected_item_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_view_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_setup_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_teardown_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_bind_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_item_unbind_cb);
  gtk_widget_class_install_action (widget_class, "select-candidate", "i", action_select_candidate);
  gtk_widget_class_install_action (widget_class, "move", "i", action_move);
}

static void
saturn_window_init (SaturnWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->model = g_list_store_new (G_TYPE_OBJECT);
  gtk_single_selection_set_model (self->selection, G_LIST_MODEL (self->model));

  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
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

static void
start_query (SaturnWindow *self,
             gpointer      search_object)
{
  guint n_providers = 0;

  cancel_query (self);

  g_list_store_remove_all (self->model);
  self->explicit_selection = FALSE;

  if (search_object == NULL)
    {
      gtk_label_set_label (self->status_label, _ ("Waiting"));
      return;
    }

  self->channels      = g_ptr_array_new_with_free_func (dex_unref);
  self->search_object = g_object_ref (search_object);

  n_providers = g_list_model_get_n_items (self->providers);
  for (guint i = 0; i < n_providers; i++)
    {
      g_autoptr (SaturnProvider) provider = NULL;
      g_autoptr (DexChannel) channel      = NULL;

      provider = g_list_model_get_item (self->providers, i);
      channel  = saturn_provider_query (provider, search_object);

      if (channel != NULL)
        g_ptr_array_add (self->channels, dex_ref (channel));
    }
  if (self->channels->len == 0)
    {
      cancel_query (self);
      gtk_label_set_label (self->status_label, _ ("Waiting"));
      return;
    }

  self->query = dex_future_then_loop (
      make_receive_future (self),
      (DexFutureCallback) query_then_loop,
      self, NULL);
}

static DexFuture *
query_then_loop (DexFuture    *future,
                 SaturnWindow *self)
{
  guint            n_items     = 0;
  g_autofree char *status_text = NULL;

  g_signal_handlers_block_by_func (
      self->selection,
      selected_item_changed_cb,
      self);

  for (guint i = 0; i < self->futures->len; i++)
    {
      const GValue *value = NULL;

      value = dex_future_get_value (
          g_ptr_array_index (self->futures, i),
          NULL);
      if (value != NULL)
        {
          if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY))
            {
              GPtrArray *array = NULL;

              array = g_value_get_boxed (value);
              for (guint j = 0; j < array->len; j++)
                {
                  gpointer object = NULL;

                  object = g_ptr_array_index (array, j);
                  g_list_store_insert_sorted (
                      self->model,
                      object,
                      (GCompareDataFunc) cmp_item,
                      self->search_object);
                }
            }
          else if (G_VALUE_HOLDS (value, G_TYPE_OBJECT))
            {
              gpointer object = NULL;

              object = g_value_get_object (value);
              g_list_store_insert_sorted (
                  self->model,
                  object,
                  (GCompareDataFunc) cmp_item,
                  self->search_object);
            }
          else
            g_assert_not_reached ();
        }
    }

  if (!self->explicit_selection &&
      g_list_model_get_n_items (G_LIST_MODEL (self->model)) > 0)
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

  return dex_future_finally (
      dex_timeout_new_usec (100),
      (DexFutureCallback) timeout_finally,
      self, NULL);
}

static DexFuture *
timeout_finally (DexFuture    *future,
                 SaturnWindow *self)
{
  return make_receive_future (self);
}

static DexFuture *
make_receive_future (SaturnWindow *self)
{
  g_autoptr (GPtrArray) futures = NULL;
  g_autoptr (DexFuture) ret     = NULL;

  if (self->channels == NULL ||
      self->channels->len == 0)
    return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_CANCELLED, "No provider channels");

  if (self->futures != NULL)
    g_assert (self->futures->len == self->channels->len);

  futures = g_ptr_array_new_with_free_func (dex_unref);
  for (guint i = 0; i < self->channels->len; i++)
    {
      DexFuture *last              = NULL;
      g_autoptr (DexFuture) future = NULL;

      if (self->futures != NULL)
        last = g_ptr_array_index (self->futures, i);
      if (last != NULL &&
          dex_future_is_pending (last))
        future = dex_ref (last);
      else
        {
          DexChannel *channel = NULL;

          channel = g_ptr_array_index (self->channels, i);
          future  = dex_channel_receive (channel);
        }

      g_ptr_array_add (futures, g_steal_pointer (&future));
    }

  ret = dex_future_anyv (
      (DexFuture *const *) futures->pdata,
      futures->len);

  g_clear_pointer (&self->futures, g_ptr_array_unref);
  self->futures = g_steal_pointer (&futures);

  return g_steal_pointer (&ret);
}

static void
cancel_query (SaturnWindow *self)
{
  dex_clear (&self->query);

  if (self->channels != NULL)
    {
      for (guint i = 0; i < self->channels->len; i++)
        {
          DexChannel *channel = NULL;

          channel = g_ptr_array_index (self->channels, i);
          dex_channel_close_receive (channel);
        }
    }
  g_clear_pointer (&self->channels, g_ptr_array_unref);
  g_clear_pointer (&self->futures, g_ptr_array_unref);
  g_clear_object (&self->search_object);
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
