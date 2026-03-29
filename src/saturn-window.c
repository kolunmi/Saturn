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

#include "saturn-provider.h"
#include "saturn-threadsafe-list-store.h"
#include "saturn-window.h"

static void
start_query (SaturnWindow *self,
             gpointer      search_object);

static void
try_string_query (SaturnWindow *self);

struct _SaturnWindow
{
  AdwApplicationWindow parent_instance;

  GListModel *providers;

  gboolean                   initializing;
  SaturnThreadsafeListStore *model;

  guint debounce;
  /* if less than 0, explicit selection is active */
  int      explicit_selection;
  gpointer selected_item;

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

  PROP_INITIALIZING,
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
    case PROP_INITIALIZING:
      g_value_set_boolean (value, self->initializing);
      break;
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
    case PROP_INITIALIZING:
      self->initializing = g_value_get_boolean (value);
      if (!self->initializing)
        try_string_query (self);
      break;
    case PROP_PROVIDERS:
      saturn_window_set_providers (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
not (gpointer object,
     gboolean value)
{
  return !value;
}

static void
text_changed_cb (SaturnWindow *self,
                 GtkEditable  *editable)
{
  try_string_query (self);
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

static void
debounce_timeout (SaturnWindow *self)
{
  guint selected           = 0;
  g_autoptr (GObject) item = NULL;
  SaturnProvider *provider = NULL;

  self->debounce = 0;

  selected = gtk_single_selection_get_selected (self->selection);
  if (selected == GTK_INVALID_LIST_POSITION)
    {
      adw_bin_set_child (self->preview_bin, NULL);
      return;
    }

  item     = g_list_model_get_item (G_LIST_MODEL (self->selection), selected);
  provider = g_object_get_qdata (item, SATURN_PROVIDER_QUARK);
  if (provider == NULL)
    {
      adw_bin_set_child (self->preview_bin, NULL);
      return;
    }

  saturn_provider_bind_preview (provider, item, self->preview_bin);
}

static void
selection_changed_cb (SaturnWindow *self,
                      guint         position,
                      guint         removed,
                      guint         added,
                      GListModel   *model)
{
  guint n_items  = 0;
  guint selected = 0;
  char  buf[64]  = { 0 };

  n_items  = g_list_model_get_n_items (model);
  selected = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model));
  if (n_items > 0 &&
      self->explicit_selection >= 0 &&
      selected > 0)
    {
      self->explicit_selection++;
      gtk_list_view_scroll_to (self->list_view, 0, GTK_LIST_SCROLL_SELECT, NULL);
    }

  g_snprintf (buf, sizeof (buf), "%u", n_items);
  gtk_label_set_label (self->status_label, buf);
}

static void
selected_changed_cb (SaturnWindow       *self,
                     GParamSpec         *pspec,
                     GtkSingleSelection *selection)
{
  g_clear_handle_id (&self->debounce, g_source_remove);
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
  if (self->explicit_selection >= 0)
    self->explicit_selection--;
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
action_select_candidate (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *parameter)
{
  SaturnWindow *self                 = SATURN_WINDOW (widget);
  g_autoptr (GError) local_error     = NULL;
  int selected                       = 0;
  g_autoptr (GObject) item           = NULL;
  SaturnProvider *provider           = NULL;
  const char     *text               = NULL;
  g_autoptr (GtkStringObject) string = NULL;
  g_autofree char *selected_text     = FALSE;
  SaturnSelectKind select_kind       = SATURN_SELECT_KIND_NONE;

  selected = g_variant_get_int32 (parameter);
  if (selected < 0)
    {
      selected = gtk_single_selection_get_selected (self->selection);
      if (selected == GTK_INVALID_LIST_POSITION)
        return;
    }

  g_clear_object (&self->selected_item);
  self->selected_item = g_list_model_get_item (G_LIST_MODEL (self->selection), selected);

  item     = g_steal_pointer (&self->selected_item);
  provider = g_object_get_qdata (item, SATURN_PROVIDER_QUARK);
  if (provider == NULL)
    return;

  text        = gtk_editable_get_text (self->entry);
  string      = gtk_string_object_new (text);
  select_kind = saturn_provider_select (
      provider,
      item,
      G_OBJECT (string),
      &selected_text,
      &local_error);
  if (select_kind == SATURN_SELECT_KIND_NONE)
    return;

  if (selected_text == NULL)
    {
      g_warning ("provider returned non %s select kind "
                 "without providing selected text",
                 g_enum_to_string (SATURN_TYPE_SELECT_KIND, SATURN_SELECT_KIND_NONE));
      return;
    }

  switch (select_kind)
    {
    case SATURN_SELECT_KIND_CLOSE:
      g_object_set (
          g_application_get_default (),
          "selected-text", selected_text,
          NULL);
      gtk_window_close (GTK_WINDOW (self));
      break;
    case SATURN_SELECT_KIND_SUBSTITUTE:
      gtk_editable_set_text (self->entry, selected_text);
      gtk_editable_set_position (self->entry, -1);
      break;
    case SATURN_SELECT_KIND_NONE:
    default:
      break;
    }
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

  props[PROP_INITIALIZING] =
      g_param_spec_boolean (
          "initializing",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

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
  gtk_widget_class_bind_template_callback (widget_class, not);
  gtk_widget_class_bind_template_callback (widget_class, text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, text_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, selection_changed_cb);
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

  if (self->model != NULL)
    saturn_threadsafe_list_store_cancel (self->model);
  g_clear_object (&self->model);
  gtk_single_selection_set_model (self->selection, NULL);
  self->explicit_selection = 1;
  if (search_object == NULL)
    return;

  self->model = saturn_threadsafe_list_store_new (
      (GCompareDataFunc) cmp_item, g_object_ref (search_object), g_object_unref);
  gtk_single_selection_set_model (self->selection, G_LIST_MODEL (self->model));

  n_providers = g_list_model_get_n_items (self->providers);
  for (guint i = 0; i < n_providers; i++)
    {
      g_autoptr (SaturnProvider) provider = NULL;

      provider = g_list_model_get_item (self->providers, i);
      saturn_provider_query (provider, search_object, self->model);
    }
}

static void
try_string_query (SaturnWindow *self)
{
  const char *text                   = NULL;
  g_autoptr (GtkStringObject) string = NULL;

  if (self->initializing)
    return;

  text   = gtk_editable_get_text (self->entry);
  string = gtk_string_object_new (text);
  start_query (self, string);
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
