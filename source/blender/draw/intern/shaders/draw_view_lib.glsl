/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view_info.hh"

SHADER_LIBRARY_CREATE_INFO(draw_view)

#if !defined(DRAW_VIEW_CREATE_INFO) && !defined(GLSL_CPP_STUBS)
#  error Missing draw_view additional create info on shader create info
#endif

/* Returns the current active view. */
ViewMatrices drw_view()
{
  return drw_view_buf[drw_view_id];
}

/* Returns true if the current view has a perspective projection matrix. */
bool drw_view_is_perspective()
{
  return drw_view().winmat[3][3] == 0.0f;
}

/* Returns the view forward vector, going towards the viewer. */
vec3 drw_view_forward()
{
  return drw_view().viewinv[2].xyz;
}

/* Returns the view origin. */
vec3 drw_view_position()
{
  return drw_view().viewinv[3].xyz;
}

/* Positive Z distance from the view origin. Faster than using `drw_point_world_to_view`. */
float drw_view_z_distance(vec3 P)
{
  return dot(P - drw_view_position(), -drw_view_forward());
}

/* Returns the projection matrix far clip distance. */
float drw_view_far()
{
  if (drw_view_is_perspective()) {
    return -drw_view().winmat[3][2] / (drw_view().winmat[2][2] + 1.0f);
  }
  return -(drw_view().winmat[3][2] - 1.0f) / drw_view().winmat[2][2];
}

/* Returns the projection matrix near clip distance. */
float drw_view_near()
{
  if (drw_view_is_perspective()) {
    return -drw_view().winmat[3][2] / (drw_view().winmat[2][2] - 1.0f);
  }
  return -(drw_view().winmat[3][2] + 1.0f) / drw_view().winmat[2][2];
}

/**
 * Returns the world incident vector `V` (going towards the viewer)
 * from the world position `P` and the current view.
 */
vec3 drw_world_incident_vector(vec3 P)
{
  return drw_view_is_perspective() ? normalize(drw_view_position() - P) : drw_view_forward();
}

/**
 * Returns the view incident vector `vV` (going towards the viewer)
 * from the view position `vP` and the current view.
 */
vec3 drw_view_incident_vector(vec3 vP)
{
  return drw_view_is_perspective() ? normalize(-vP) : vec3(0.0f, 0.0f, 1.0f);
}

/**
 * Transform position on screen UV space [0..1] to Normalized Device Coordinate space [-1..1].
 */
vec3 drw_screen_to_ndc(vec3 ss_P)
{
  return ss_P * 2.0f - 1.0f;
}
vec2 drw_screen_to_ndc(vec2 ss_P)
{
  return ss_P * 2.0f - 1.0f;
}
float drw_screen_to_ndc(float ss_P)
{
  return ss_P * 2.0f - 1.0f;
}

/**
 * Transform position in Normalized Device Coordinate [-1..1] to screen UV space [0..1].
 */
vec3 drw_ndc_to_screen(vec3 ndc_P)
{
  return ndc_P * 0.5f + 0.5f;
}
vec2 drw_ndc_to_screen(vec2 ndc_P)
{
  return ndc_P * 0.5f + 0.5f;
}
float drw_ndc_to_screen(float ndc_P)
{
  return ndc_P * 0.5f + 0.5f;
}

/* -------------------------------------------------------------------- */
/** \name Transform Normal
 * \{ */

vec3 drw_normal_view_to_world(vec3 vN)
{
  return (to_float3x3(drw_view().viewinv) * vN);
}

vec3 drw_normal_world_to_view(vec3 N)
{
  return (to_float3x3(drw_view().viewmat) * N);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Position
 * \{ */

vec3 drw_perspective_divide(vec4 hs_P)
{
  return hs_P.xyz / hs_P.w;
}

vec3 drw_point_view_to_world(vec3 vP)
{
  return (drw_view().viewinv * vec4(vP, 1.0f)).xyz;
}
vec4 drw_point_view_to_homogenous(vec3 vP)
{
  return (drw_view().winmat * vec4(vP, 1.0f));
}
vec3 drw_point_view_to_ndc(vec3 vP)
{
  return drw_perspective_divide(drw_point_view_to_homogenous(vP));
}

vec3 drw_point_world_to_view(vec3 P)
{
  return (drw_view().viewmat * vec4(P, 1.0f)).xyz;
}
vec4 drw_point_world_to_homogenous(vec3 P)
{
  return (drw_view().winmat * (drw_view().viewmat * vec4(P, 1.0f)));
}
vec3 drw_point_world_to_ndc(vec3 P)
{
  return drw_perspective_divide(drw_point_world_to_homogenous(P));
}

vec3 drw_point_ndc_to_view(vec3 ssP)
{
  return drw_perspective_divide(drw_view().wininv * vec4(ssP, 1.0f));
}
vec3 drw_point_ndc_to_world(vec3 ssP)
{
  return drw_point_view_to_world(drw_point_ndc_to_view(ssP));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Screen Positions
 * \{ */

vec3 drw_point_view_to_screen(vec3 vP)
{
  return drw_ndc_to_screen(drw_point_view_to_ndc(vP));
}
vec3 drw_point_world_to_screen(vec3 vP)
{
  return drw_ndc_to_screen(drw_point_world_to_ndc(vP));
}

vec3 drw_point_screen_to_view(vec3 ssP)
{
  return drw_point_ndc_to_view(drw_screen_to_ndc(ssP));
}
vec3 drw_point_screen_to_world(vec3 ssP)
{
  return drw_point_view_to_world(drw_point_screen_to_view(ssP));
}

float drw_depth_view_to_screen(float v_depth)
{
  return drw_point_view_to_screen(vec3(0.0f, 0.0f, v_depth)).z;
}
float drw_depth_screen_to_view(float ss_depth)
{
  return drw_point_screen_to_view(vec3(0.0f, 0.0f, ss_depth)).z;
}

/** \} */
