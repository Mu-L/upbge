/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0D.h"

#include "../../geometry/Geom.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0DVec2f_Type;

#define BPy_UnaryFunction0DVec2f_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0DVec2f_Type))

/*---------------------------Python BPy_UnaryFunction0DVec2f structure definition----------*/
typedef struct {
  BPy_UnaryFunction0D py_uf0D;
  Freestyle::UnaryFunction0D<Freestyle::Geometry::Vec2f> *uf0D_vec2f;
} BPy_UnaryFunction0DVec2f;

/*---------------------------Python BPy_UnaryFunction0DVec2f visible prototypes-----------*/
int UnaryFunction0DVec2f_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////
