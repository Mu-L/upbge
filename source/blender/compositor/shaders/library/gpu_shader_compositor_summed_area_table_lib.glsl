/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* Computes the sum of the rectangular region defined by the given lower and upper bounds from the
 * given summed area table. It is assumed that the given upper bound is larger than the given lower
 * bound, otherwise, undefined behavior is invoked. Looking at the diagram below, in order to
 * compute the sum of area X, we sample the table at each of the corners of the area X, to get:
 *
 *   Upper Right -> A + B + C + X      (1)
 *   Upper Left -> A + B               (2)
 *   Lower Right -> B + C              (3)
 *   Lower Left -> B                   (4)
 *
 * We start from (1) and subtract (2) and (3) to get rid of A and C to get:
 *
 *  (A + B + C + X) - (A + B) - (B + C) = (X - B)
 *
 * To get rid of B, we add (4) to get:
 *
 *  (X - B) + B = X
 *
 *         ^
 *         |
 *         +-------+-----+
 *         |       |     |
 *         |   A   |  X  |
 *         |       |     |
 *         +-------+-----+
 *         |       |     |
 *         |   B   |  C  |
 *         |       |     |
 *         o-------+-----+------>
 *
 * The aforementioned equation eliminates the edges between regions X, C, and A since they get
 * subtracted with C and A. To avoid this, we subtract 1 from the lower bound and fall back to zero
 * for out of bound sampling. */

#include "gpu_shader_compositor_texture_utilities.glsl"

float4 summed_area_table_sum(sampler2D table, int2 lower_bound, int2 upper_bound)
{
  int2 corrected_lower_bound = lower_bound - int2(1);
  int2 corrected_upper_bound = min(texture_size(table) - int2(1), upper_bound);
  float4 addend = texture_load(table, corrected_upper_bound, float4(0.0f)) +
                  texture_load(table, corrected_lower_bound, float4(0.0f));
  float4 subtrahend =
      texture_load(table, int2(corrected_lower_bound.x, corrected_upper_bound.y), float4(0.0f)) +
      texture_load(table, int2(corrected_upper_bound.x, corrected_lower_bound.y), float4(0.0f));
  return addend - subtrahend;
}
