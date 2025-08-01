/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Shader Virtual Machine
 *
 * A shader is a list of nodes to be executed. These are simply read one after
 * the other and executed, using an node counter. Each node and its associated
 * data is encoded as one or more uint4's in a 1D texture. If the data is larger
 * than an uint4, the node can increase the node counter to compensate for this.
 * Floats are encoded as int and then converted to float again.
 *
 * Nodes write their output into a stack. All stack data in the stack is
 * floats, since it's all factors, colors and vectors. The stack will be stored
 * in local memory on the GPU, as it would take too many register and indexes in
 * ways not known at compile time. This seems the only solution even though it
 * may be slow, with two positive factors. If the same shader is being executed,
 * memory access will be coalesced and cached.
 *
 * The result of shader execution will be a single closure. This means the
 * closure type, associated label, data and weight. Sampling from multiple
 * closures is supported through the mix closure node, the logic for that is
 * mostly taken care of in the SVM compiler.
 */

#include "kernel/globals.h"
#include "kernel/types.h"

#include "kernel/svm/types.h"
#include "kernel/svm/util.h"

/* Nodes */

#include "kernel/svm/aov.h"
#include "kernel/svm/attribute.h"
#include "kernel/svm/blackbody.h"
#include "kernel/svm/brick.h"
#include "kernel/svm/brightness.h"
#include "kernel/svm/bump.h"
#include "kernel/svm/camera.h"
#include "kernel/svm/checker.h"
#include "kernel/svm/clamp.h"
#include "kernel/svm/closure.h"
#include "kernel/svm/convert.h"
#include "kernel/svm/displace.h"
#include "kernel/svm/fresnel.h"
#include "kernel/svm/gabor.h"
#include "kernel/svm/gamma.h"
#include "kernel/svm/geometry.h"
#include "kernel/svm/gradient.h"
#include "kernel/svm/hsv.h"
#include "kernel/svm/ies.h"
#include "kernel/svm/image.h"
#include "kernel/svm/invert.h"
#include "kernel/svm/light_path.h"
#include "kernel/svm/magic.h"
#include "kernel/svm/map_range.h"
#include "kernel/svm/mapping.h"
#include "kernel/svm/math.h"
#include "kernel/svm/mix.h"
#include "kernel/svm/noisetex.h"
#include "kernel/svm/normal.h"
#include "kernel/svm/ramp.h"
#include "kernel/svm/sepcomb_color.h"
#include "kernel/svm/sepcomb_vector.h"
#include "kernel/svm/sky.h"
#include "kernel/svm/tex_coord.h"
#include "kernel/svm/value.h"
#include "kernel/svm/vector_rotate.h"
#include "kernel/svm/vector_transform.h"
#include "kernel/svm/vertex_color.h"
#include "kernel/svm/voronoi.h"
#include "kernel/svm/wave.h"
#include "kernel/svm/wavelength.h"
#include "kernel/svm/white_noise.h"
#include "kernel/svm/wireframe.h"

