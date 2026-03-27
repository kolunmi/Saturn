/* saturn-cl-completion-provider.c
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

#include "saturn-cl-completion-provider.h"
#include "saturn-cl-completion-proposal.h"
#include "util.h"

struct _SaturnClCompletionProvider
{
  GObject parent_instance;

  GListModel *model;
  int         priority;
  char       *title;

  GPtrArray *latest_snapshot;
};

static void
completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SaturnClCompletionProvider,
    saturn_cl_completion_provider,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                           completion_provider_iface_init));

enum
{
  PROP_0,

  PROP_MODEL,
  PROP_PRIORITY,
  PROP_TITLE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static GPtrArray *
make_model_snapshot (GListModel *model);

static GListModel *
filter (const char *query,
        GPtrArray  *proposals,
        GListStore *edit);

static gint
cmp_proposal (SaturnClCompletionProposal *proposal_a,
              SaturnClCompletionProposal *proposal_b,
              GHashTable                 *scores);

SATURN_DEFINE_DATA (
    populate_async,
    PopulateAsync,
    {
      char      *query;
      GPtrArray *snapshot;
    },
    SATURN_RELEASE_DATA (query, g_free);
    SATURN_RELEASE_DATA (snapshot, g_ptr_array_unref));

static void
populate_async_thread (GTask                      *task,
                       SaturnClCompletionProvider *self,
                       PopulateAsyncData          *data,
                       GCancellable               *cancellable);

static void
saturn_cl_completion_provider_dispose (GObject *object)
{
  SaturnClCompletionProvider *self = SATURN_CL_COMPLETION_PROVIDER (object);

  g_clear_pointer (&self->model, g_object_unref);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (saturn_cl_completion_provider_parent_class)->dispose (object);
}

static void
saturn_cl_completion_provider_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  SaturnClCompletionProvider *self = SATURN_CL_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, saturn_cl_completion_provider_get_model (self));
      break;
    case PROP_PRIORITY:
      g_value_set_int (value, saturn_cl_completion_provider_get_priority (self));
      break;
    case PROP_TITLE:
      g_value_set_string (value, saturn_cl_completion_provider_get_title (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_cl_completion_provider_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  SaturnClCompletionProvider *self = SATURN_CL_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      saturn_cl_completion_provider_set_model (self, g_value_get_object (value));
      break;
    case PROP_PRIORITY:
      saturn_cl_completion_provider_set_priority (self, g_value_get_int (value));
      break;
    case PROP_TITLE:
      saturn_cl_completion_provider_set_title (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_cl_completion_provider_class_init (SaturnClCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = saturn_cl_completion_provider_set_property;
  object_class->get_property = saturn_cl_completion_provider_get_property;
  object_class->dispose      = saturn_cl_completion_provider_dispose;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_OBJECT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PRIORITY] =
      g_param_spec_int (
          "priority",
          NULL, NULL,
          G_MININT, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TITLE] =
      g_param_spec_string (
          "title",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static char *
completion_provider_get_title (GtkSourceCompletionProvider *provider)
{
  SaturnClCompletionProvider *self = SATURN_CL_COMPLETION_PROVIDER (provider);

  if (self->title != NULL)
    return g_strdup (self->title);
  else
    return NULL;
}

static int
completion_provider_get_priority (GtkSourceCompletionProvider *provider,
                                  GtkSourceCompletionContext  *context)
{
  SaturnClCompletionProvider *self = SATURN_CL_COMPLETION_PROVIDER (provider);
  return self->priority;
}

static gboolean
completion_provider_is_trigger (GtkSourceCompletionProvider *provider,
                                const GtkTextIter           *iter,
                                gunichar                     ch)
{
  SaturnClCompletionProvider *self = SATURN_CL_COMPLETION_PROVIDER (provider);

  if (self->model == NULL)
    return FALSE;
  if (g_unichar_isspace (ch))
    return FALSE;

  if (self->latest_snapshot == NULL)
    self->latest_snapshot = make_model_snapshot (self->model);

  /* check if there is at least one leading char */
  for (guint i = 0; i < self->latest_snapshot->len; i++)
    {
      SaturnClCompletionProposal *proposal   = NULL;
      const char                 *string     = NULL;
      gunichar                    leading_ch = 0;

      proposal   = g_ptr_array_index (self->latest_snapshot, i);
      string     = saturn_cl_completion_proposal_get_string (proposal);
      leading_ch = g_utf8_get_char (string);

      if (ch == leading_ch)
        return TRUE;
    }

  return FALSE;
}

