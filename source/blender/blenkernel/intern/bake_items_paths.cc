/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_bake_items_paths.hh"

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"

namespace blender::bke::bake {

std::string frame_to_file_name(const SubFrame &frame)
{
  char file_name_c[FILE_MAX];
  SNPRINTF(file_name_c, "%011.5f", double(frame));
  BLI_string_replace_char(file_name_c, '.', '_');
  return file_name_c;
}

std::optional<SubFrame> file_name_to_frame(const StringRef file_name)
{
  char modified_file_name[FILE_MAX];
  file_name.copy_utf8_truncated(modified_file_name);
  BLI_string_replace_char(modified_file_name, '_', '.');
  try {
    const SubFrame frame = std::stof(modified_file_name);
    return frame;
  }
  catch (...) {
    return std::nullopt;
  }
}

Vector<MetaFile> find_sorted_meta_files(const StringRefNull meta_dir)
{
  if (!BLI_is_dir(meta_dir.c_str())) {
    return {};
  }

  direntry *dir_entries = nullptr;
  const int dir_entries_num = BLI_filelist_dir_contents(meta_dir.c_str(), &dir_entries);
  BLI_SCOPED_DEFER([&]() { BLI_filelist_free(dir_entries, dir_entries_num); });

  Vector<MetaFile> meta_files;
  for (const int i : IndexRange(dir_entries_num)) {
    const direntry &dir_entry = dir_entries[i];
    const StringRefNull dir_entry_path = dir_entry.path;
    if (!dir_entry_path.endswith(".json")) {
      continue;
    }
    const std::optional<SubFrame> frame = file_name_to_frame(dir_entry.relname);
    if (!frame) {
      continue;
    }
    meta_files.append({*frame, dir_entry_path});
  }

  std::sort(meta_files.begin(), meta_files.end(), [](const MetaFile &a, const MetaFile &b) {
    return a.frame < b.frame;
  });

  return meta_files;
}

BakePath BakePath::from_single_root(StringRefNull root_dir)
{
  char meta_dir[FILE_MAX];
  BLI_path_join(meta_dir, sizeof(meta_dir), root_dir.c_str(), "meta");
  char blobs_dir[FILE_MAX];
  BLI_path_join(blobs_dir, sizeof(blobs_dir), root_dir.c_str(), "blobs");

  BakePath bake_path;
  bake_path.meta_dir = meta_dir;
  bake_path.blobs_dir = blobs_dir;
  bake_path.bake_dir = root_dir;
  return bake_path;
}

}  // namespace blender::bke::bake
