/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BM Inline functions.
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"

#include "bmesh_class.hh"

/* stuff for dealing with header flags */
#define BM_elem_flag_test(ele, hflag) _bm_elem_flag_test(&(ele)->head, hflag)
#define BM_elem_flag_test_bool(ele, hflag) _bm_elem_flag_test_bool(&(ele)->head, hflag)
#define BM_elem_flag_enable(ele, hflag) _bm_elem_flag_enable(&(ele)->head, hflag)
#define BM_elem_flag_disable(ele, hflag) _bm_elem_flag_disable(&(ele)->head, hflag)
#define BM_elem_flag_set(ele, hflag, val) _bm_elem_flag_set(&(ele)->head, hflag, val)
#define BM_elem_flag_toggle(ele, hflag) _bm_elem_flag_toggle(&(ele)->head, hflag)
#define BM_elem_flag_merge(ele_a, ele_b) _bm_elem_flag_merge(&(ele_a)->head, &(ele_b)->head)
#define BM_elem_flag_merge_ex(ele_a, ele_b, hflag_and) \
  _bm_elem_flag_merge_ex(&(ele_a)->head, &(ele_b)->head, hflag_and)
#define BM_elem_flag_merge_into(ele, ele_a, ele_b) \
  _bm_elem_flag_merge_into(&(ele)->head, &(ele_a)->head, &(ele_b)->head)

ATTR_WARN_UNUSED_RESULT
BLI_INLINE char _bm_elem_flag_test(const BMHeader *head, const char hflag)
{
  return head->hflag & hflag;
}

ATTR_WARN_UNUSED_RESULT
BLI_INLINE bool _bm_elem_flag_test_bool(const BMHeader *head, const char hflag)
{
  return (head->hflag & hflag) != 0;
}

BLI_INLINE void _bm_elem_flag_enable(BMHeader *head, const char hflag)
{
  head->hflag |= hflag;
}

BLI_INLINE void _bm_elem_flag_disable(BMHeader *head, const char hflag)
{
  head->hflag &= (char)~hflag;
}

BLI_INLINE void _bm_elem_flag_set(BMHeader *head, const char hflag, const int val)
{
  if (val) {
    _bm_elem_flag_enable(head, hflag);
  }
  else {
    _bm_elem_flag_disable(head, hflag);
  }
}

BLI_INLINE void _bm_elem_flag_toggle(BMHeader *head, const char hflag)
{
  head->hflag ^= hflag;
}

BLI_INLINE void _bm_elem_flag_merge(BMHeader *head_a, BMHeader *head_b)
{
  head_a->hflag = head_b->hflag = head_a->hflag | head_b->hflag;
}

BLI_INLINE void _bm_elem_flag_merge_ex(BMHeader *head_a, BMHeader *head_b, const char hflag_and)
{
  if (((head_a->hflag & head_b->hflag) & hflag_and) == 0) {
    head_a->hflag &= ~hflag_and;
    head_b->hflag &= ~hflag_and;
  }
  _bm_elem_flag_merge(head_a, head_b);
}

BLI_INLINE void _bm_elem_flag_merge_into(BMHeader *head,
                                         const BMHeader *head_a,
                                         const BMHeader *head_b)
{
  head->hflag = head_a->hflag | head_b->hflag;
}

/**
 * notes on #BM_elem_index_set(...) usage,
 * Set index is sometimes abused as temp storage, other times we can't be
 * sure if the index values are valid because certain operations have modified
 * the mesh structure.
 *
 * To set the elements to valid indices 'BM_mesh_elem_index_ensure' should be used
 * rather than adding inline loops, however there are cases where we still
 * set the index directly
 *
 * In an attempt to manage this,
 * here are 5 tags I'm adding to uses of #BM_elem_index_set
 *
 * - `set_inline`  -- since the data is already being looped over set to a
 *                    valid value inline.
 *
 * - `set_dirty!`  -- intentionally sets the index to an invalid value,
 *                    flagging `bm->elem_index_dirty` so we don't use it.
 *
 * - `set_ok`      -- this is valid use since the part of the code is low level.
 *
 * - `set_ok_invalid`  -- set to -1 on purpose since this should not be
 *                    used without a full array re-index, do this on
 *                    adding new vert/edge/faces since they may be added at
 *                    the end of the array.
 *
 * - campbell */

#define BM_elem_index_get(ele) _bm_elem_index_get(&(ele)->head)
#define BM_elem_index_set(ele, index) _bm_elem_index_set(&(ele)->head, index)

BLI_INLINE void _bm_elem_index_set(BMHeader *head, const int index)
{
  head->index = index;
}

ATTR_WARN_UNUSED_RESULT
BLI_INLINE int _bm_elem_index_get(const BMHeader *head)
{
  return head->index;
}
