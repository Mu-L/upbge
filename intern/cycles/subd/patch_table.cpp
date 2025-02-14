/* SPDX-FileCopyrightText: 2014 DreamWorks Animation LLC
 *
 * SPDX-License-Identifier: Apache-2.0 */

/** \file
 * Based on code from OpenSubdiv.
 */

#include "subd/patch_table.h"
#include "kernel/types.h"

#include "util/math.h"

#ifdef WITH_OPENSUBDIV
#  include <opensubdiv/far/patchTable.h>
#endif

CCL_NAMESPACE_BEGIN

#ifdef WITH_OPENSUBDIV

using namespace OpenSubdiv;

/* functions for building patch maps */

struct PatchMapQuadNode {
  /* sets all the children to point to the patch of index */
  void set_child(const int index)
  {
    for (int i = 0; i < 4; i++) {
      children[i] = index | PATCH_MAP_NODE_IS_SET | PATCH_MAP_NODE_IS_LEAF;
    }
  }

  /* sets the child in quadrant to point to the node or patch of the given index */
  void set_child(unsigned char quadrant, const int index, bool is_leaf = true)
  {
    assert(quadrant < 4);
    children[quadrant] = index | PATCH_MAP_NODE_IS_SET | (is_leaf ? PATCH_MAP_NODE_IS_LEAF : 0);
  }

  uint children[4];
};

template<class T> static int resolve_quadrant(T &median, T &u, T &v)
{
  int quadrant = -1;

  if (u < median) {
    if (v < median) {
      quadrant = 0;
    }
    else {
      quadrant = 1;
      v -= median;
    }
  }
  else {
    if (v < median) {
      quadrant = 3;
    }
    else {
      quadrant = 2;
      v -= median;
    }
    u -= median;
  }

  return quadrant;
}

static void build_patch_map(PackedPatchTable &table,
                            OpenSubdiv::Far::PatchTable *patch_table,
                            int offset)
{
  int num_faces = 0;

  for (int array = 0; array < table.num_arrays; array++) {
    const Far::ConstPatchParamArray params = patch_table->GetPatchParams(array);

    for (int j = 0; j < patch_table->GetNumPatches(array); j++) {
      num_faces = max(num_faces, (int)params[j].GetFaceId());
    }
  }
  num_faces++;

  vector<PatchMapQuadNode> quadtree;
  quadtree.reserve(num_faces + table.num_patches);
  quadtree.resize(num_faces);

  /* adjust offsets to make indices relative to the table */
  int handle_index = -(table.num_patches * PATCH_HANDLE_SIZE);
  offset += table.total_size();

  /* populate the quadtree from the FarPatchArrays sub-patches */
  for (int array = 0; array < table.num_arrays; array++) {
    const Far::ConstPatchParamArray params = patch_table->GetPatchParams(array);

    for (int i = 0; i < patch_table->GetNumPatches(array); i++, handle_index += PATCH_HANDLE_SIZE)
    {
      const Far::PatchParam &param = params[i];
      const unsigned short depth = param.GetDepth();

      PatchMapQuadNode *node = &quadtree[params[i].GetFaceId()];

      if (depth == (param.NonQuadRoot() ? 1 : 0)) {
        /* special case : regular BSpline face w/ no sub-patches */
        node->set_child(handle_index + offset);
        continue;
      }

      int u = param.GetU();
      int v = param.GetV();
      const int pdepth = param.NonQuadRoot() ? depth - 2 : depth - 1;
      int half = 1 << pdepth;

      for (int j = 0; j < depth; j++) {
        const int delta = half >> 1;

        const int quadrant = resolve_quadrant(half, u, v);
        assert(quadrant >= 0);

        half = delta;

        if (j == pdepth) {
          /* we have reached the depth of the sub-patch : add a leaf */
          assert(!(node->children[quadrant] & PATCH_MAP_NODE_IS_SET));
          node->set_child(quadrant, handle_index + offset, true);
          break;
        }
        /* travel down the child node of the corresponding quadrant */
        if (!(node->children[quadrant] & PATCH_MAP_NODE_IS_SET)) {
          /* create a new branch in the quadrant */
          quadtree.push_back(PatchMapQuadNode());

          const int idx = (int)quadtree.size() - 1;
          node->set_child(quadrant, idx * 4 + offset, false);

          node = &quadtree[idx];
        }
        else {
          /* travel down an existing branch */
          const uint idx = node->children[quadrant] & PATCH_MAP_NODE_INDEX_MASK;
          node = &(quadtree[(idx - offset) / 4]);
        }
      }
    }
  }

  /* copy into table */
  assert(table.table.size() == table.total_size());
  const uint map_offset = table.total_size();

  table.num_nodes = quadtree.size() * 4;
  table.table.resize(table.total_size());

  uint *data = &table.table[map_offset];

  for (int i = 0; i < quadtree.size(); i++) {
    for (int j = 0; j < 4; j++) {
      assert(quadtree[i].children[j] & PATCH_MAP_NODE_IS_SET);
      *(data++) = quadtree[i].children[j];
    }
  }
}

