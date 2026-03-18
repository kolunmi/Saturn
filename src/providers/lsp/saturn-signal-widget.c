/* saturn-signal-widget.c
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

/* This is necessary as subclassing widgets seems not to work at the moment with
   cl-cffi-gtk4 */

#include "saturn-signal-widget.h"

typedef struct
{
  GtkWidget parent_instance;

  GtkWidget *child;
  GObject   *item;
} SaturnSignalWidgetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (SaturnSignalWidget, saturn_signal_widget, GTK_TYPE_WIDGET);

enum
{
  PROP_0,

  PROP_CHILD,
  PROP_ITEM,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_SNAPSHOT,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
saturn_signal_widget_dispose (GObject *object)
{
  SaturnSignalWidget        *self = SATURN_SIGNAL_WIDGET (object);
  SaturnSignalWidgetPrivate *priv = saturn_signal_widget_get_instance_private (self);

  g_clear_pointer (&priv->child, gtk_widget_unparent);
  g_clear_pointer (&priv->item, g_object_unref);

  G_OBJECT_CLASS (saturn_signal_widget_parent_class)->dispose (object);
}

static void
saturn_signal_widget_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  SaturnSignalWidget        *self = SATURN_SIGNAL_WIDGET (object);
  SaturnSignalWidgetPrivate *priv = saturn_signal_widget_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, saturn_signal_widget_get_child (self));
      break;
    case PROP_ITEM:
      g_value_set_object (value, priv->item);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_signal_widget_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  SaturnSignalWidget        *self = SATURN_SIGNAL_WIDGET (object);
  SaturnSignalWidgetPrivate *priv = saturn_signal_widget_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CHILD:
      saturn_signal_widget_set_child (self, g_value_get_object (value));
      break;
    case PROP_ITEM:
      g_clear_pointer (&priv->item, g_object_unref);
      priv->item = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_signal_widget_size_allocate (GtkWidget *widget,
                                    int        width,
                                    int        height,
                                    int        baseline)
{
  SaturnSignalWidget        *self = SATURN_SIGNAL_WIDGET (widget);
  SaturnSignalWidgetPrivate *priv = saturn_signal_widget_get_instance_private (self);

  if (priv->child != NULL &&
      gtk_widget_should_layout (priv->child))
    gtk_widget_allocate (priv->child, width, height, baseline, NULL);
  gtk_widget_queue_draw (widget);
}

static void
saturn_signal_widget_snapshot (GtkWidget   *widget,
                               GtkSnapshot *snapshot)
{
  SaturnSignalWidget        *self = SATURN_SIGNAL_WIDGET (widget);
  SaturnSignalWidgetPrivate *priv = saturn_signal_widget_get_instance_private (self);

  g_signal_emit (self, signals[SIGNAL_SNAPSHOT], 0, snapshot);

  if (priv->child != NULL)
    gtk_widget_snapshot_child (GTK_WIDGET (self), priv->child, snapshot);
}

static void
saturn_signal_widget_class_init (SaturnSignalWidgetClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = saturn_signal_widget_set_property;
  object_class->get_property = saturn_signal_widget_get_property;
  object_class->dispose      = saturn_signal_widget_dispose;

  props[PROP_CHILD] =
      g_param_spec_object (
          "child",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ITEM] =
      g_param_spec_object (
          "item",
          NULL, NULL,
          G_TYPE_OBJECT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_SNAPSHOT] =
      g_signal_new (
          "snapshot",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE,
          1,
          GTK_TYPE_SNAPSHOT);
  g_signal_set_va_marshaller (
      signals[SIGNAL_SNAPSHOT],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  widget_class->size_allocate = saturn_signal_widget_size_allocate;
  widget_class->snapshot      = saturn_signal_widget_snapshot;
}

static void
saturn_signal_widget_init (SaturnSignalWidget *self)
{
}

SaturnSignalWidget *
saturn_signal_widget_new (void)
{
  return g_object_new (SATURN_TYPE_SIGNAL_WIDGET, NULL);
}

GtkWidget *
saturn_signal_widget_get_child (SaturnSignalWidget *self)
{
  SaturnSignalWidgetPrivate *priv = NULL;

  g_return_val_if_fail (SATURN_IS_SIGNAL_WIDGET (self), NULL);

  priv = saturn_signal_widget_get_instance_private (self);
  return priv->child;
}

void
saturn_signal_widget_set_child (SaturnSignalWidget *self,
                                GtkWidget          *child)
{
  SaturnSignalWidgetPrivate *priv = NULL;

  g_return_if_fail (SATURN_IS_SIGNAL_WIDGET (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  priv = saturn_signal_widget_get_instance_private (self);
  if (priv->child == child)
    return;

  if (child != NULL)
    g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  g_clear_pointer (&priv->child, gtk_widget_unparent);
  priv->child = child;

  if (child != NULL)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD]);
}

/* End of saturn-signal-widget.c */
