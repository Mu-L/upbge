/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <gtest/gtest.h>

#include "util/path.h"

CCL_NAMESPACE_BEGIN

/* ******** Tests for path_filename() ******** */

#ifndef _WIN32
TEST(util_path_filename, simple_unix)
{
  const string str = path_filename("/tmp/foo.txt");
  EXPECT_EQ(str, "foo.txt");
}

TEST(util_path_filename, root_unix)
{
  const string str = path_filename("/");
  EXPECT_EQ(str, "/");
}

TEST(util_path_filename, last_slash_unix)
{
  const string str = path_filename("/tmp/foo.txt/");
  EXPECT_EQ(str, ".");
}

TEST(util_path_filename, alternate_slash_unix)
{
  const string str = path_filename("/tmp\\foo.txt");
  EXPECT_EQ(str, "tmp\\foo.txt");
}
#endif /* !_WIN32 */

TEST(util_path_filename, file_only)
{
  const string str = path_filename("foo.txt");
  EXPECT_EQ(str, "foo.txt");
}

TEST(util_path_filename, empty)
{
  const string str = path_filename("");
  EXPECT_EQ(str, "");
}

#ifdef _WIN32
TEST(util_path_filename, simple_windows)
{
  string str = path_filename("C:\\tmp\\foo.txt");
  EXPECT_EQ(str, "foo.txt");
}

TEST(util_path_filename, root_windows)
{
  string str = path_filename("C:\\");
  EXPECT_EQ(str, "\\");
}

TEST(util_path_filename, last_slash_windows)
{
  string str = path_filename("C:\\tmp\\foo.txt\\");
  EXPECT_EQ(str, ".");
}

TEST(util_path_filename, alternate_slash_windows)
{
  string str = path_filename("C:\\tmp/foo.txt");
  EXPECT_EQ(str, "foo.txt");
}
#endif /* _WIN32 */

/* ******** Tests for path_dirname() ******** */

#ifndef _WIN32
TEST(util_path_dirname, simple_unix)
{
  const string str = path_dirname("/tmp/foo.txt");
  EXPECT_EQ(str, "/tmp");
}

TEST(util_path_dirname, root_unix)
{
  const string str = path_dirname("/");
  EXPECT_EQ(str, "");
}

TEST(util_path_dirname, last_slash_unix)
{
  const string str = path_dirname("/tmp/foo.txt/");
  EXPECT_EQ(str, "/tmp/foo.txt");
}

TEST(util_path_dirname, alternate_slash_unix)
{
  const string str = path_dirname("/tmp\\foo.txt");
  EXPECT_EQ(str, "/");
}
#endif /* !_WIN32 */

TEST(util_path_dirname, file_only)
{
  const string str = path_dirname("foo.txt");
  EXPECT_EQ(str, "");
}

TEST(util_path_dirname, empty)
{
  const string str = path_dirname("");
  EXPECT_EQ(str, "");
}

#ifdef _WIN32
TEST(util_path_dirname, simple_windows)
{
  string str = path_dirname("C:\\tmp\\foo.txt");
  EXPECT_EQ(str, "C:\\tmp");
}

TEST(util_path_dirname, root_windows)
{
  string str = path_dirname("C:\\");
  EXPECT_EQ(str, "C:");
}

TEST(util_path_dirname, last_slash_windows)
{
  string str = path_dirname("C:\\tmp\\foo.txt\\");
  EXPECT_EQ(str, "C:\\tmp\\foo.txt");
}

TEST(util_path_dirname, alternate_slash_windows)
{
  string str = path_dirname("C:\\tmp/foo.txt");
  EXPECT_EQ(str, "C:\\tmp");
}
#endif /* _WIN32 */

/* ******** Tests for path_join() ******** */

TEST(util_path_join, empty_both)
{
  const string str = path_join("", "");
  EXPECT_EQ(str, "");
}

TEST(util_path_join, empty_directory)
{
  const string str = path_join("", "foo.txt");
  EXPECT_EQ(str, "foo.txt");
}

TEST(util_path_join, empty_filename)
{
  const string str = path_join("foo", "");
  EXPECT_EQ(str, "foo");
}

#ifndef _WIN32
TEST(util_path_join, simple_unix)
{
  const string str = path_join("foo", "bar");
  EXPECT_EQ(str, "foo/bar");
}

TEST(util_path_join, directory_slash_unix)
{
  const string str = path_join("foo/", "bar");
  EXPECT_EQ(str, "foo/bar");
}

TEST(util_path_join, filename_slash_unix)
{
  const string str = path_join("foo", "/bar");
  EXPECT_EQ(str, "foo/bar");
}

TEST(util_path_join, both_slash_unix)
{
  const string str = path_join("foo/", "/bar");
  EXPECT_EQ(str, "foo//bar");
}

TEST(util_path_join, directory_alternate_slash_unix)
{
  const string str = path_join("foo\\", "bar");
  EXPECT_EQ(str, "foo\\/bar");
}

TEST(util_path_join, filename_alternate_slash_unix)
{
  const string str = path_join("foo", "\\bar");
  EXPECT_EQ(str, "foo/\\bar");
}

TEST(util_path_join, both_alternate_slash_unix)
{
  const string str = path_join("foo", "\\bar");
  EXPECT_EQ(str, "foo/\\bar");
}

