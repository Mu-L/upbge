/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "subdiv_lib.glsl"

COMPUTE_SHADER_CREATE_INFO(subdiv_custom_data_interp_4d_f32)

#if defined(DIMENSIONS_1)
#  define DIMENSIONS 1
#elif defined(DIMENSIONS_2)
#  define DIMENSIONS 2
#elif defined(DIMENSIONS_3)
#  define DIMENSIONS 3
#else  // defined(DIMENSIONS_4)
#  define DIMENSIONS 4
#endif

struct Vertex {
  float vertex_data[DIMENSIONS];
};

void clear(inout Vertex v)
{
  for (int i = 0; i < DIMENSIONS; i++) {
    v.vertex_data[i] = 0.0f;
  }
}

Vertex read_vertex(uint index)
{
  Vertex result;
#if defined(GPU_COMP_U16)
  uint base_index = index * 2;
  if (DIMENSIONS == 4) {
    uint xy = src_data[base_index];
    uint zw = src_data[base_index + 1];

    float x = float((xy >> 16) & 0xffff) / 65535.0f;
    float y = float(xy & 0xffff) / 65535.0f;
    float z = float((zw >> 16) & 0xffff) / 65535.0f;
    float w = float(zw & 0xffff) / 65535.0f;

    result.vertex_data[0] = x;
    result.vertex_data[1] = y;
    result.vertex_data[2] = z;
    result.vertex_data[3] = w;
  }
  else {
    /* This case is unsupported for now. */
    clear(result);
  }
#elif defined(GPU_COMP_I32)
  uint base_index = index * DIMENSIONS;
  for (int i = 0; i < DIMENSIONS; i++) {
    result.vertex_data[i] = float(src_data[base_index + i]);
  }
#else
  uint base_index = index * DIMENSIONS;
  for (int i = 0; i < DIMENSIONS; i++) {
    result.vertex_data[i] = src_data[base_index + i];
  }
#endif
  return result;
}

void write_vertex(uint index, Vertex v)
{
#if defined(GPU_COMP_U16)
  uint base_index = shader_data.dst_offset + index * 2;
  if (DIMENSIONS == 4) {
    uint x = uint(v.vertex_data[0] * 65535.0f);
    uint y = uint(v.vertex_data[1] * 65535.0f);
    uint z = uint(v.vertex_data[2] * 65535.0f);
    uint w = uint(v.vertex_data[3] * 65535.0f);

    uint xy = x << 16 | y;
    uint zw = z << 16 | w;

    dst_data[base_index] = xy;
    dst_data[base_index + 1] = zw;
  }
  else {
    /* This case is unsupported for now. */
    dst_data[base_index] = 0;
  }
#elif defined(GPU_COMP_I32)
  uint base_index = shader_data.dst_offset + index * DIMENSIONS;
  for (int i = 0; i < DIMENSIONS; i++) {
    dst_data[base_index + i] = int(round(v.vertex_data[i]));
  }
#else
  uint base_index = shader_data.dst_offset + index * DIMENSIONS;
#  ifdef NORMALIZE
  vec3 value = vec3(v.vertex_data[0], v.vertex_data[1], v.vertex_data[2]);
  value = normalize(value);
  v.vertex_data[0] = value.x;
  v.vertex_data[1] = value.y;
  v.vertex_data[2] = value.z;
#  endif
  for (int i = 0; i < DIMENSIONS; i++) {
    dst_data[base_index + i] = v.vertex_data[i];
  }
#endif
}

Vertex interp_vertex(Vertex v0, Vertex v1, Vertex v2, Vertex v3, float2 uv)
{
  Vertex result;
  for (int i = 0; i < DIMENSIONS; i++) {
    float e = mix(v0.vertex_data[i], v1.vertex_data[i], uv.x);
    float f = mix(v2.vertex_data[i], v3.vertex_data[i], uv.x);
    result.vertex_data[i] = mix(e, f, uv.y);
  }
  return result;
}