#ifdef __SHADER_RAYTRACE__
#  include "kernel/svm/ao.h"
#  include "kernel/svm/bevel.h"
#endif

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_USE_DATA_CONSTANTS__
#  define SVM_CASE(node) \
    case node: \
      if (!kernel_data_svm_usage_##node) \
        break;
#else
#  define SVM_CASE(node) case node:
#endif

/* Main Interpreter Loop */
template<uint node_feature_mask, ShaderType type, typename ConstIntegratorGenericState>
ccl_device void svm_eval_nodes(KernelGlobals kg,
                               ConstIntegratorGenericState state,
                               ccl_private ShaderData *sd,
                               ccl_global float *render_buffer,
                               const uint32_t path_flag)
{
  float stack[SVM_STACK_SIZE];
  Spectrum closure_weight;
  int offset = sd->shader & SHADER_MASK;

  while (true) {
    uint4 node = read_node(kg, &offset);

    switch (node.x) {
      SVM_CASE(NODE_END)
      return;
      SVM_CASE(NODE_SHADER_JUMP)
      {
        if (type == SHADER_TYPE_SURFACE) {
          offset = node.y;
        }
        else if (type == SHADER_TYPE_VOLUME) {
          offset = node.z;
        }
        else if (type == SHADER_TYPE_DISPLACEMENT) {
          offset = node.w;
        }
        else {
          return;
        }
        break;
      }
      SVM_CASE(NODE_CLOSURE_BSDF)
      offset = svm_node_closure_bsdf<node_feature_mask, type>(
          kg, sd, stack, closure_weight, node, path_flag, offset);
      break;
      SVM_CASE(NODE_CLOSURE_EMISSION)
      IF_KERNEL_NODES_FEATURE(EMISSION)
      {
        svm_node_closure_emission(kg, sd, stack, closure_weight, node);
      }
      break;
      SVM_CASE(NODE_CLOSURE_BACKGROUND)
      IF_KERNEL_NODES_FEATURE(EMISSION)
      {
        svm_node_closure_background(sd, stack, closure_weight, node);
      }
      break;
      SVM_CASE(NODE_CLOSURE_SET_WEIGHT)
      svm_node_closure_set_weight(&closure_weight, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_CLOSURE_WEIGHT)
      svm_node_closure_weight(stack, &closure_weight, node.y);
      break;
      SVM_CASE(NODE_EMISSION_WEIGHT)
      IF_KERNEL_NODES_FEATURE(EMISSION)
      {
        svm_node_emission_weight(stack, &closure_weight, node);
      }
      break;
      SVM_CASE(NODE_MIX_CLOSURE)
      svm_node_mix_closure(stack, node);
      break;
      SVM_CASE(NODE_JUMP_IF_ZERO)
      if (stack_load_float(stack, node.z) <= 0.0f) {
        offset += node.y;
      }
      break;
      SVM_CASE(NODE_JUMP_IF_ONE)
      if (stack_load_float(stack, node.z) >= 1.0f) {
        offset += node.y;
      }
      break;
      SVM_CASE(NODE_GEOMETRY)
      svm_node_geometry(kg, sd, stack, node.y, node.z);
      break;
      SVM_CASE(NODE_CONVERT)
      svm_node_convert(kg, stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_TEX_COORD)
      offset = svm_node_tex_coord(kg, sd, path_flag, stack, node, offset);
      break;
      SVM_CASE(NODE_VALUE_F)
      svm_node_value_f(stack, node.y, node.z);
      break;
      SVM_CASE(NODE_VALUE_V)
      offset = svm_node_value_v(kg, stack, node.y, offset);
      break;
      SVM_CASE(NODE_ATTR)
      svm_node_attr<node_feature_mask>(kg, sd, stack, node);
      break;
      SVM_CASE(NODE_VERTEX_COLOR)
      svm_node_vertex_color(kg, sd, stack, node);
      break;
      SVM_CASE(NODE_GEOMETRY_BUMP_DX)
      IF_KERNEL_NODES_FEATURE(BUMP)
      {
        svm_node_geometry_bump_dx(kg, sd, stack, node.y, node.z, __uint_as_float(node.w));
      }
      break;
      SVM_CASE(NODE_GEOMETRY_BUMP_DY)
      IF_KERNEL_NODES_FEATURE(BUMP)
      {
        svm_node_geometry_bump_dy(kg, sd, stack, node.y, node.z, __uint_as_float(node.w));
      }
      break;
      SVM_CASE(NODE_SET_DISPLACEMENT)
      svm_node_set_displacement<node_feature_mask>(sd, stack, node.y);
      break;
      SVM_CASE(NODE_DISPLACEMENT)
      svm_node_displacement<node_feature_mask>(kg, sd, stack, node);
      break;
      SVM_CASE(NODE_VECTOR_DISPLACEMENT)
      offset = svm_node_vector_displacement<node_feature_mask>(kg, sd, stack, node, offset);
      break;
      SVM_CASE(NODE_TEX_IMAGE)
      offset = svm_node_tex_image(kg, sd, stack, node, offset);
      break;
      SVM_CASE(NODE_TEX_IMAGE_BOX)
      svm_node_tex_image_box(kg, sd, stack, node);
      break;
      SVM_CASE(NODE_TEX_NOISE)
      offset = svm_node_tex_noise(kg, stack, node.y, node.z, node.w, offset);
      break;
      SVM_CASE(NODE_SET_BUMP)
      offset = svm_node_set_bump<node_feature_mask>(kg, sd, stack, node, offset);
      break;
      SVM_CASE(NODE_ATTR_BUMP_DX)
      IF_KERNEL_NODES_FEATURE(BUMP)
      {
        svm_node_attr_bump_dx(kg, sd, stack, node);
      }
      break;
      SVM_CASE(NODE_ATTR_BUMP_DY)
      IF_KERNEL_NODES_FEATURE(BUMP)
      {
        svm_node_attr_bump_dy(kg, sd, stack, node);
      }
      break;
      SVM_CASE(NODE_VERTEX_COLOR_BUMP_DX)
      IF_KERNEL_NODES_FEATURE(BUMP)
      {
        svm_node_vertex_color_bump_dx(kg, sd, stack, node);
      }
      break;
      SVM_CASE(NODE_VERTEX_COLOR_BUMP_DY)
      IF_KERNEL_NODES_FEATURE(BUMP)
      {
        svm_node_vertex_color_bump_dy(kg, sd, stack, node);
      }
      break;
      SVM_CASE(NODE_TEX_COORD_BUMP_DX)
      IF_KERNEL_NODES_FEATURE(BUMP)
      {
        offset = svm_node_tex_coord_bump_dx(kg, sd, path_flag, stack, node, offset);
      }
      break;
      SVM_CASE(NODE_TEX_COORD_BUMP_DY)
      IF_KERNEL_NODES_FEATURE(BUMP)
      {
        offset = svm_node_tex_coord_bump_dy(kg, sd, path_flag, stack, node, offset);
      }
      break;
      SVM_CASE(NODE_CLOSURE_SET_NORMAL)
      IF_KERNEL_NODES_FEATURE(BUMP)
      {
        svm_node_set_normal(sd, stack, node.y, node.z);
      }
      break;
      SVM_CASE(NODE_ENTER_BUMP_EVAL)
      IF_KERNEL_NODES_FEATURE(BUMP_STATE)
      {
        svm_node_enter_bump_eval(kg, sd, stack, node.y);
      }
      break;
      SVM_CASE(NODE_LEAVE_BUMP_EVAL)
      IF_KERNEL_NODES_FEATURE(BUMP_STATE)
      {
        svm_node_leave_bump_eval(sd, stack, node.y);
      }
      break;
      SVM_CASE(NODE_HSV)
      svm_node_hsv(stack, node);
      break;
      SVM_CASE(NODE_CLOSURE_HOLDOUT)
      svm_node_closure_holdout(sd, stack, closure_weight, node);
      break;
      SVM_CASE(NODE_FRESNEL)
      svm_node_fresnel(sd, stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_LAYER_WEIGHT)
      svm_node_layer_weight(sd, stack, node);
      break;
      SVM_CASE(NODE_CLOSURE_VOLUME)
      IF_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_closure_volume<type>(kg, sd, stack, closure_weight, node);
      }
      break;
      SVM_CASE(NODE_VOLUME_COEFFICIENTS)
      IF_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_volume_coefficients<type>(kg, sd, stack, closure_weight, node, path_flag);
      }
      break;
      SVM_CASE(NODE_PRINCIPLED_VOLUME)
      IF_KERNEL_NODES_FEATURE(VOLUME)
      {
        offset = svm_node_principled_volume<type>(
            kg, sd, stack, closure_weight, node, path_flag, offset);
      }
      break;
      SVM_CASE(NODE_MATH)
      svm_node_math(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_VECTOR_MATH)
      offset = svm_node_vector_math(kg, stack, node.y, node.z, node.w, offset);
      break;
      SVM_CASE(NODE_RGB_RAMP)
      offset = svm_node_rgb_ramp(kg, stack, node, offset);
      break;
      SVM_CASE(NODE_GAMMA)
      svm_node_gamma(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_BRIGHTCONTRAST)
      svm_node_brightness(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_LIGHT_PATH)
      svm_node_light_path<node_feature_mask>(kg, state, sd, stack, node.y, node.z, path_flag);
      break;
      SVM_CASE(NODE_OBJECT_INFO)
      svm_node_object_info(kg, sd, stack, node.y, node.z);
      break;
      SVM_CASE(NODE_PARTICLE_INFO)
      svm_node_particle_info(kg, sd, stack, node.y, node.z);
      break;
#if defined(__HAIR__)
      SVM_CASE(NODE_HAIR_INFO)
      svm_node_hair_info(kg, sd, stack, node.y, node.z);
      break;
#endif
#if defined(__POINTCLOUD__)
      SVM_CASE(NODE_POINT_INFO)
      svm_node_point_info(kg, sd, stack, node.y, node.z);
      break;
#endif
      SVM_CASE(NODE_TEXTURE_MAPPING)
      offset = svm_node_texture_mapping(kg, stack, node.y, node.z, offset);
      break;
      SVM_CASE(NODE_MAPPING)
      svm_node_mapping(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_MIN_MAX)
      offset = svm_node_min_max(kg, stack, node.y, node.z, offset);
      break;
      SVM_CASE(NODE_CAMERA)
      svm_node_camera(kg, sd, stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_TEX_ENVIRONMENT)
      svm_node_tex_environment(kg, sd, stack, node);
      break;
      SVM_CASE(NODE_TEX_SKY)
      offset = svm_node_tex_sky(kg, path_flag, stack, node, offset);
      break;
      SVM_CASE(NODE_TEX_GRADIENT)
      svm_node_tex_gradient(stack, node);
      break;
      SVM_CASE(NODE_TEX_VORONOI)
      offset = svm_node_tex_voronoi<node_feature_mask>(kg, stack, node.y, node.z, node.w, offset);
      break;
      SVM_CASE(NODE_TEX_GABOR)
      offset = svm_node_tex_gabor(kg, stack, node.y, node.z, node.w, offset);
      break;
      SVM_CASE(NODE_TEX_WAVE)
      offset = svm_node_tex_wave(kg, stack, node, offset);
      break;
      SVM_CASE(NODE_TEX_MAGIC)
      offset = svm_node_tex_magic(kg, stack, node, offset);
      break;
      SVM_CASE(NODE_TEX_CHECKER)
      svm_node_tex_checker(stack, node);
      break;
      SVM_CASE(NODE_TEX_BRICK)
      offset = svm_node_tex_brick(kg, stack, node, offset);
      break;
      SVM_CASE(NODE_TEX_WHITE_NOISE)
      svm_node_tex_white_noise(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_NORMAL)
      offset = svm_node_normal(kg, stack, node.y, node.z, node.w, offset);
      break;
      SVM_CASE(NODE_LIGHT_FALLOFF)
      svm_node_light_falloff(sd, stack, node);
      break;
      SVM_CASE(NODE_IES)
      svm_node_ies(kg, sd, stack, node);
      break;
      SVM_CASE(NODE_CURVES)
      offset = svm_node_curves(kg, stack, node, offset);
      break;
      SVM_CASE(NODE_FLOAT_CURVE)
      offset = svm_node_curve(kg, stack, node, offset);
      break;
      SVM_CASE(NODE_TANGENT)
      svm_node_tangent(kg, sd, stack, node);
      break;
      SVM_CASE(NODE_NORMAL_MAP)
      svm_node_normal_map(kg, sd, stack, node);
      break;
      SVM_CASE(NODE_INVERT)
      svm_node_invert(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_MIX)
      offset = svm_node_mix(kg, stack, node.y, node.z, node.w, offset);
      break;
      SVM_CASE(NODE_SEPARATE_COLOR)
      svm_node_separate_color(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_COMBINE_COLOR)
      svm_node_combine_color(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_SEPARATE_VECTOR)
      svm_node_separate_vector(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_COMBINE_VECTOR)
      svm_node_combine_vector(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_VECTOR_ROTATE)
      svm_node_vector_rotate(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_VECTOR_TRANSFORM)
      svm_node_vector_transform(kg, sd, stack, node);
      break;
      SVM_CASE(NODE_WIREFRAME)
      svm_node_wireframe(kg, sd, stack, node);
      break;
      SVM_CASE(NODE_WAVELENGTH)
      svm_node_wavelength(kg, stack, node.y, node.z);
      break;
      SVM_CASE(NODE_BLACKBODY)
      svm_node_blackbody(kg, stack, node.y, node.z);
      break;
      SVM_CASE(NODE_MAP_RANGE)
      offset = svm_node_map_range(kg, stack, node.y, node.z, node.w, offset);
      break;
      SVM_CASE(NODE_VECTOR_MAP_RANGE)
      offset = svm_node_vector_map_range(stack, node.y, node.z, node.w, offset);
      break;
      SVM_CASE(NODE_CLAMP)
      offset = svm_node_clamp(kg, stack, node.y, node.z, node.w, offset);
      break;
#ifdef __SHADER_RAYTRACE__
      SVM_CASE(NODE_BEVEL)
      svm_node_bevel<node_feature_mask>(kg, state, sd, stack, node);
      break;
      SVM_CASE(NODE_AMBIENT_OCCLUSION)
      svm_node_ao<node_feature_mask>(kg, state, sd, stack, node);
      break;
#endif

      SVM_CASE(NODE_AOV_START)
      if (!svm_node_aov_check(path_flag, render_buffer)) {
        return;
      }
      break;
      SVM_CASE(NODE_AOV_COLOR)
      svm_node_aov_color<node_feature_mask>(kg, state, stack, node, render_buffer);
      break;
      SVM_CASE(NODE_AOV_VALUE)
      svm_node_aov_value<node_feature_mask>(kg, state, stack, node, render_buffer);
      break;
      SVM_CASE(NODE_MIX_COLOR)
      svm_node_mix_color(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_MIX_FLOAT)
      svm_node_mix_float(stack, node.y, node.z, node.w);
      break;
      SVM_CASE(NODE_MIX_VECTOR)
      svm_node_mix_vector(stack, node.y, node.z);
      break;
      SVM_CASE(NODE_MIX_VECTOR_NON_UNIFORM)
      svm_node_mix_vector_non_uniform(stack, node.y, node.z);
      break;
      default:
        kernel_assert(!"Unknown node type was passed to the SVM machine");
        return;
    }
  }
}

CCL_NAMESPACE_END