static gboolean
completion_provider_key_activates (GtkSourceCompletionProvider *provider,
                                   GtkSourceCompletionContext  *context,
                                   GtkSourceCompletionProposal *proposal,
                                   guint                        keyval,
                                   GdkModifierType              state)
{
  SaturnClCompletionProvider *self = SATURN_CL_COMPLETION_PROVIDER (provider);

  (void) self;

  return state == GDK_ALT_MASK &&
         keyval == GDK_KEY_Tab;
}

static GListModel *
completion_provider_populate (GtkSourceCompletionProvider *provider,
                              GtkSourceCompletionContext  *context,
                              GError                     **error)
{
  SaturnClCompletionProvider *self  = SATURN_CL_COMPLETION_PROVIDER (provider);
  const char                 *query = NULL;

  if (self->model == NULL)
    return NULL;
  query = gtk_source_completion_context_get_word (context);

  if (self->latest_snapshot == NULL)
    self->latest_snapshot = make_model_snapshot (self->model);
  return filter (query, self->latest_snapshot, NULL);
}

static void
completion_provider_populate_async (GtkSourceCompletionProvider *provider,
                                    GtkSourceCompletionContext  *context,
                                    GCancellable                *cancellable,
                                    GAsyncReadyCallback          callback,
                                    gpointer                     user_data)
{
  SaturnClCompletionProvider *self   = SATURN_CL_COMPLETION_PROVIDER (provider);
  const char                 *query  = NULL;
  g_autoptr (PopulateAsyncData) data = NULL;
  g_autoptr (GTask) task             = NULL;

  if (self->model == NULL)
    {
      g_task_report_error (
          provider, callback, user_data, NULL,
          g_error_new (G_IO_ERROR, G_IO_ERROR_UNKNOWN, "no model"));
      return;
    }

  query = gtk_source_completion_context_get_word (context);
  if (self->latest_snapshot == NULL)
    self->latest_snapshot = make_model_snapshot (self->model);

  data           = populate_async_data_new ();
  data->query    = g_strdup (query);
  data->snapshot = g_ptr_array_ref (self->latest_snapshot);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, NULL);
  g_task_set_task_data (task, populate_async_data_ref (data), populate_async_data_unref);
  g_task_set_priority (task, G_PRIORITY_DEFAULT);
  g_task_set_check_cancellable (task, TRUE);
  g_task_run_in_thread (task, (GTaskThreadFunc) populate_async_thread);
}

