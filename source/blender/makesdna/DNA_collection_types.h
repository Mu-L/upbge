/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * \brief Object groups, one object can be in many groups at once.
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
namespace blender::bke {
struct CollectionRuntime;
}  // namespace blender::bke
using CollectionRuntimeHandle = blender::bke::CollectionRuntime;
#else
typedef struct CollectionRuntimeHandle CollectionRuntimeHandle;
#endif

struct Collection;
struct Object;
struct GHash;

/* Light linking relation of a collection or an object. */
typedef struct CollectionLightLinking {
  /* Light and shadow linking configuration, an enumerator of eCollectionLightLinkingState.
   * The meaning depends on whether the collection is specified as a light or shadow linking on the
   * Object's LightLinking.
   *
   * For the light linking collection:
   *
   *   - INCLUDE: the receiver is included into the light linking and is only receiving lights from
   *     emitters which include it in their light linking collections. The receiver is not affected
   *     by regular scene lights.
   *
   *   - EXCLUDE: the receiver does not receive light from this emitter, but is lit by regular
   *     lights in the scene or by emitters which are linked to it via INCLUDE on their
   *     light_state.
   *
   * For the shadow linking collection:
   *
   *   - INCLUDE: the collection or object casts shadows from the emitter. It does not cast shadow
   *     from light sources which do not have INCLUDE on their light linking configuration for it.
   *
   *   - EXCLUDE: the collection or object does not cast shadow when lit by this emitter, but does
   *     for other light sources in the scene. */
  uint8_t link_state;

  uint8_t _pad[3];
} CollectionLightLinking;

typedef struct CollectionObject {
  struct CollectionObject *next, *prev;
  struct Object *ob;

  CollectionLightLinking light_linking;
  int _pad;
} CollectionObject;

typedef struct CollectionChild {
  struct CollectionChild *next, *prev;
  struct Collection *collection;

  CollectionLightLinking light_linking;
  int _pad;
} CollectionChild;

/* Collection IO property storage and access. */
typedef struct CollectionExport {
  struct CollectionExport *next, *prev;

  /** Identifier that matches the #FileHandlerType.idname. */
  char fh_idname[64];
  char name[64];

  IDProperty *export_properties;
  uint32_t flag;

  uint32_t _pad0;
} CollectionExport;

typedef enum IOHandlerPanelFlag {
  IO_HANDLER_PANEL_OPEN = 1 << 0,
} IOHandlerPanelFlag;

/* Light linking state of object or collection: defines how they react to the emitters in the
 * scene. See the comment for the link_state in the CollectionLightLinking for the details. */
typedef enum eCollectionLightLinkingState {
  COLLECTION_LIGHT_LINKING_STATE_INCLUDE = 0,
  COLLECTION_LIGHT_LINKING_STATE_EXCLUDE = 1,
} eCollectionLightLinkingState;

enum eCollectionLineArt_Usage {
  COLLECTION_LRT_INCLUDE = 0,
  COLLECTION_LRT_OCCLUSION_ONLY = (1 << 0),
  COLLECTION_LRT_EXCLUDE = (1 << 1),
  COLLECTION_LRT_INTERSECTION_ONLY = (1 << 2),
  COLLECTION_LRT_NO_INTERSECTION = (1 << 3),
  COLLECTION_LRT_FORCE_INTERSECTION = (1 << 4),
};

enum eCollectionLineArt_Flags {
  COLLECTION_LRT_USE_INTERSECTION_MASK = (1 << 0),
  COLLECTION_LRT_USE_INTERSECTION_PRIORITY = (1 << 1),
};

typedef struct Collection {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_GR;
#endif

  ID id;

  /** The ID owning this collection, in case it is an embedded one. */
  ID *owner_id;

  /** CollectionObject. */
  ListBase gobject;
  /** CollectionChild. */
  ListBase children;

  char _pad0[4];

  int active_exporter_index;
  ListBase exporters;

  struct PreviewImage *preview;

  unsigned int layer DNA_DEPRECATED;
  float instance_offset[3];

  uint16_t flag;
  int8_t color_tag;

  char _pad1[1];

  uint8_t lineart_usage; /* #eCollectionLineArt_Usage */
  uint8_t lineart_flags; /* #eCollectionLineArt_Flags */
  uint8_t lineart_intersection_mask;
  uint8_t lineart_intersection_priority;

  struct ViewLayer *view_layer DNA_DEPRECATED;

  /* Keep last. */
  CollectionRuntimeHandle *runtime;
} Collection;

/** #Collection.flag */
enum {
  /** Disable in viewports. */
  COLLECTION_HIDE_VIEWPORT = (1 << 0),
  /** Not selectable in viewport. */
  COLLECTION_HIDE_SELECT = (1 << 1),
  // COLLECTION_DISABLED_DEPRECATED = (1 << 2), /* DIRTY */
  /** Disable in renders. */
  COLLECTION_HIDE_RENDER = (1 << 3),
  /** Runtime: object_cache is populated. */
  COLLECTION_HAS_OBJECT_CACHE = (1 << 4),
  /** Is master collection embedded in the scene. */
  COLLECTION_IS_MASTER = (1 << 5),
  /** for object_cache_instanced. */
  COLLECTION_HAS_OBJECT_CACHE_INSTANCED = (1 << 6),
  /** UPBGE to alternate between old spawn method */
  COLLECTION_IS_SPAWNED = (1 << 14),
};

#define COLLECTION_FLAG_ALL_RUNTIME \
  (COLLECTION_HAS_OBJECT_CACHE | COLLECTION_HAS_OBJECT_CACHE_INSTANCED)

/** #Collection.color_tag */
typedef enum CollectionColorTag {
  COLLECTION_COLOR_NONE = -1,
  COLLECTION_COLOR_01,
  COLLECTION_COLOR_02,
  COLLECTION_COLOR_03,
  COLLECTION_COLOR_04,
  COLLECTION_COLOR_05,
  COLLECTION_COLOR_06,
  COLLECTION_COLOR_07,
  COLLECTION_COLOR_08,

  COLLECTION_COLOR_TOT,
} CollectionColorTag;