TEST(util_path_join, empty_dir_filename_slash_unix)
{
  const string str = path_join("", "/foo.txt");
  EXPECT_EQ(str, "/foo.txt");
}

TEST(util_path_join, empty_dir_filename_alternate_slash_unix)
{
  const string str = path_join("", "\\foo.txt");
  EXPECT_EQ(str, "\\foo.txt");
}

TEST(util_path_join, empty_filename_dir_slash_unix)
{
  const string str = path_join("foo/", "");
  EXPECT_EQ(str, "foo/");
}

TEST(util_path_join, empty_filename_dir_alternate_slash_unix)
{
  const string str = path_join("foo\\", "");
  EXPECT_EQ(str, "foo\\");
}
#else  /* !_WIN32 */
TEST(util_path_join, simple_windows)
{
  string str = path_join("foo", "bar");
  EXPECT_EQ(str, "foo\\bar");
}

TEST(util_path_join, directory_slash_windows)
{
  string str = path_join("foo\\", "bar");
  EXPECT_EQ(str, "foo\\bar");
}

TEST(util_path_join, filename_slash_windows)
{
  string str = path_join("foo", "\\bar");
  EXPECT_EQ(str, "foo\\bar");
}

TEST(util_path_join, both_slash_windows)
{
  string str = path_join("foo\\", "\\bar");
  EXPECT_EQ(str, "foo\\\\bar");
}

TEST(util_path_join, directory_alternate_slash_windows)
{
  string str = path_join("foo/", "bar");
  EXPECT_EQ(str, "foo/bar");
}

TEST(util_path_join, filename_alternate_slash_windows)
{
  string str = path_join("foo", "/bar");
  EXPECT_EQ(str, "foo/bar");
}

TEST(util_path_join, both_alternate_slash_windows)
{
  string str = path_join("foo/", "/bar");
  EXPECT_EQ(str, "foo//bar");
}

TEST(util_path_join, empty_dir_filename_slash_windows)
{
  string str = path_join("", "\\foo.txt");
  EXPECT_EQ(str, "\\foo.txt");
}

TEST(util_path_join, empty_dir_filename_alternate_slash_windows)
{
  string str = path_join("", "/foo.txt");
  EXPECT_EQ(str, "/foo.txt");
}

TEST(util_path_join, empty_filename_dir_slash_windows)
{
  string str = path_join("foo\\", "");
  EXPECT_EQ(str, "foo\\");
}

TEST(util_path_join, empty_filename_dir_alternate_slash_windows)
{
  string str = path_join("foo/", "");
  EXPECT_EQ(str, "foo/");
}
#endif /* !_WIN32 */

/* ******** Tests for path_escape() ******** */

TEST(util_path_escape, no_escape_chars)
{
  const string str = path_escape("/tmp/foo/bar");
  EXPECT_EQ(str, "/tmp/foo/bar");
}

TEST(util_path_escape, simple)
{
  const string str = path_escape("/tmp/foo bar");
  EXPECT_EQ(str, "/tmp/foo\\ bar");
}

TEST(util_path_escape, simple_end)
{
  const string str = path_escape("/tmp/foo/bar ");
  EXPECT_EQ(str, "/tmp/foo/bar\\ ");
}

TEST(util_path_escape, multiple)
{
  const string str = path_escape("/tmp/foo  bar");
  EXPECT_EQ(str, "/tmp/foo\\ \\ bar");
}

TEST(util_path_escape, simple_multiple_end)
{
  const string str = path_escape("/tmp/foo/bar  ");
  EXPECT_EQ(str, "/tmp/foo/bar\\ \\ ");
}

/* ******** Tests for path_is_relative() ******** */

TEST(util_path_is_relative, filename)
{
  const bool is_relative = path_is_relative("foo.txt");
  EXPECT_TRUE(is_relative);
}

#ifndef _WIN32
TEST(util_path_is_relative, absolute_unix)
{
  const bool is_relative = path_is_relative("/tmp/foo.txt");
  EXPECT_FALSE(is_relative);
}

TEST(util_path_is_relative, relative_dir_unix)
{
  const bool is_relative = path_is_relative("tmp/foo.txt");
  EXPECT_TRUE(is_relative);
}

TEST(util_path_is_relative, absolute_windir_on_unix)
{
  const bool is_relative = path_is_relative("C:\\tmp\\foo.txt");
  EXPECT_TRUE(is_relative);
}

TEST(util_path_is_relative, relative_windir_on_unix)
{
  const bool is_relative = path_is_relative("tmp\\foo.txt");
  EXPECT_TRUE(is_relative);
}
#endif /* !_WIN32 */

#ifdef _WIN32
TEST(util_path_is_relative, absolute_windows)
{
  bool is_relative = path_is_relative("C:\\tmp\\foo.txt");
  EXPECT_FALSE(is_relative);
}

TEST(util_path_is_relative, relative_dir_windows)
{
  bool is_relative = path_is_relative("tmp\\foo.txt");
  EXPECT_TRUE(is_relative);
}

TEST(util_path_is_relative, absolute_unixdir_on_windows)
{
  bool is_relative = path_is_relative("/tmp/foo.txt");
  EXPECT_TRUE(is_relative);
}

TEST(util_path_is_relative, relative_unixdir_on_windows)
{
  bool is_relative = path_is_relative("tmp/foo.txt");
  EXPECT_TRUE(is_relative);
}
#endif /* _WIN32 */

CCL_NAMESPACE_END