static GListModel *
completion_provider_populate_finish (GtkSourceCompletionProvider *provider,
                                     GAsyncResult                *result,
                                     GError                     **error)
{
  SaturnClCompletionProvider *self = SATURN_CL_COMPLETION_PROVIDER (provider);

  (void) self;

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
completion_provider_refilter (GtkSourceCompletionProvider *provider,
                              GtkSourceCompletionContext  *context,
                              GListModel                  *model)
{
  SaturnClCompletionProvider *self  = SATURN_CL_COMPLETION_PROVIDER (provider);
  const char                 *query = NULL;
  g_autoptr (GListModel) res_model  = NULL;

  query = gtk_source_completion_context_get_word (context);
  if (self->latest_snapshot == NULL)
    self->latest_snapshot = make_model_snapshot (self->model);
  res_model = filter (query, self->latest_snapshot, NULL);

  gtk_source_completion_context_set_proposals_for_provider (context, provider, res_model);
}

static void
completion_provider_display (GtkSourceCompletionProvider *provider,
                             GtkSourceCompletionContext  *context,
                             GtkSourceCompletionProposal *proposal,
                             GtkSourceCompletionCell     *cell)
{
  SaturnClCompletionProvider    *self        = SATURN_CL_COMPLETION_PROVIDER (provider);
  SaturnClCompletionProposal    *cl_proposal = SATURN_CL_COMPLETION_PROPOSAL (proposal);
  GtkSourceCompletionColumn      column      = 0;
  SaturnClCompletionProposalKind kind        = 0;
  const char                    *string      = 0;

  (void) self;

  kind   = saturn_cl_completion_proposal_get_kind (cl_proposal);
  string = saturn_cl_completion_proposal_get_string (cl_proposal);

  column = gtk_source_completion_cell_get_column (cell);
  switch (column)
    {
    case GTK_SOURCE_COMPLETION_COLUMN_ICON:
      gtk_source_completion_cell_set_icon_name (cell, "insert-text-symbolic");
      break;
    case GTK_SOURCE_COMPLETION_COLUMN_BEFORE:
      {
        g_autoptr (GEnumClass) enum_class = NULL;
        GEnumValue *enum_value            = NULL;

        enum_class = g_type_class_ref (SATURN_TYPE_CL_COMPLETION_PROPOSAL_KIND);
        enum_value = g_enum_get_value (enum_class, kind);
        gtk_source_completion_cell_set_text (cell, enum_value->value_nick);
        gtk_widget_set_margin_end (GTK_WIDGET (cell), 10);
        gtk_widget_add_css_class (GTK_WIDGET (cell), "accent");
      }
      break;
    case GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT:
      gtk_source_completion_cell_set_text (cell, NULL);
      break;
    case GTK_SOURCE_COMPLETION_COLUMN_AFTER:
      gtk_source_completion_cell_set_text (cell, string);
      gtk_widget_add_css_class (GTK_WIDGET (cell), "monospace");
      break;
    case GTK_SOURCE_COMPLETION_COLUMN_COMMENT:
      gtk_source_completion_cell_set_text (cell, NULL);
      break;
    case GTK_SOURCE_COMPLETION_COLUMN_DETAILS:
      gtk_source_completion_cell_set_text (cell, NULL);
      break;
    default:
      break;
    }
}

static void
completion_provider_activate (GtkSourceCompletionProvider *provider,
                              GtkSourceCompletionContext  *context,
                              GtkSourceCompletionProposal *proposal)
{
  SaturnClCompletionProvider *self        = SATURN_CL_COMPLETION_PROVIDER (provider);
  SaturnClCompletionProposal *cl_proposal = SATURN_CL_COMPLETION_PROPOSAL (proposal);
  gboolean                    result      = FALSE;
  GtkSourceBuffer            *buffer      = NULL;
  GtkTextIter                 begin       = { 0 };
  GtkTextIter                 end         = { 0 };
  const char                 *string      = NULL;

  (void) self;

  buffer = gtk_source_completion_context_get_buffer (context);
  result = gtk_source_completion_context_get_bounds (context, &begin, &end);
  if (!result)
    return;

  string = saturn_cl_completion_proposal_get_string (cl_proposal);

  gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
  gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, string, -1);
}

static GPtrArray *
completion_provider_list_alternates (GtkSourceCompletionProvider *provider,
                                     GtkSourceCompletionContext  *context,
                                     GtkSourceCompletionProposal *proposal)
{
  SaturnClCompletionProvider *self = SATURN_CL_COMPLETION_PROVIDER (provider);

  (void) self;

  return NULL;
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
  iface->get_title       = completion_provider_get_title;
  iface->get_priority    = completion_provider_get_priority;
  iface->is_trigger      = completion_provider_is_trigger;
  iface->key_activates   = completion_provider_key_activates;
  iface->populate        = completion_provider_populate;
  iface->populate_async  = completion_provider_populate_async;
  iface->populate_finish = completion_provider_populate_finish;
  iface->refilter        = completion_provider_refilter;
  iface->display         = completion_provider_display;
  iface->activate        = completion_provider_activate;
  iface->list_alternates = completion_provider_list_alternates;
}

static void
saturn_cl_completion_provider_init (SaturnClCompletionProvider *self)
{
}

SaturnClCompletionProvider *
saturn_cl_completion_provider_new (void)
{
  return g_object_new (SATURN_TYPE_CL_COMPLETION_PROVIDER, NULL);
}

GListModel *
saturn_cl_completion_provider_get_model (SaturnClCompletionProvider *self)
{
  g_return_val_if_fail (SATURN_IS_CL_COMPLETION_PROVIDER (self), NULL);
  return self->model;
}

int
saturn_cl_completion_provider_get_priority (SaturnClCompletionProvider *self)
{
  g_return_val_if_fail (SATURN_IS_CL_COMPLETION_PROVIDER (self), 0);
  return self->priority;
}

const char *
saturn_cl_completion_provider_get_title (SaturnClCompletionProvider *self)
{
  g_return_val_if_fail (SATURN_IS_CL_COMPLETION_PROVIDER (self), NULL);
  return self->title;
}

void
saturn_cl_completion_provider_set_model (SaturnClCompletionProvider *self,
                                         GListModel                 *model)
{
  g_return_if_fail (SATURN_IS_CL_COMPLETION_PROVIDER (self));

  if (model == self->model)
    return;

  g_clear_pointer (&self->model, g_object_unref);
  if (model != NULL)
    self->model = g_object_ref (model);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

void
saturn_cl_completion_provider_set_priority (SaturnClCompletionProvider *self,
                                            int                         priority)
{
  g_return_if_fail (SATURN_IS_CL_COMPLETION_PROVIDER (self));

  if (priority == self->priority)
    return;

  self->priority = priority;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PRIORITY]);
}

