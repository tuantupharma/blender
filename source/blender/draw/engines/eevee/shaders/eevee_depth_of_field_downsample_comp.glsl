/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Downsample pass: CoC aware downsample to quarter resolution.
 *
 * Pretty much identical to the setup pass but get CoC from buffer.
 * Also does not weight luma for the bilateral weights.
 */

#include "infos/eevee_depth_of_field_info.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_depth_of_field_downsample)

#include "eevee_depth_of_field_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

void main()
{
  vec2 halfres_texel_size = 1.0f / vec2(textureSize(color_tx, 0).xy);
  /* Center uv around the 4 half-resolution pixels. */
  vec2 quad_center = vec2(gl_GlobalInvocationID.xy * 2 + 1) * halfres_texel_size;

  vec4 colors[4];
  vec4 cocs;
  for (int i = 0; i < 4; i++) {
    vec2 sample_uv = quad_center + quad_offsets[i] * halfres_texel_size;
    colors[i] = textureLod(color_tx, sample_uv, 0.0f);
    cocs[i] = textureLod(coc_tx, sample_uv, 0.0f).r;
  }

  vec4 weights = dof_bilateral_coc_weights(cocs);
  /* Normalize so that the sum is 1. */
  weights *= safe_rcp(reduce_add(weights));

  vec4 out_color = weighted_sum_array(colors, weights);

  imageStore(out_color_img, ivec2(gl_GlobalInvocationID.xy), out_color);
}
