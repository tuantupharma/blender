/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_info.hh"

FRAGMENT_SHADER_CREATE_INFO(gpencil_layer_blend)

#include "gpencil_common_lib.glsl"

void main()
{
  vec4 color;

  /* Remember, this is associated alpha (aka. pre-multiply). */
  color.rgb = textureLod(colorBuf, uvcoordsvar.xy, 0).rgb;
  /* Stroke only render mono-chromatic revealage. We convert to alpha. */
  color.a = 1.0f - textureLod(revealBuf, uvcoordsvar.xy, 0).r;

  float mask = textureLod(maskBuf, uvcoordsvar.xy, 0).r;
  mask *= blendOpacity;

  fragColor = vec4(1.0f, 0.0f, 1.0f, 1.0f);
  fragRevealage = vec4(1.0f, 0.0f, 1.0f, 1.0f);

  blend_mode_output(blendMode, color, mask, fragColor, fragRevealage);
}
