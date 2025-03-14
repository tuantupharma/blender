/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "bmesh_class.hh"

/**
 * Split all faces that match `filter_fn`.
 * \note
 */
void BM_mesh_separate_faces(BMesh *bm, BMFaceFilterFunc filter_fn, void *user_data);
