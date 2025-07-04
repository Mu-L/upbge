/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_nodes.h"

#include "DNA_scene_types.h"

#include "BLI_listbase.h"

namespace blender::deg {

void DepsgraphNodeBuilder::build_scene_render(Scene *scene, ViewLayer *view_layer)
{
  scene_ = scene;
  view_layer_ = view_layer;
  const bool build_compositor = (scene->r.scemode & R_DOCOMP);
  const bool build_sequencer = (scene->r.scemode & R_DOSEQ);
  IDNode *id_node = add_id_node(&scene->id);
  id_node->linked_state = DEG_ID_LINKED_DIRECTLY;
  add_time_source();
  build_animdata(&scene->id);
  build_scene_parameters(scene);
  build_scene_audio(scene);
  if (build_compositor) {
    build_scene_compositor(scene);
  }
  if (build_sequencer) {
    build_scene_sequencer(scene);
    build_scene_speakers(scene, view_layer);
  }
  build_scene_camera(scene);
}

void DepsgraphNodeBuilder::build_scene_camera(Scene *scene)
{
  if (scene->camera != nullptr) {
    build_object(-1, scene->camera, DEG_ID_LINKED_INDIRECTLY, true);
  }
  LISTBASE_FOREACH (TimeMarker *, marker, &scene->markers) {
    if (!ELEM(marker->camera, nullptr, scene->camera)) {
      build_object(-1, marker->camera, DEG_ID_LINKED_INDIRECTLY, true);
    }
  }
}

void DepsgraphNodeBuilder::build_scene_parameters(Scene *scene)
{
  if (built_map_.check_is_built_and_tag(scene, BuilderMap::TAG_PARAMETERS)) {
    return;
  }
  build_parameters(&scene->id);
  build_idproperties(scene->id.properties);
  build_idproperties(scene->id.system_properties);

  add_operation_node(&scene->id, NodeType::SCENE, OperationCode::SCENE_EVAL);

  /* NOTE: This is a bit overkill and can potentially pull a bit too much into the graph, but:
   *
   * - We definitely need an ID node for the scene's compositor, otherwise re-mapping will no
   *   happen correct and we will risk remapping pointers in the main database.
   * - Alternatively, we should discard compositor tree, but this might cause other headache like
   *   drivers which are coming from the tree.
   *
   * Would be nice to find some reliable way of ignoring compositor here, but it's already pulled
   * in when building scene from view layer, so this particular case does not make things
   * marginally worse. */
  build_scene_compositor(scene);

  LISTBASE_FOREACH (TimeMarker *, marker, &scene->markers) {
    build_idproperties(marker->prop);
  }
}

void DepsgraphNodeBuilder::build_scene_compositor(Scene *scene)
{
  if (built_map_.check_is_built_and_tag(scene, BuilderMap::TAG_SCENE_COMPOSITOR)) {
    return;
  }
  if (scene->compositing_node_group == nullptr) {
    return;
  }
  build_nodetree(scene->compositing_node_group);
}

}  // namespace blender::deg
