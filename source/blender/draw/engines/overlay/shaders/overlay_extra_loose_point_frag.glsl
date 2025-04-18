/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_info.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_extra_loose_point_base)

void main()
{
  vec2 centered = abs(gl_PointCoord - vec2(0.5f));
  float dist = max(centered.x, centered.y);

  float fac = dist * dist * 4.0f;
  /* Non linear blend. */
  vec4 col1 = sqrt(colorEditMeshMiddle);
  vec4 col2 = sqrt(finalColor);
  fragColor = mix(col1, col2, 0.45f + fac * 0.65f);
  fragColor *= fragColor;

  lineOutput = vec4(0.0f);

  /* Make the effect more like a fresnel by offsetting
   * the depth and creating mini-spheres.
   * Disabled as it has performance impact. */
  // gl_FragDepth = gl_FragCoord.z + 1e-6f * fac;
}
