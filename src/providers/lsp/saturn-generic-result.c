/* saturn-generic-result.c
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

#include "saturn-generic-result.h"

typedef struct
{
  GObject parent_instance;

  GObject *obj0;
  GObject *obj1;
  GObject *obj2;
  GObject *obj3;
} SaturnGenericResultPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (SaturnGenericResult, saturn_generic_result, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_OBJ0,
  PROP_OBJ1,
  PROP_OBJ2,
  PROP_OBJ3,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
saturn_generic_result_dispose (GObject *object)
{
  SaturnGenericResult        *self = SATURN_GENERIC_RESULT (object);
  SaturnGenericResultPrivate *priv = saturn_generic_result_get_instance_private (self);

  g_clear_pointer (&priv->obj0, g_object_unref);
  g_clear_pointer (&priv->obj1, g_object_unref);
  g_clear_pointer (&priv->obj2, g_object_unref);
  g_clear_pointer (&priv->obj3, g_object_unref);

  G_OBJECT_CLASS (saturn_generic_result_parent_class)->dispose (object);
}

static void
saturn_generic_result_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  SaturnGenericResult *self = SATURN_GENERIC_RESULT (object);

  switch (prop_id)
    {
    case PROP_OBJ0:
      g_value_set_object (value, saturn_generic_result_get_obj0 (self));
      break;
    case PROP_OBJ1:
      g_value_set_object (value, saturn_generic_result_get_obj1 (self));
      break;
    case PROP_OBJ2:
      g_value_set_object (value, saturn_generic_result_get_obj2 (self));
      break;
    case PROP_OBJ3:
      g_value_set_object (value, saturn_generic_result_get_obj3 (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_generic_result_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  SaturnGenericResult *self = SATURN_GENERIC_RESULT (object);

  switch (prop_id)
    {
    case PROP_OBJ0:
      saturn_generic_result_set_obj0 (self, g_value_get_object (value));
      break;
    case PROP_OBJ1:
      saturn_generic_result_set_obj1 (self, g_value_get_object (value));
      break;
    case PROP_OBJ2:
      saturn_generic_result_set_obj2 (self, g_value_get_object (value));
      break;
    case PROP_OBJ3:
      saturn_generic_result_set_obj3 (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_generic_result_class_init (SaturnGenericResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = saturn_generic_result_set_property;
  object_class->get_property = saturn_generic_result_get_property;
  object_class->dispose      = saturn_generic_result_dispose;

  props[PROP_OBJ0] =
      g_param_spec_object (
          "obj0",
          NULL, NULL,
          G_TYPE_OBJECT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_OBJ1] =
      g_param_spec_object (
          "obj1",
          NULL, NULL,
          G_TYPE_OBJECT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_OBJ2] =
      g_param_spec_object (
          "obj2",
          NULL, NULL,
          G_TYPE_OBJECT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_OBJ3] =
      g_param_spec_object (
          "obj3",
          NULL, NULL,
          G_TYPE_OBJECT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
saturn_generic_result_init (SaturnGenericResult *self)
{
}

SaturnGenericResult *
saturn_generic_result_new (void)
{
  return g_object_new (SATURN_TYPE_GENERIC_RESULT, NULL);
}

GObject *
saturn_generic_result_get_obj0 (SaturnGenericResult *self)
{
  SaturnGenericResultPrivate *priv = NULL;

  g_return_val_if_fail (SATURN_IS_GENERIC_RESULT (self), NULL);

  priv = saturn_generic_result_get_instance_private (self);
  return priv->obj0;
}

GObject *
saturn_generic_result_get_obj1 (SaturnGenericResult *self)
{
  SaturnGenericResultPrivate *priv = NULL;

  g_return_val_if_fail (SATURN_IS_GENERIC_RESULT (self), NULL);

  priv = saturn_generic_result_get_instance_private (self);
  return priv->obj1;
}

GObject *
saturn_generic_result_get_obj2 (SaturnGenericResult *self)
{
  SaturnGenericResultPrivate *priv = NULL;

  g_return_val_if_fail (SATURN_IS_GENERIC_RESULT (self), NULL);

  priv = saturn_generic_result_get_instance_private (self);
  return priv->obj2;
}

GObject *
saturn_generic_result_get_obj3 (SaturnGenericResult *self)
{
  SaturnGenericResultPrivate *priv = NULL;

  g_return_val_if_fail (SATURN_IS_GENERIC_RESULT (self), NULL);

  priv = saturn_generic_result_get_instance_private (self);
  return priv->obj3;
}

void
saturn_generic_result_set_obj0 (SaturnGenericResult *self,
                                GObject             *obj0)
{
  SaturnGenericResultPrivate *priv = NULL;

  g_return_if_fail (SATURN_IS_GENERIC_RESULT (self));

  priv = saturn_generic_result_get_instance_private (self);
  if (obj0 == priv->obj0)
    return;

  g_clear_pointer (&priv->obj0, g_object_unref);
  if (obj0 != NULL)
    priv->obj0 = g_object_ref (obj0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_OBJ0]);
}

void
saturn_generic_result_set_obj1 (SaturnGenericResult *self,
                                GObject             *obj1)
{
  SaturnGenericResultPrivate *priv = NULL;

  g_return_if_fail (SATURN_IS_GENERIC_RESULT (self));

  priv = saturn_generic_result_get_instance_private (self);
  if (obj1 == priv->obj1)
    return;

  g_clear_pointer (&priv->obj1, g_object_unref);
  if (obj1 != NULL)
    priv->obj1 = g_object_ref (obj1);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_OBJ1]);
}

void
saturn_generic_result_set_obj2 (SaturnGenericResult *self,
                                GObject             *obj2)
{
  SaturnGenericResultPrivate *priv = NULL;

  g_return_if_fail (SATURN_IS_GENERIC_RESULT (self));

  priv = saturn_generic_result_get_instance_private (self);
  if (obj2 == priv->obj2)
    return;

  g_clear_pointer (&priv->obj2, g_object_unref);
  if (obj2 != NULL)
    priv->obj2 = g_object_ref (obj2);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_OBJ2]);
}

void
saturn_generic_result_set_obj3 (SaturnGenericResult *self,
                                GObject             *obj3)
{
  SaturnGenericResultPrivate *priv = NULL;

  g_return_if_fail (SATURN_IS_GENERIC_RESULT (self));

  priv = saturn_generic_result_get_instance_private (self);
  if (obj3 == priv->obj3)
    return;

  g_clear_pointer (&priv->obj3, g_object_unref);
  if (obj3 != NULL)
    priv->obj3 = g_object_ref (obj3);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_OBJ3]);
}

/* End of saturn-generic-result.c */