void add_with_weight(inout Vertex v0, Vertex v1, float weight)
{
  for (int i = 0; i < DIMENSIONS; i++) {
    v0.vertex_data[i] += v1.vertex_data[i] * weight;
  }
}

Vertex average(Vertex v0, Vertex v1)
{
  Vertex result;
  for (int i = 0; i < DIMENSIONS; i++) {
    result.vertex_data[i] = (v0.vertex_data[i] + v1.vertex_data[i]) * 0.5f;
  }
  return result;
}

uint get_vertex_count(uint coarse_face)
{
  uint number_of_patches = face_ptex_offset[coarse_face + 1] - face_ptex_offset[coarse_face];
  if (number_of_patches == 1) {
    /* If there is only one patch for the current coarse polygon, then it is a quad. */
    return 4;
  }
  /* Otherwise, the number of patches is the number of vertices. */
  return number_of_patches;
}

uint get_polygon_corner_index(uint coarse_face, uint patch_index)
{
  uint patch_offset = face_ptex_offset[coarse_face];
  return patch_index - patch_offset;
}

uint get_loop_start(uint coarse_face)
{
  return extra_coarse_face_data[coarse_face] & shader_data.coarse_face_loopstart_mask;
}

void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= shader_data.total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  /* Find which coarse polygon we came from. */
  uint coarse_face = coarse_face_index_from_subdiv_quad_index(quad_index,
                                                              shader_data.coarse_face_count);
  uint loop_start = get_loop_start(coarse_face);

  /* Find the number of vertices for the coarse polygon. */
  Vertex v0, v1, v2, v3;
  clear(v0);
  clear(v1);
  clear(v2);
  clear(v3);

  uint number_of_vertices = get_vertex_count(coarse_face);
  if (number_of_vertices == 4) {
    /* Interpolate the src data. */
    v0 = read_vertex(loop_start + 0);
    v1 = read_vertex(loop_start + 1);
    v2 = read_vertex(loop_start + 2);
    v3 = read_vertex(loop_start + 3);
  }
  else {
    /* Interpolate the src data for the center. */
    uint loop_end = loop_start + number_of_vertices;
    Vertex center_value;
    clear(center_value);

    float weight = 1.0f / float(number_of_vertices);

    for (uint l = loop_start; l < loop_end; l++) {
      add_with_weight(center_value, read_vertex(l), weight);
    }

    /* Interpolate between the previous and next corner for the middle values for the edges. */
    uint patch_index = uint(patch_coords[start_loop_index].patch_index);
    uint current_coarse_corner = get_polygon_corner_index(coarse_face, patch_index);
    uint next_coarse_corner = (current_coarse_corner + 1) % number_of_vertices;
    uint prev_coarse_corner = (current_coarse_corner + number_of_vertices - 1) %
                              number_of_vertices;

    v0 = read_vertex(loop_start + current_coarse_corner);
    v1 = average(v0, read_vertex(loop_start + next_coarse_corner));
    v3 = average(v0, read_vertex(loop_start + prev_coarse_corner));

    /* Interpolate between the current value, and the ones for the center and mid-edges. */
    v2 = center_value;
  }

  /* Do a linear interpolation of the data based on the UVs for each loop of this subdivided quad.
   */
  for (uint loop_index = start_loop_index; loop_index < start_loop_index + 4; loop_index++) {
    BlenderPatchCoord co = patch_coords[loop_index];
    float2 uv = decode_uv(co.encoded_uv);
    /* NOTE: v2 and v3 are reversed to stay consistent with the interpolation weight on the x-axis:
     *
     * v3 +-----+ v2
     *    |     |
     *    |     |
     * v0 +-----+ v1
     *
     * otherwise, weight would be `1.0f - uv.x` for `v2 <-> v3`, but `uv.x` for `v0 <-> v1`.
     */
    Vertex result = interp_vertex(v0, v1, v3, v2, uv);
    write_vertex(loop_index, result);
  }
}
