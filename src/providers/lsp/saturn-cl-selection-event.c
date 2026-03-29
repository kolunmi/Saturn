/* saturn-cl-selection-event.c
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

#include "saturn-cl-selection-event.h"

struct _SaturnClSelectionEvent
{
  GObject parent_instance;

  SaturnSelectKind kind;
  char            *selected_text;
};

G_DEFINE_FINAL_TYPE (SaturnClSelectionEvent, saturn_cl_selection_event, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_KIND,
  PROP_SELECTED_TEXT,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
saturn_cl_selection_event_dispose (GObject *object)
{
  SaturnClSelectionEvent *self = SATURN_CL_SELECTION_EVENT (object);

  g_clear_pointer (&self->selected_text, g_free);

  G_OBJECT_CLASS (saturn_cl_selection_event_parent_class)->dispose (object);
}

static void
saturn_cl_selection_event_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  SaturnClSelectionEvent *self = SATURN_CL_SELECTION_EVENT (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_enum (value, saturn_cl_selection_event_get_kind (self));
      break;
    case PROP_SELECTED_TEXT:
      g_value_set_string (value, saturn_cl_selection_event_get_selected_text (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_cl_selection_event_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  SaturnClSelectionEvent *self = SATURN_CL_SELECTION_EVENT (object);

  switch (prop_id)
    {
    case PROP_KIND:
      saturn_cl_selection_event_set_kind (self, g_value_get_enum (value));
      break;
    case PROP_SELECTED_TEXT:
      saturn_cl_selection_event_set_selected_text (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_cl_selection_event_class_init (SaturnClSelectionEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = saturn_cl_selection_event_set_property;
  object_class->get_property = saturn_cl_selection_event_get_property;
  object_class->dispose      = saturn_cl_selection_event_dispose;

  props[PROP_KIND] =
      g_param_spec_enum (
          "kind",
          NULL, NULL,
          SATURN_TYPE_SELECT_KIND, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SELECTED_TEXT] =
      g_param_spec_string (
          "selected-text",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
saturn_cl_selection_event_init (SaturnClSelectionEvent *self)
{
}

SaturnClSelectionEvent *
saturn_cl_selection_event_new (void)
{
  return g_object_new (SATURN_TYPE_CL_SELECTION_EVENT, NULL);
}

SaturnSelectKind
saturn_cl_selection_event_get_kind (SaturnClSelectionEvent *self)
{
  g_return_val_if_fail (SATURN_IS_CL_SELECTION_EVENT (self), 0);
  return self->kind;
}

const char *
saturn_cl_selection_event_get_selected_text (SaturnClSelectionEvent *self)
{
  g_return_val_if_fail (SATURN_IS_CL_SELECTION_EVENT (self), NULL);
  return self->selected_text;
}

void
saturn_cl_selection_event_set_kind (SaturnClSelectionEvent *self,
                                    SaturnSelectKind        kind)
{
  g_return_if_fail (SATURN_IS_CL_SELECTION_EVENT (self));

  if (kind == self->kind)
    return;

  self->kind = kind;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_KIND]);
}

void
saturn_cl_selection_event_set_selected_text (SaturnClSelectionEvent *self,
                                             const char             *selected_text)
{
  g_return_if_fail (SATURN_IS_CL_SELECTION_EVENT (self));

  if (selected_text == self->selected_text || (selected_text != NULL && self->selected_text != NULL && g_strcmp0 (selected_text, self->selected_text) == 0))
    return;

  g_clear_pointer (&self->selected_text, g_free);
  if (selected_text != NULL)
    self->selected_text = g_strdup (selected_text);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED_TEXT]);
}

void
saturn_cl_selection_event_set_selected_text_take (SaturnClSelectionEvent *self,
                                                  char                   *selected_text)
{
  g_return_if_fail (SATURN_IS_CL_SELECTION_EVENT (self));

  if (selected_text != NULL && self->selected_text != NULL && g_strcmp0 (selected_text, self->selected_text) == 0)
    {
      g_free (selected_text);
      return;
    }

  g_clear_pointer (&self->selected_text, g_free);
  if (selected_text != NULL)
    self->selected_text = selected_text;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED_TEXT]);
}

/* End of saturn-cl-selection-event.c */
