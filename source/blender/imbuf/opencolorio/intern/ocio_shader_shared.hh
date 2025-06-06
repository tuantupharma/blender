/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  include "GPU_shader_shared_utils.hh"
#endif

struct OCIO_GPUCurveMappingParameters {
  /* Curve mapping parameters
   *
   * See documentation for OCIO_CurveMappingSettings to get fields descriptions.
   * (this ones pretty much copies stuff from C structure.)
   */
  float4 mintable;
  float4 range;
  float4 ext_in_x;
  float4 ext_in_y;
  float4 ext_out_x;
  float4 ext_out_y;
  float4 first_x;
  float4 first_y;
  float4 last_x;
  float4 last_y;
  float4 black;
  float4 bwmul;
  int lut_size;
  int use_extend_extrapolate;
  int _pad0;
  int _pad1;
};

struct OCIO_GPUParameters {
  float dither;
  float exponent;
  bool32_t use_predivide;
  bool32_t do_overlay_merge;
  bool32_t use_hdr;
  int _pad0;
  int _pad1;
  int _pad2;
  float4x4 scene_linear_matrix;
};
