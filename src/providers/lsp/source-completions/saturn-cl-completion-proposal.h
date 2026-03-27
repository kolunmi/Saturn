/* saturn-cl-completion-proposal.h
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

#pragma once

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

typedef enum
{
  SATURN_CL_COMPLETION_PROPOSAL_KIND_NORMAL,
  SATURN_CL_COMPLETION_PROPOSAL_KIND_PACKAGE,
  SATURN_CL_COMPLETION_PROPOSAL_KIND_FUNCTION,
  SATURN_CL_COMPLETION_PROPOSAL_KIND_MACRO,
} SaturnClCompletionProposalKind;
GType saturn_cl_completion_proposal_kind_get_type (void);
#define SATURN_TYPE_CL_COMPLETION_PROPOSAL_KIND (saturn_cl_completion_proposal_kind_get_type ())

#define SATURN_TYPE_CL_COMPLETION_PROPOSAL (saturn_cl_completion_proposal_get_type ())
G_DECLARE_FINAL_TYPE (SaturnClCompletionProposal, saturn_cl_completion_proposal, SATURN, CL_COMPLETION_PROPOSAL, GObject)

SaturnClCompletionProposal *
saturn_cl_completion_proposal_new (void);

SaturnClCompletionProposalKind
saturn_cl_completion_proposal_get_kind (SaturnClCompletionProposal *self);

const char *
saturn_cl_completion_proposal_get_string (SaturnClCompletionProposal *self);

void
saturn_cl_completion_proposal_set_kind (SaturnClCompletionProposal    *self,
                                        SaturnClCompletionProposalKind kind);

void
saturn_cl_completion_proposal_set_string (SaturnClCompletionProposal *self,
                                          const char                 *string);

void
saturn_cl_completion_proposal_set_string_take (SaturnClCompletionProposal *self,
                                               char                       *string);

#define saturn_cl_completion_proposal_set_string_take_printf(self, ...) saturn_cl_completion_proposal_set_string_take (self, g_strdup_printf (__VA_ARGS__))

G_END_DECLS

/* End of saturn-cl-completion-proposal.h */
