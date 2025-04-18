/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

#ifdef POINTCLOUD_SHADER
#  define COMMON_POINTCLOUD_LIB

#  ifndef DRW_POINTCLOUD_INFO
#    error Ensure createInfo includes draw_pointcloud.
#  endif

int pointcloud_get_point_id()
{
#  ifdef GPU_VERTEX_SHADER
  /* Remove shape indices. */
  return gl_VertexID >> 3;
#  endif
  return 0;
}

mat3 pointcloud_get_facing_matrix(vec3 p)
{
  mat3 facing_mat;
  facing_mat[2] = drw_world_incident_vector(p);
  facing_mat[1] = normalize(cross(drw_view().viewinv[0].xyz, facing_mat[2]));
  facing_mat[0] = cross(facing_mat[1], facing_mat[2]);
  return facing_mat;
}

/* Returns world center position and radius. */
void pointcloud_get_pos_and_radius(out vec3 outpos, out float outradius)
{
  int id = pointcloud_get_point_id();
  vec4 pos_rad = texelFetch(ptcloud_pos_rad_tx, id);
  outpos = drw_point_object_to_world(pos_rad.xyz);
  outradius = dot(abs(to_float3x3(drw_modelmat()) * pos_rad.www), vec3(1.0f / 3.0f));
}

/* Return world position and normal. */
void pointcloud_get_pos_nor_radius(out vec3 outpos, out vec3 outnor, out float outradius)
{
  vec3 p;
  float radius = 0.0f;
  pointcloud_get_pos_and_radius(p, radius);

  mat3 facing_mat = pointcloud_get_facing_matrix(p);

  uint vert_id = 0u;
#  ifdef GPU_VERTEX_SHADER
  /* Mask point indices. */
  vert_id = uint(gl_VertexID) & ~(0xFFFFFFFFu << 3u);
#  endif

  vec3 pos_inst = vec3(0.0f);

  switch (vert_id) {
    case 0:
      pos_inst.z = 1.0f;
      break;
    case 1:
      pos_inst.x = 1.0f;
      break;
    case 2:
      pos_inst.y = 1.0f;
      break;
    case 3:
      pos_inst.x = -1.0f;
      break;
    case 4:
      pos_inst.y = -1.0f;
      break;
  }

  outnor = facing_mat * pos_inst;
  outpos = p + outnor * radius;
  outradius = radius;
}

/* Return world position and normal. */
void pointcloud_get_pos_and_nor(out vec3 outpos, out vec3 outnor)
{
  vec3 nor, pos;
  float radius = 0.0f;
  pointcloud_get_pos_nor_radius(pos, nor, radius);
  outpos = pos;
  outnor = nor;
}

vec3 pointcloud_get_pos()
{
  vec3 outpos, outnor;
  pointcloud_get_pos_and_nor(outpos, outnor);
  return outpos;
}

float pointcloud_get_customdata_float(const samplerBuffer cd_buf)
{
  int id = pointcloud_get_point_id();
  return texelFetch(cd_buf, id).r;
}

vec2 pointcloud_get_customdata_vec2(const samplerBuffer cd_buf)
{
  int id = pointcloud_get_point_id();
  return texelFetch(cd_buf, id).rg;
}

vec3 pointcloud_get_customdata_vec3(const samplerBuffer cd_buf)
{
  int id = pointcloud_get_point_id();
  return texelFetch(cd_buf, id).rgb;
}

vec4 pointcloud_get_customdata_vec4(const samplerBuffer cd_buf)
{
  int id = pointcloud_get_point_id();
  return texelFetch(cd_buf, id).rgba;
}

vec2 pointcloud_get_barycentric()
{
  /* TODO: To be implemented. */
  return vec2(0.0f);
}
#endif
