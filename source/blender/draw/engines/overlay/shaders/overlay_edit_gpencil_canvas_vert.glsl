/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_info.hh"

VERTEX_SHADER_CREATE_INFO(overlay_gpencil_canvas)

#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  vec2 pos;
  pos.x = float(gl_VertexID % 2);
  pos.y = float(gl_VertexID / 2) / float(halfLineCount - 1);

  if (pos.y > 1.0f) {
    pos.xy = pos.yx;
    pos.x -= 1.0f + 1.0f / float(halfLineCount - 1);
  }

  pos -= 0.5f;

  vec3 world_pos = xAxis * pos.x + yAxis * pos.y + origin;

  gl_Position = drw_point_world_to_homogenous(world_pos);

  view_clipping_distances(world_pos);

  finalColor = color;

  /* Convert to screen position [0..sizeVp]. */
  edgePos = edgeStart = ((gl_Position.xy / gl_Position.w) * 0.5f + 0.5f) * sizeViewport;
}
