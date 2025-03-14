/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Engine for drawing a selection map where the pixels indicate the selection indices.
 */

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "BLT_translation.hh"

#include "draw_manager.hh"
#include "overlay_next_instance.hh"

#include "overlay_engine.h"
#include "overlay_next_private.hh"

using namespace blender::draw;

using Instance = blender::draw::overlay::Instance;

/* -------------------------------------------------------------------- */
/** \name Engine Instance
 * \{ */

static void OVERLAY_next_engine_init(void *vedata)
{
  OVERLAY_Data *ved = reinterpret_cast<OVERLAY_Data *>(vedata);

  if (ved->instance == nullptr) {
    ved->instance = new Instance(select::SelectionType::DISABLED);
  }
  reinterpret_cast<Instance *>(ved->instance)->init();
}

static void OVERLAY_next_cache_init(void *vedata)
{
  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)->begin_sync();
}

static void OVERLAY_next_cache_populate(void *vedata, blender::draw::ObjectRef &ob_ref)
{
  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)
      ->object_sync(ob_ref, *DRW_manager_get());
}

static void OVERLAY_next_cache_finish(void *vedata)
{
  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)->end_sync();
}

static void OVERLAY_next_draw_scene(void *vedata)
{
  DRW_submission_start();
  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)
      ->draw(*DRW_manager_get());
  DRW_submission_end();
}

static void OVERLAY_next_instance_free(void *instance_)
{
  Instance *instance = (Instance *)instance_;
  delete instance;
}

static void OVERLAY_next_engine_free()
{
  overlay::ShaderModule::module_free();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Type
 * \{ */

DrawEngineType draw_engine_overlay_next_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("Overlay"),
    /*engine_init*/ &OVERLAY_next_engine_init,
    /*engine_free*/ &OVERLAY_next_engine_free,
    /*instance_free*/ &OVERLAY_next_instance_free,
    /*cache_init*/ &OVERLAY_next_cache_init,
    /*cache_populate*/ &OVERLAY_next_cache_populate,
    /*cache_finish*/ &OVERLAY_next_cache_finish,
    /*draw_scene*/ &OVERLAY_next_draw_scene,
    /*render_to_image*/ nullptr,
    /*store_metadata*/ nullptr,
};

/** \} */
