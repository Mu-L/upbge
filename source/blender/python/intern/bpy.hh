/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#include <Python.h>

struct bContext;

/** Creates the `bpy` module and adds it to `sys.modules` for importing. */
void BPy_init_modules(bContext *C);
void BPy_end_modules();

extern PyObject *bpy_package_py;
extern PyObject *bpy_sys_module_backup;

/* `bpy_interface_atexit.cc` */

void BPY_atexit_register();
void BPY_atexit_unregister();

extern struct CLG_LogRef *BPY_LOG_RNA;
extern struct CLG_LogRef *BPY_LOG_INTERFACE;