void
saturn_cl_completion_provider_set_title (SaturnClCompletionProvider *self,
                                         const char                 *title)
{
  g_return_if_fail (SATURN_IS_CL_COMPLETION_PROVIDER (self));

  if (title == self->title || (title != NULL && self->title != NULL && g_strcmp0 (title, self->title) == 0))
    return;

  g_clear_pointer (&self->title, g_free);
  if (title != NULL)
    self->title = g_strdup (title);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TITLE]);
}

void
saturn_cl_completion_provider_set_title_take (SaturnClCompletionProvider *self,
                                              char                       *title)
{
  g_return_if_fail (SATURN_IS_CL_COMPLETION_PROVIDER (self));

  if (title != NULL && self->title != NULL && g_strcmp0 (title, self->title) == 0)
    {
      g_free (title);
      return;
    }

  g_clear_pointer (&self->title, g_free);
  if (title != NULL)
    self->title = title;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TITLE]);
}

static GPtrArray *
make_model_snapshot (GListModel *model)
{
  g_autoptr (GPtrArray) parray = NULL;
  guint n_items                = 0;

  parray = g_ptr_array_new_with_free_func (g_object_unref);

  n_items = g_list_model_get_n_items (model);
  g_ptr_array_set_size (parray, n_items);

  for (guint i = 0; i < n_items; i++)
    {
      g_ptr_array_index (parray, i) =
          g_list_model_get_item (model, i);
    }

  return g_steal_pointer (&parray);
}

static GListModel *
filter (const char *query,
        GPtrArray  *proposals,
        GListStore *edit)
{
  g_autofree char *folded       = NULL;
  g_autoptr (GHashTable) scores = NULL;
  g_autoptr (GListStore) store  = NULL;

  folded = g_utf8_casefold (query, -1);
  scores = g_hash_table_new (g_direct_hash, g_direct_equal);

  if (edit == NULL)
    store = g_list_store_new (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL);

  for (guint i = 0; i < proposals->len; i++)
    {
      SaturnClCompletionProposal *proposal = NULL;
      const char                 *string   = NULL;
      guint                       score    = 0;
      gboolean                    match    = FALSE;

      proposal = g_ptr_array_index (proposals, i);
      string   = saturn_cl_completion_proposal_get_string (proposal);

      match = gtk_source_completion_fuzzy_match (string, folded, &score);
      if (match)
        {
          g_hash_table_replace (scores, proposal, GUINT_TO_POINTER (score));
          if (edit == NULL)
            g_list_store_insert_sorted (store, proposal, (GCompareDataFunc) cmp_proposal, scores);
        }
    }

  if (edit != NULL)
    {
      guint n_items = 0;

      for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (edit));)
        {
          g_autoptr (SaturnClCompletionProposal) proposal = NULL;

          proposal = g_list_model_get_item (G_LIST_MODEL (edit), i);
          if (g_hash_table_contains (scores, proposal))
            i++;
          else
            g_list_store_remove (edit, i);
        }

      n_items = g_list_model_get_n_items (G_LIST_MODEL (edit));
      if (n_items > 0)
        g_list_store_sort (edit, (GCompareDataFunc) cmp_proposal, scores);
      return G_LIST_MODEL (edit);
    }
  else
    return (GListModel *) g_steal_pointer (&store);
}

static gint
cmp_proposal (SaturnClCompletionProposal *proposal_a,
              SaturnClCompletionProposal *proposal_b,
              GHashTable                 *scores)
{
  guint score_a = 0;
  guint score_b = 0;

  score_a = GPOINTER_TO_UINT (g_hash_table_lookup (scores, proposal_a));
  score_b = GPOINTER_TO_UINT (g_hash_table_lookup (scores, proposal_b));

  if (score_b > score_a)
    return -1;
  else
    return 1;
}

static void
populate_async_thread (GTask                      *task,
                       SaturnClCompletionProvider *self,
                       PopulateAsyncData          *data,
                       GCancellable               *cancellable)
{
  if (g_task_return_error_if_cancelled (task))
    return;
  g_task_return_pointer (task, filter (data->query, data->snapshot, NULL), g_object_unref);
}

/* End of saturn-cl-completion-provider.c */
