/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Debug Shader outputting a gradient of orange - white - blue to mark culling hot-spots.
 * Green pixels are error pixels that are missing lights from the culling pass (i.e: when culling
 * pass is not conservative enough).
 */

#include "infos/eevee_light_culling_info.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_light_culling_debug)

#include "draw_view_lib.glsl"
#include "eevee_light_iter_lib.glsl"
#include "eevee_light_lib.glsl"
#include "gpu_shader_debug_gradients_lib.glsl"

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;
  float vP_z = drw_depth_screen_to_view(depth);
  vec3 P = drw_point_screen_to_world(vec3(uvcoordsvar.xy, depth));

  float light_count = 0.0f;
  uint light_cull = 0u;
  vec2 px = gl_FragCoord.xy;
  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, px, vP_z, l_idx) {
    light_cull |= 1u << l_idx;
    light_count += 1.0f;
  }
  LIGHT_FOREACH_END

  uint light_nocull = 0u;
  LIGHT_FOREACH_BEGIN_LOCAL_NO_CULL(light_cull_buf, l_idx)
  {
    LightData light = light_buf[l_idx];
    LightVector lv = light_vector_get(light, false, P);
    if (light_attenuation_surface(light, false, lv) > LIGHT_ATTENUATION_THRESHOLD) {
      light_nocull |= 1u << l_idx;
    }
  }
  LIGHT_FOREACH_END

  vec4 color = vec4(heatmap_gradient(light_count / 4.0f), 1.0f);

  if ((light_cull & light_nocull) != light_nocull) {
    /* ERROR. Some lights were culled incorrectly. */
    color = vec4(0.0f, 1.0f, 0.0f, 1.0f);
  }

  out_debug_color_add = vec4(color.rgb, 0.0f) * 0.2f;
  out_debug_color_mul = color;
}
