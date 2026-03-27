/* saturn-cl-completion-proposal.c
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

#include "saturn-cl-completion-proposal.h"

G_DEFINE_ENUM_TYPE (
    SaturnClCompletionProposalKind,
    saturn_cl_completion_proposal_kind,
    G_DEFINE_ENUM_VALUE (SATURN_CL_COMPLETION_PROPOSAL_KIND_NORMAL, "normal"),
    G_DEFINE_ENUM_VALUE (SATURN_CL_COMPLETION_PROPOSAL_KIND_PACKAGE, "package"),
    G_DEFINE_ENUM_VALUE (SATURN_CL_COMPLETION_PROPOSAL_KIND_FUNCTION, "function"),
    G_DEFINE_ENUM_VALUE (SATURN_CL_COMPLETION_PROPOSAL_KIND_MACRO, "macro"));

struct _SaturnClCompletionProposal
{
  GObject parent_instance;

  SaturnClCompletionProposalKind kind;
  char                          *string;
  GListModel                    *lambda_args;
};

static void
completion_proposal_iface_init (GtkSourceCompletionProposalInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SaturnClCompletionProposal,
    saturn_cl_completion_proposal,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL,
                           completion_proposal_iface_init));

enum
{
  PROP_0,

  PROP_KIND,
  PROP_STRING,
  PROP_LAMBDA_ARGS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
saturn_cl_completion_proposal_dispose (GObject *object)
{
  SaturnClCompletionProposal *self = SATURN_CL_COMPLETION_PROPOSAL (object);

  g_clear_pointer (&self->string, g_free);
  g_clear_object (&self->lambda_args);

  G_OBJECT_CLASS (saturn_cl_completion_proposal_parent_class)->dispose (object);
}

static void
saturn_cl_completion_proposal_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  SaturnClCompletionProposal *self = SATURN_CL_COMPLETION_PROPOSAL (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_enum (value, saturn_cl_completion_proposal_get_kind (self));
      break;
    case PROP_STRING:
      g_value_set_string (value, saturn_cl_completion_proposal_get_string (self));
      break;
    case PROP_LAMBDA_ARGS:
      g_value_set_object (value, saturn_cl_completion_proposal_get_lambda_args (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_cl_completion_proposal_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  SaturnClCompletionProposal *self = SATURN_CL_COMPLETION_PROPOSAL (object);

  switch (prop_id)
    {
    case PROP_KIND:
      saturn_cl_completion_proposal_set_kind (self, g_value_get_enum (value));
      break;
    case PROP_STRING:
      saturn_cl_completion_proposal_set_string (self, g_value_get_string (value));
      break;
    case PROP_LAMBDA_ARGS:
      saturn_cl_completion_proposal_set_lambda_args (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
saturn_cl_completion_proposal_class_init (SaturnClCompletionProposalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = saturn_cl_completion_proposal_set_property;
  object_class->get_property = saturn_cl_completion_proposal_get_property;
  object_class->dispose      = saturn_cl_completion_proposal_dispose;

  props[PROP_KIND] =
      g_param_spec_enum (
          "kind",
          NULL, NULL,
          SATURN_TYPE_CL_COMPLETION_PROPOSAL_KIND, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_STRING] =
      g_param_spec_string (
          "string",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_LAMBDA_ARGS] =
      g_param_spec_object (
          "lambda-args",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static char *
completion_proposal_get_typed_text (GtkSourceCompletionProposal *proposal)
{
  return NULL;
}

static void
completion_proposal_iface_init (GtkSourceCompletionProposalInterface *iface)
{
  iface->get_typed_text = completion_proposal_get_typed_text;
}

static void
saturn_cl_completion_proposal_init (SaturnClCompletionProposal *self)
{
}

SaturnClCompletionProposal *
saturn_cl_completion_proposal_new (void)
{
  return g_object_new (SATURN_TYPE_CL_COMPLETION_PROPOSAL, NULL);
}

SaturnClCompletionProposalKind
saturn_cl_completion_proposal_get_kind (SaturnClCompletionProposal *self)
{
  g_return_val_if_fail (SATURN_IS_CL_COMPLETION_PROPOSAL (self), 0);
  return self->kind;
}

const char *
saturn_cl_completion_proposal_get_string (SaturnClCompletionProposal *self)
{
  g_return_val_if_fail (SATURN_IS_CL_COMPLETION_PROPOSAL (self), NULL);
  return self->string;
}

GListModel *
saturn_cl_completion_proposal_get_lambda_args (SaturnClCompletionProposal *self)
{
  g_return_val_if_fail (SATURN_IS_CL_COMPLETION_PROPOSAL (self), NULL);
  return self->lambda_args;
}

void
saturn_cl_completion_proposal_set_kind (SaturnClCompletionProposal    *self,
                                        SaturnClCompletionProposalKind kind)
{
  g_return_if_fail (SATURN_IS_CL_COMPLETION_PROPOSAL (self));

  if (kind == self->kind)
    return;

  self->kind = kind;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_KIND]);
}

void
saturn_cl_completion_proposal_set_string (SaturnClCompletionProposal *self,
                                          const char                 *string)
{
  g_return_if_fail (SATURN_IS_CL_COMPLETION_PROPOSAL (self));

  if (string == self->string || (string != NULL && self->string != NULL && g_strcmp0 (string, self->string) == 0))
    return;

  g_clear_pointer (&self->string, g_free);
  if (string != NULL)
    self->string = g_strdup (string);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STRING]);
}

void
saturn_cl_completion_proposal_set_string_take (SaturnClCompletionProposal *self,
                                               char                       *string)
{
  g_return_if_fail (SATURN_IS_CL_COMPLETION_PROPOSAL (self));

  if (string != NULL && self->string != NULL && g_strcmp0 (string, self->string) == 0)
    {
      g_free (string);
      return;
    }

  g_clear_pointer (&self->string, g_free);
  if (string != NULL)
    self->string = string;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STRING]);
}

void
saturn_cl_completion_proposal_set_lambda_args (SaturnClCompletionProposal *self,
                                               GListModel                 *lambda_args)
{
  g_return_if_fail (SATURN_IS_CL_COMPLETION_PROPOSAL (self));

  if (lambda_args == self->lambda_args)
    return;

  g_clear_pointer (&self->lambda_args, g_object_unref);
  if (lambda_args != NULL)
    self->lambda_args = g_object_ref (lambda_args);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LAMBDA_ARGS]);
}

/* End of saturn-cl-completion-proposal.c */
