/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 */

#include <float.h>
#include <string.h>

#include "../geometry/BBox.h"
#include "../geometry/Geom.h"
#include "../geometry/GeomCleaner.h"
#include "../geometry/GeomUtils.h"
#include "../scene_graph/IndexedFaceSet.h"
#include "../scene_graph/NodeGroup.h"
#include "../scene_graph/NodeShape.h"
#include "../scene_graph/NodeTransform.h"
#include "../system/FreestyleConfig.h"
#include "../system/RenderMonitor.h"

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "render_types.h"

#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_mesh.h"
#include "BKE_scene.hh"

#include "BLI_iterator.h"
#include "BLI_listbase.h"

#include "DEG_depsgraph_query.hh"

#include "MEM_guardedalloc.h"

namespace Freestyle {

class NodeGroup;

struct LoaderState {
  float *pv;
  float *pn;
  IndexedFaceSet::FaceEdgeMark *pm;
  uint *pvi;
  uint *pni;
  uint *pmi;
  uint currentIndex;
  uint currentMIndex;
  float minBBox[3];
  float maxBBox[3];
};

class BlenderFileLoader {
 public:
  /** Builds a MaxFileLoader */
  BlenderFileLoader(Render *re, ViewLayer *view_layer, Depsgraph *depsgraph);
  virtual ~BlenderFileLoader();

  /** Loads the 3D scene and returns a pointer to the scene root node */
  NodeGroup *Load();

  /** Gets the number of read faces */
  inline uint numFacesRead()
  {
    return _numFacesRead;
  }

#if 0
  /** Gets the smallest edge size read */
  inline real minEdgeSize()
  {
    return _minEdgeSize;
  }
#endif

  /** Modifiers */
  inline void setRenderMonitor(RenderMonitor *iRenderMonitor)
  {
    _pRenderMonitor = iRenderMonitor;
  }

 protected:
  void insertShapeNode(Object *ob, Mesh *mesh, int id);
  int testDegenerateTriangle(float v1[3], float v2[3], float v3[3]);
  int countClippedFaces(float v1[3], float v2[3], float v3[3], int clip[3]);
  void clipLine(float v1[3], float v2[3], float c[3], float z);
  void clipTriangle(int numTris,
                    float triCoords[][3],
                    float v1[3],
                    float v2[3],
                    float v3[3],
                    float triNormals[][3],
                    float n1[3],
                    float n2[3],
                    float n3[3],
                    bool edgeMarks[5],
                    bool em1,
                    bool em2,
                    bool em3,
                    const int clip[3]);
  void addTriangle(struct LoaderState *ls,
                   float v1[3],
                   float v2[3],
                   float v3[3],
                   float n1[3],
                   float n2[3],
                   float n3[3],
                   bool fm,
                   bool em1,
                   bool em2,
                   bool em3);

 protected:
  struct detri_t {
    uint viA, viB, viP;  // 0 <= viA, viB, viP < viSize
    Vec3r v;
    uint n;
  };
  Render *_re;
  Depsgraph *_depsgraph;
  NodeGroup *_Scene;
  uint _numFacesRead;
#if 0
  real _minEdgeSize;
#endif
  bool _smooth; /* if true, face smoothness is taken into account */
  float _z_near, _z_far;
  float _z_offset;

  RenderMonitor *_pRenderMonitor;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BlenderFileLoader")
};

} /* namespace Freestyle */