#endif

/* packed patch table functions */

size_t PackedPatchTable::total_size()
{
  return num_arrays * PATCH_ARRAY_SIZE + num_indices +
         num_patches * (PATCH_PARAM_SIZE + PATCH_HANDLE_SIZE) + num_nodes * PATCH_NODE_SIZE;
}

void PackedPatchTable::pack(Far::PatchTable *patch_table, const int offset)
{
  num_arrays = 0;
  num_patches = 0;
  num_indices = 0;
  num_nodes = 0;

#ifdef WITH_OPENSUBDIV
  num_arrays = patch_table->GetNumPatchArrays();

  for (int i = 0; i < num_arrays; i++) {
    const int patches = patch_table->GetNumPatches(i);
    const int num_control = patch_table->GetPatchArrayDescriptor(i).GetNumControlVertices();

    num_patches += patches;
    num_indices += patches * num_control;
  }

  table.resize(total_size());
  uint *data = table.data();

  uint *array = data;
  uint *index = array + num_arrays * PATCH_ARRAY_SIZE;
  uint *param = index + num_indices;
  uint *handle = param + num_patches * PATCH_PARAM_SIZE;

  uint current_param = 0;

  for (int i = 0; i < num_arrays; i++) {
    *(array++) = patch_table->GetPatchArrayDescriptor(i).GetType();
    *(array++) = patch_table->GetNumPatches(i);
    *(array++) = (index - data) + offset;
    *(array++) = (param - data) + offset;

    const Far::ConstIndexArray indices = patch_table->GetPatchArrayVertices(i);

    for (int j = 0; j < indices.size(); j++) {
      *(index++) = indices[j];
    }

    const Far::PatchParamTable &param_table = patch_table->GetPatchParamTable();

    const int num_control = patch_table->GetPatchArrayDescriptor(i).GetNumControlVertices();
    const int patches = patch_table->GetNumPatches(i);

    for (int j = 0; j < patches; j++, current_param++) {
      *(param++) = param_table[current_param].field0;
      *(param++) = param_table[current_param].field1;

      *(handle++) = (array - data) - PATCH_ARRAY_SIZE + offset;
      *(handle++) = (param - data) - PATCH_PARAM_SIZE + offset;
      *(handle++) = j * num_control;
    }
  }

  build_patch_map(*this, patch_table, offset);
#else
  (void)patch_table;
  (void)offset;
#endif
}

void PackedPatchTable::copy_adjusting_offsets(uint *dest, const int doffset)
{
  uint *src = table.data();

  /* arrays */
  for (int i = 0; i < num_arrays; i++) {
    *(dest++) = *(src++);
    *(dest++) = *(src++);
    *(dest++) = *(src++) + doffset;
    *(dest++) = *(src++) + doffset;
  }

  /* indices */
  for (int i = 0; i < num_indices; i++) {
    *(dest++) = *(src++);
  }

  /* params */
  for (int i = 0; i < num_patches; i++) {
    *(dest++) = *(src++);
    *(dest++) = *(src++);
  }

  /* handles */
  for (int i = 0; i < num_patches; i++) {
    *(dest++) = *(src++) + doffset;
    *(dest++) = *(src++) + doffset;
    *(dest++) = *(src++);
  }

  /* nodes */
  for (int i = 0; i < num_nodes; i++) {
    *(dest++) = *(src++) + doffset;
  }
}

CCL_NAMESPACE_END
