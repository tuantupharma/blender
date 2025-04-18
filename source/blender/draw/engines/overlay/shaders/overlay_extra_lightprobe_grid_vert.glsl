/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_info.hh"

VERTEX_SHADER_CREATE_INFO(overlay_extra_grid_base)
VERTEX_SHADER_CREATE_INFO(draw_modelmat)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

vec4 color_from_id(float color_id)
{
  if (isTransform) {
    return colorTransform;
  }
  else if (color_id == 1.0f) {
    return colorActive;
  }
  else /* 2.0f */ {
    return colorSelect;
  }

  return colorTransform;
}

/* Replace top 2 bits (of the 16bit output) by outlineId.
 * This leaves 16K different IDs to create outlines between objects.
 * SHIFT = (32 - (16 - 2)) */
#define SHIFT 18u

void main()
{
  select_id_set(drw_custom_id());
  mat4 model_mat = gridModelMatrix;
  model_mat[0][3] = model_mat[1][3] = model_mat[2][3] = 0.0f;
  model_mat[3][3] = 1.0f;
  float color_id = gridModelMatrix[3].w;

  ivec3 grid_resolution = ivec3(gridModelMatrix[0].w, gridModelMatrix[1].w, gridModelMatrix[2].w);

  vec3 ls_cell_location;
  /* Keep in sync with update_irradiance_probe */
  ls_cell_location.z = float(gl_VertexID % grid_resolution.z);
  ls_cell_location.y = float((gl_VertexID / grid_resolution.z) % grid_resolution.y);
  ls_cell_location.x = float(gl_VertexID / (grid_resolution.z * grid_resolution.y));

  ls_cell_location += 1.0f;
  ls_cell_location /= vec3(grid_resolution + 1);
  ls_cell_location = ls_cell_location * 2.0f - 1.0f;

  vec3 ws_cell_location = (model_mat * vec4(ls_cell_location, 1.0f)).xyz;
  gl_Position = drw_point_world_to_homogenous(ws_cell_location);
  gl_PointSize = sizeVertex * 2.0f;

  finalColor = color_from_id(color_id);

  /* Shade occluded points differently. */
  vec4 p = gl_Position / gl_Position.w;
  float z_depth = texture(depthBuffer, p.xy * 0.5f + 0.5f).r * 2.0f - 1.0f;
  float z_delta = p.z - z_depth;
  if (z_delta > 0.0f) {
    float fac = 1.0f - z_delta * 10000.0f;
    /* Smooth blend to avoid flickering. */
    finalColor = mix(colorBackground, finalColor, clamp(fac, 0.2f, 1.0f));
  }

  view_clipping_distances(ws_cell_location);
}
