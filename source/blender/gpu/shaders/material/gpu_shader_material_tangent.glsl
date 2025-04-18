/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_transform_utils.glsl"

void tangent_orco_x(vec3 orco_in, out vec3 orco_out)
{
  orco_out = orco_in.xzy * vec3(0.0f, -0.5f, 0.5f) + vec3(0.0f, 0.25f, -0.25f);
}

void tangent_orco_y(vec3 orco_in, out vec3 orco_out)
{
  orco_out = orco_in.zyx * vec3(-0.5f, 0.0f, 0.5f) + vec3(0.25f, 0.0f, -0.25f);
}

void tangent_orco_z(vec3 orco_in, out vec3 orco_out)
{
  orco_out = orco_in.yxz * vec3(-0.5f, 0.5f, 0.0f) + vec3(0.25f, -0.25f, 0.0f);
}

void node_tangentmap(vec4 attr_tangent, out vec3 tangent)
{
  tangent = normalize(attr_tangent.xyz);
}

void node_tangent(vec3 orco, out vec3 T)
{
  direction_transform_object_to_world(orco, T);
  T = cross(g_data.N, normalize(cross(T, g_data.N)));
}
