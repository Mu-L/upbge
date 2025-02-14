/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include "BlenderContext.h"
#include "collada_utils.h"

class DocumentExporter {
 public:
  DocumentExporter(BlenderContext &blender_context, ExportSettings *export_settings);
  int exportCurrentScene();

 private:
  BlenderContext &blender_context;
  BCExportSettings export_settings;
  KeyImageMap key_image_map;
};
