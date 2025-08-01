/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pymathutils
 */

#include <algorithm>

#include <Python.h>

#include "mathutils.hh"

#include "BLI_math_base_safe.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_utildefines.hh"

#ifndef MATH_STANDALONE
#  include "BLI_dynstr.h"
#endif

#define QUAT_SIZE 4

static PyObject *quat__apply_to_copy(PyObject *(*quat_func)(QuaternionObject *),
                                     QuaternionObject *self);
static void quat__axis_angle_sanitize(float axis[3], float *angle);
static PyObject *Quaternion_copy(QuaternionObject *self);
static PyObject *Quaternion_deepcopy(QuaternionObject *self, PyObject *args);

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

static PyObject *quat__apply_to_copy(PyObject *(*quat_func)(QuaternionObject *),
                                     QuaternionObject *self)
{
  PyObject *ret = Quaternion_copy(self);
  PyObject *ret_dummy = quat_func((QuaternionObject *)ret);
  if (ret_dummy) {
    Py_DECREF(ret_dummy);
    return ret;
  }
  /* error */
  Py_DECREF(ret);
  return nullptr;
}

/** Axis vector suffers from precision errors, use this function to ensure. */
static void quat__axis_angle_sanitize(float axis[3], float *angle)
{
  if (axis) {
    if (is_zero_v3(axis) || !isfinite(axis[0]) || !isfinite(axis[1]) || !isfinite(axis[2])) {
      axis[0] = 1.0f;
      axis[1] = 0.0f;
      axis[2] = 0.0f;
    }
    else if (EXPP_FloatsAreEqual(axis[0], 0.0f, 10) && EXPP_FloatsAreEqual(axis[1], 0.0f, 10) &&
             EXPP_FloatsAreEqual(axis[2], 0.0f, 10))
    {
      axis[0] = 1.0f;
    }
  }

  if (angle) {
    if (!isfinite(*angle)) {
      *angle = 0.0f;
    }
  }
}

/**
 * \note #BaseMath_ReadCallback must be called beforehand.
 */
static PyObject *Quaternion_to_tuple_ext(QuaternionObject *self, int ndigits)
{
  PyObject *ret;
  int i;

  ret = PyTuple_New(QUAT_SIZE);

  if (ndigits >= 0) {
    for (i = 0; i < QUAT_SIZE; i++) {
      PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(double_round(double(self->quat[i]), ndigits)));
    }
  }
  else {
    for (i = 0; i < QUAT_SIZE; i++) {
      PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(self->quat[i]));
    }
  }

  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: `__new__` / `mathutils.Quaternion()`
 * \{ */

static PyObject *Quaternion_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PyObject *seq = nullptr;
  double angle = 0.0f;
  float quat[QUAT_SIZE];
  unit_qt(quat);

  if (kwds && PyDict_Size(kwds)) {
    PyErr_SetString(PyExc_TypeError,
                    "mathutils.Quaternion(): "
                    "takes no keyword args");
    return nullptr;
  }

  if (!PyArg_ParseTuple(args, "|Od:mathutils.Quaternion", &seq, &angle)) {
    return nullptr;
  }

  switch (PyTuple_GET_SIZE(args)) {
    case 0:
      break;
    case 1: {
      int size;

      if ((size = mathutils_array_parse(quat, 3, QUAT_SIZE, seq, "mathutils.Quaternion()")) == -1)
      {
        return nullptr;
      }

      if (size == 4) {
        /* 4d: Quaternion (common case) */
      }
      else {
        /* 3d: Interpret as exponential map */
        BLI_assert(size == 3);
        expmap_to_quat(quat, quat);
      }

      break;
    }
    case 2: {
      float axis[3];
      if (mathutils_array_parse(axis, 3, 3, seq, "mathutils.Quaternion()") == -1) {
        return nullptr;
      }
      angle = angle_wrap_rad(angle); /* clamp because of precision issues */
      axis_angle_to_quat(quat, axis, angle);
      break;
      /* PyArg_ParseTuple assures no more than 2 */
    }
  }
  return Quaternion_CreatePyObject(quat, type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: To Euler
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_to_euler_doc,
    ".. method:: to_euler(order='XYZ', euler_compat=None, /)\n"
    "\n"
    "   Return Euler representation of the quaternion.\n"
    "\n"
    "   :arg order: Rotation order.\n"
    "   :type order: Literal['XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX']\n"
    "   :arg euler_compat: Optional euler argument the new euler will be made\n"
    "      compatible with (no axis flipping between them).\n"
    "      Useful for converting a series of matrices to animation curves.\n"
    "   :type euler_compat: :class:`Euler`\n"
    "   :return: Euler representation of the quaternion.\n"
    "   :rtype: :class:`Euler`\n");
static PyObject *Quaternion_to_euler(QuaternionObject *self, PyObject *args)
{
  float tquat[4];
  float eul[3];
  const char *order_str = nullptr;
  short order = EULER_ORDER_XYZ;
  EulerObject *eul_compat = nullptr;

  if (!PyArg_ParseTuple(args, "|sO!:to_euler", &order_str, &euler_Type, &eul_compat)) {
    return nullptr;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (order_str) {
    order = euler_order_from_string(order_str, "Quaternion.to_euler()");

    if (order == -1) {
      return nullptr;
    }
  }

  normalize_qt_qt(tquat, self->quat);

  if (eul_compat) {
    if (BaseMath_ReadCallback(eul_compat) == -1) {
      return nullptr;
    }

    if (order == EULER_ORDER_XYZ) {
      quat_to_compatible_eul(eul, eul_compat->eul, tquat);
    }
    else {
      quat_to_compatible_eulO(eul, eul_compat->eul, order, tquat);
    }
  }
  else {
    if (order == EULER_ORDER_XYZ) {
      quat_to_eul(eul, tquat);
    }
    else {
      quat_to_eulO(eul, order, tquat);
    }
  }

  return Euler_CreatePyObject(eul, order, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: To Matrix
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_to_matrix_doc,
    ".. method:: to_matrix()\n"
    "\n"
    "   Return a matrix representation of the quaternion.\n"
    "\n"
    "   :return: A 3x3 rotation matrix representation of the quaternion.\n"
    "   :rtype: :class:`Matrix`\n");
static PyObject *Quaternion_to_matrix(QuaternionObject *self)
{
  float mat[9]; /* all values are set */

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  quat_to_mat3((float(*)[3])mat, self->quat);
  return Matrix_CreatePyObject(mat, 3, 3, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: To Axis/Angle
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_to_axis_angle_doc,
    ".. method:: to_axis_angle()\n"
    "\n"
    "   Return the axis, angle representation of the quaternion.\n"
    "\n"
    "   :return: Axis, angle.\n"
    "   :rtype: tuple[:class:`Vector`, float]\n");
static PyObject *Quaternion_to_axis_angle(QuaternionObject *self)
{
  PyObject *ret;

  float tquat[4];

  float axis[3];
  float angle;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  normalize_qt_qt(tquat, self->quat);
  quat_to_axis_angle(axis, &angle, tquat);

  quat__axis_angle_sanitize(axis, &angle);

  ret = PyTuple_New(2);
  PyTuple_SET_ITEMS(ret, Vector_CreatePyObject(axis, 3, nullptr), PyFloat_FromDouble(angle));
  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: To Swing/Twist
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_to_swing_twist_doc,
    ".. method:: to_swing_twist(axis, /)\n"
    "\n"
    "   Split the rotation into a swing quaternion with the specified\n"
    "   axis fixed at zero, and the remaining twist rotation angle.\n"
    "\n"
    "   :arg axis: Twist axis as a string.\n"
    "   :type axis: Literal['X', 'Y', 'Z']\n"
    "   :return: Swing, twist angle.\n"
    "   :rtype: tuple[:class:`Quaternion`, float]\n");
static PyObject *Quaternion_to_swing_twist(QuaternionObject *self, PyObject *axis_arg)
{
  PyObject *ret;

  const char *axis_str = nullptr;
  float swing[4], twist;
  int axis;

  if (axis_arg && PyUnicode_Check(axis_arg)) {
    axis_str = PyUnicode_AsUTF8(axis_arg);
  }

  if (axis_str && axis_str[0] >= 'X' && axis_str[0] <= 'Z' && axis_str[1] == 0) {
    axis = axis_str[0] - 'X';
  }
  else {
    PyErr_SetString(PyExc_ValueError,
                    "Quaternion.to_swing_twist(): "
                    "the axis argument must be "
                    "a string in 'X', 'Y', 'Z'");
    return nullptr;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  twist = quat_split_swing_and_twist(self->quat, axis, swing, nullptr);

  ret = PyTuple_New(2);
  PyTuple_SET_ITEMS(
      ret, Quaternion_CreatePyObject(swing, Py_TYPE(self)), PyFloat_FromDouble(twist));
  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: To Exponential Map
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_to_exponential_map_doc,
    ".. method:: to_exponential_map()\n"
    "\n"
    "   Return the exponential map representation of the quaternion.\n"
    "\n"
    "   This representation consist of the rotation axis multiplied by the rotation angle.\n"
    "   Such a representation is useful for interpolation between multiple orientations.\n"
    "\n"
    "   :return: exponential map.\n"
    "   :rtype: :class:`Vector` of size 3\n"
    "\n"
    "   To convert back to a quaternion, pass it to the :class:`Quaternion` constructor.\n");
static PyObject *Quaternion_to_exponential_map(QuaternionObject *self)
{
  float expmap[3];

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  quat_to_expmap(expmap, self->quat);
  return Vector_CreatePyObject(expmap, 3, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: Cross Product
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_cross_doc,
    ".. method:: cross(other, /)\n"
    "\n"
    "   Return the cross product of this quaternion and another.\n"
    "\n"
    "   :arg other: The other quaternion to perform the cross product with.\n"
    "   :type other: :class:`Quaternion`\n"
    "   :return: The cross product.\n"
    "   :rtype: :class:`Quaternion`\n");
static PyObject *Quaternion_cross(QuaternionObject *self, PyObject *value)
{
  float quat[QUAT_SIZE], tquat[QUAT_SIZE];

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (mathutils_array_parse(
          tquat, QUAT_SIZE, QUAT_SIZE, value, "Quaternion.cross(other), invalid 'other' arg") ==
      -1)
  {
    return nullptr;
  }

  mul_qt_qtqt(quat, self->quat, tquat);
  return Quaternion_CreatePyObject(quat, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: Dot Product
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_dot_doc,
    ".. method:: dot(other, /)\n"
    "\n"
    "   Return the dot product of this quaternion and another.\n"
    "\n"
    "   :arg other: The other quaternion to perform the dot product with.\n"
    "   :type other: :class:`Quaternion`\n"
    "   :return: The dot product.\n"
    "   :rtype: float\n");
static PyObject *Quaternion_dot(QuaternionObject *self, PyObject *value)
{
  float tquat[QUAT_SIZE];

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (mathutils_array_parse(
          tquat, QUAT_SIZE, QUAT_SIZE, value, "Quaternion.dot(other), invalid 'other' arg") == -1)
  {
    return nullptr;
  }

  return PyFloat_FromDouble(dot_qtqt(self->quat, tquat));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: Rotation Difference
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_rotation_difference_doc,
    ".. function:: rotation_difference(other, /)\n"
    "\n"
    "   Returns a quaternion representing the rotational difference.\n"
    "\n"
    "   :arg other: second quaternion.\n"
    "   :type other: :class:`Quaternion`\n"
    "   :return: the rotational difference between the two quat rotations.\n"
    "   :rtype: :class:`Quaternion`\n");
static PyObject *Quaternion_rotation_difference(QuaternionObject *self, PyObject *value)
{
  float tquat[QUAT_SIZE], quat[QUAT_SIZE];

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (mathutils_array_parse(tquat,
                            QUAT_SIZE,
                            QUAT_SIZE,
                            value,
                            "Quaternion.rotation_difference(other), invalid 'other' arg") == -1)
  {
    return nullptr;
  }

  rotation_between_quats_to_quat(quat, self->quat, tquat);

  return Quaternion_CreatePyObject(quat, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: Spherical Interpolation (slerp)
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_slerp_doc,
    ".. function:: slerp(other, factor, /)\n"
    "\n"
    "   Returns the interpolation of two quaternions.\n"
    "\n"
    "   :arg other: value to interpolate with.\n"
    "   :type other: :class:`Quaternion`\n"
    "   :arg factor: The interpolation value in [0.0, 1.0].\n"
    "   :type factor: float\n"
    "   :return: The interpolated rotation.\n"
    "   :rtype: :class:`Quaternion`\n");
static PyObject *Quaternion_slerp(QuaternionObject *self, PyObject *args)
{
  PyObject *value;
  float tquat[QUAT_SIZE], quat[QUAT_SIZE], fac;

  if (!PyArg_ParseTuple(args, "Of:slerp", &value, &fac)) {
    PyErr_SetString(PyExc_TypeError,
                    "quat.slerp(): "
                    "expected Quaternion types and float");
    return nullptr;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (mathutils_array_parse(
          tquat, QUAT_SIZE, QUAT_SIZE, value, "Quaternion.slerp(other), invalid 'other' arg") ==
      -1)
  {
    return nullptr;
  }

  if (fac > 1.0f || fac < 0.0f) {
    PyErr_SetString(PyExc_ValueError,
                    "quat.slerp(): "
                    "interpolation factor must be between 0.0 and 1.0");
    return nullptr;
  }

  interp_qt_qtqt(quat, self->quat, tquat, fac);

  return Quaternion_CreatePyObject(quat, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: Rotate
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_rotate_doc,
    ".. method:: rotate(other, /)\n"
    "\n"
    "   Rotates the quaternion by another mathutils value.\n"
    "\n"
    "   :arg other: rotation component of mathutils value\n"
    "   :type other: :class:`Euler` | :class:`Quaternion` | :class:`Matrix`\n");
static PyObject *Quaternion_rotate(QuaternionObject *self, PyObject *value)
{
  float self_rmat[3][3], other_rmat[3][3], rmat[3][3];
  float tquat[4], length;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  if (mathutils_any_to_rotmat(other_rmat, value, "Quaternion.rotate(value)") == -1) {
    return nullptr;
  }

  length = normalize_qt_qt(tquat, self->quat);
  quat_to_mat3(self_rmat, tquat);
  mul_m3_m3m3(rmat, other_rmat, self_rmat);
  mat3_to_quat(self->quat, rmat);
  mul_qt_fl(self->quat, length); /* maintain length after rotating */

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_make_compatible_doc,
    ".. method:: make_compatible(other, /)\n"
    "\n"
    "   Make this quaternion compatible with another,\n"
    "   so interpolating between them works as intended.\n"
    "\n"
    "   :arg other: The other quaternion to make compatible with.\n"
    "   :type other: :class:`Quaternion`\n");
static PyObject *Quaternion_make_compatible(QuaternionObject *self, PyObject *value)
{
  float quat[QUAT_SIZE];
  float tquat[QUAT_SIZE];

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  if (mathutils_array_parse(tquat,
                            QUAT_SIZE,
                            QUAT_SIZE,
                            value,
                            "Quaternion.make_compatible(other), invalid 'other' arg") == -1)
  {
    return nullptr;
  }

  /* Can only operate on unit length quaternions. */
  const float quat_len = normalize_qt_qt(quat, self->quat);
  quat_to_compatible_quat(self->quat, quat, tquat);
  mul_qt_fl(self->quat, quat_len);

  (void)BaseMath_WriteCallback(self);

  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: Normalize
 *
 * Normalize the quaternion. This may change the angle as well as the
 * rotation axis, as all of (w, x, y, z) are scaled.
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_normalize_doc,
    ".. function:: normalize()\n"
    "\n"
    "   Normalize the quaternion.\n");
static PyObject *Quaternion_normalize(QuaternionObject *self)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  normalize_qt(self->quat);

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}
PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_normalized_doc,
    ".. function:: normalized()\n"
    "\n"
    "   Return a new normalized quaternion.\n"
    "\n"
    "   :return: a normalized copy.\n"
    "   :rtype: :class:`Quaternion`\n");
static PyObject *Quaternion_normalized(QuaternionObject *self)
{
  return quat__apply_to_copy(Quaternion_normalize, self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: Invert
 *
 * Normalize the quaternion. This may change the angle as well as the
 * rotation axis, as all of (w, x, y, z) are scaled.
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_invert_doc,
    ".. function:: invert()\n"
    "\n"
    "   Set the quaternion to its inverse.\n");
static PyObject *Quaternion_invert(QuaternionObject *self)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  invert_qt(self->quat);

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}
PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_inverted_doc,
    ".. function:: inverted()\n"
    "\n"
    "   Return a new, inverted quaternion.\n"
    "\n"
    "   :return: the inverted value.\n"
    "   :rtype: :class:`Quaternion`\n");
static PyObject *Quaternion_inverted(QuaternionObject *self)
{
  return quat__apply_to_copy(Quaternion_invert, self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: Set Identity
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_identity_doc,
    ".. function:: identity()\n"
    "\n"
    "   Set the quaternion to an identity quaternion.\n");
static PyObject *Quaternion_identity(QuaternionObject *self)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  unit_qt(self->quat);

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: Negate
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_negate_doc,
    ".. function:: negate()\n"
    "\n"
    "   Set the quaternion to its negative.\n");
static PyObject *Quaternion_negate(QuaternionObject *self)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  mul_qt_fl(self->quat, -1.0f);

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: Conjugate
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_conjugate_doc,
    ".. function:: conjugate()\n"
    "\n"
    "   Set the quaternion to its conjugate (negate x, y, z).\n");
static PyObject *Quaternion_conjugate(QuaternionObject *self)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  conjugate_qt(self->quat);

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}
PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_conjugated_doc,
    ".. function:: conjugated()\n"
    "\n"
    "   Return a new conjugated quaternion.\n"
    "\n"
    "   :return: a new quaternion.\n"
    "   :rtype: :class:`Quaternion`\n");
static PyObject *Quaternion_conjugated(QuaternionObject *self)
{
  return quat__apply_to_copy(Quaternion_conjugate, self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Methods: Copy/Deep-Copy
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_copy_doc,
    ".. function:: copy()\n"
    "\n"
    "   Returns a copy of this quaternion.\n"
    "\n"
    "   :return: A copy of the quaternion.\n"
    "   :rtype: :class:`Quaternion`\n"
    "\n"
    "   .. note:: use this to get a copy of a wrapped quaternion with\n"
    "      no reference to the original data.\n");
static PyObject *Quaternion_copy(QuaternionObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  return Quaternion_CreatePyObject(self->quat, Py_TYPE(self));
}
static PyObject *Quaternion_deepcopy(QuaternionObject *self, PyObject *args)
{
  if (!PyC_CheckArgs_DeepCopy(args)) {
    return nullptr;
  }
  return Quaternion_copy(self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: `__repr__` & `__str__`
 * \{ */

static PyObject *Quaternion_repr(QuaternionObject *self)
{
  PyObject *ret, *tuple;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  tuple = Quaternion_to_tuple_ext(self, -1);

  ret = PyUnicode_FromFormat("Quaternion(%R)", tuple);

  Py_DECREF(tuple);
  return ret;
}

#ifndef MATH_STANDALONE
static PyObject *Quaternion_str(QuaternionObject *self)
{
  DynStr *ds;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  ds = BLI_dynstr_new();

  BLI_dynstr_appendf(ds,
                     "<Quaternion (w=%.4f, x=%.4f, y=%.4f, z=%.4f)>",
                     self->quat[0],
                     self->quat[1],
                     self->quat[2],
                     self->quat[3]);

  return mathutils_dynstr_to_py(ds); /* frees ds */
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: Rich Compare
 * \{ */

static PyObject *Quaternion_richcmpr(PyObject *a, PyObject *b, int op)
{
  PyObject *res;
  int ok = -1; /* zero is true */

  if (QuaternionObject_Check(a) && QuaternionObject_Check(b)) {
    QuaternionObject *quatA = (QuaternionObject *)a;
    QuaternionObject *quatB = (QuaternionObject *)b;

    if (BaseMath_ReadCallback(quatA) == -1 || BaseMath_ReadCallback(quatB) == -1) {
      return nullptr;
    }

    ok = EXPP_VectorsAreEqual(quatA->quat, quatB->quat, QUAT_SIZE, 1) ? 0 : -1;
  }

  switch (op) {
    case Py_NE:
      ok = !ok;
      ATTR_FALLTHROUGH;
    case Py_EQ:
      res = ok ? Py_False : Py_True;
      break;

    case Py_LT:
    case Py_LE:
    case Py_GT:
    case Py_GE:
      res = Py_NotImplemented;
      break;
    default:
      PyErr_BadArgument();
      return nullptr;
  }

  return Py_NewRef(res);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: Hash (`__hash__`)
 * \{ */

static Py_hash_t Quaternion_hash(QuaternionObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return -1;
  }

  if (BaseMathObject_Prepare_ForHash(self) == -1) {
    return -1;
  }

  return mathutils_array_hash(self->quat, QUAT_SIZE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: Sequence & Mapping Protocols Implementation
 * \{ */

/** Sequence length: `len(object)`. */
static Py_ssize_t Quaternion_len(QuaternionObject * /*self*/)
{
  return QUAT_SIZE;
}

/** Sequence accessor (get): `x = object[i]`. */
static PyObject *Quaternion_item(QuaternionObject *self, Py_ssize_t i)
{
  if (i < 0) {
    i = QUAT_SIZE - i;
  }

  if (i < 0 || i >= QUAT_SIZE) {
    PyErr_SetString(PyExc_IndexError,
                    "quaternion[attribute]: "
                    "array index out of range");
    return nullptr;
  }

  if (BaseMath_ReadIndexCallback(self, i) == -1) {
    return nullptr;
  }

  return PyFloat_FromDouble(self->quat[i]);
}

/** Sequence accessor (set): `object[i] = x`. */
static int Quaternion_ass_item(QuaternionObject *self, Py_ssize_t i, PyObject *ob)
{
  float f;

  if (BaseMath_Prepare_ForWrite(self) == -1) {
    return -1;
  }

  f = float(PyFloat_AsDouble(ob));

  if (f == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
    PyErr_SetString(PyExc_TypeError,
                    "quaternion[index] = x: "
                    "assigned value not a number");
    return -1;
  }

  if (i < 0) {
    i = QUAT_SIZE - i;
  }

  if (i < 0 || i >= QUAT_SIZE) {
    PyErr_SetString(PyExc_IndexError,
                    "quaternion[attribute] = x: "
                    "array assignment index out of range");
    return -1;
  }
  self->quat[i] = f;

  if (BaseMath_WriteIndexCallback(self, i) == -1) {
    return -1;
  }

  return 0;
}

/** Sequence slice accessor (get): `x = object[i:j]`. */
static PyObject *Quaternion_slice(QuaternionObject *self, int begin, int end)
{
  PyObject *tuple;
  int count;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  CLAMP(begin, 0, QUAT_SIZE);
  if (end < 0) {
    end = (QUAT_SIZE + 1) + end;
  }
  CLAMP(end, 0, QUAT_SIZE);
  begin = std::min(begin, end);

  tuple = PyTuple_New(end - begin);
  for (count = begin; count < end; count++) {
    PyTuple_SET_ITEM(tuple, count - begin, PyFloat_FromDouble(self->quat[count]));
  }

  return tuple;
}

/** Sequence slice accessor (set): `object[i:j] = x`. */
static int Quaternion_ass_slice(QuaternionObject *self, int begin, int end, PyObject *seq)
{
  int i, size;
  float quat[QUAT_SIZE];

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  CLAMP(begin, 0, QUAT_SIZE);
  if (end < 0) {
    end = (QUAT_SIZE + 1) + end;
  }
  CLAMP(end, 0, QUAT_SIZE);
  begin = std::min(begin, end);

  if ((size = mathutils_array_parse(
           quat, 0, QUAT_SIZE, seq, "mathutils.Quaternion[begin:end] = []")) == -1)
  {
    return -1;
  }

  if (size != (end - begin)) {
    PyErr_SetString(PyExc_ValueError,
                    "quaternion[begin:end] = []: "
                    "size mismatch in slice assignment");
    return -1;
  }

  /* Parsed well, now set in vector. */
  for (i = 0; i < size; i++) {
    self->quat[begin + i] = quat[i];
  }

  (void)BaseMath_WriteCallback(self);
  return 0;
}

/** Sequence generic subscript (get): `x = object[...]`. */
static PyObject *Quaternion_subscript(QuaternionObject *self, PyObject *item)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i;
    i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return nullptr;
    }
    if (i < 0) {
      i += QUAT_SIZE;
    }
    return Quaternion_item(self, i);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, QUAT_SIZE, &start, &stop, &step, &slicelength) < 0) {
      return nullptr;
    }

    if (slicelength <= 0) {
      return PyTuple_New(0);
    }
    if (step == 1) {
      return Quaternion_slice(self, start, stop);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with quaternions");
    return nullptr;
  }

  PyErr_Format(
      PyExc_TypeError, "quaternion indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return nullptr;
}

/** Sequence generic subscript (set): `object[...] = x`. */
static int Quaternion_ass_subscript(QuaternionObject *self, PyObject *item, PyObject *value)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return -1;
    }
    if (i < 0) {
      i += QUAT_SIZE;
    }
    return Quaternion_ass_item(self, i, value);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, QUAT_SIZE, &start, &stop, &step, &slicelength) < 0) {
      return -1;
    }

    if (step == 1) {
      return Quaternion_ass_slice(self, start, stop, value);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with quaternion");
    return -1;
  }

  PyErr_Format(
      PyExc_TypeError, "quaternion indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: Numeric Protocol Implementation
 * \{ */

/** Addition: `object + object`. */
static PyObject *Quaternion_add(PyObject *q1, PyObject *q2)
{
  float quat[QUAT_SIZE];
  QuaternionObject *quat1 = nullptr, *quat2 = nullptr;

  if (!QuaternionObject_Check(q1) || !QuaternionObject_Check(q2)) {
    PyErr_Format(PyExc_TypeError,
                 "Quaternion addition: (%s + %s) "
                 "invalid type for this operation",
                 Py_TYPE(q1)->tp_name,
                 Py_TYPE(q2)->tp_name);
    return nullptr;
  }
  quat1 = (QuaternionObject *)q1;
  quat2 = (QuaternionObject *)q2;

  if (BaseMath_ReadCallback(quat1) == -1 || BaseMath_ReadCallback(quat2) == -1) {
    return nullptr;
  }

  add_qt_qtqt(quat, quat1->quat, quat2->quat, 1.0f);
  return Quaternion_CreatePyObject(quat, Py_TYPE(q1));
}

/** Subtraction: `object - object`. */
static PyObject *Quaternion_sub(PyObject *q1, PyObject *q2)
{
  int x;
  float quat[QUAT_SIZE];
  QuaternionObject *quat1 = nullptr, *quat2 = nullptr;

  if (!QuaternionObject_Check(q1) || !QuaternionObject_Check(q2)) {
    PyErr_Format(PyExc_TypeError,
                 "Quaternion subtraction: (%s - %s) "
                 "invalid type for this operation",
                 Py_TYPE(q1)->tp_name,
                 Py_TYPE(q2)->tp_name);
    return nullptr;
  }

  quat1 = (QuaternionObject *)q1;
  quat2 = (QuaternionObject *)q2;

  if (BaseMath_ReadCallback(quat1) == -1 || BaseMath_ReadCallback(quat2) == -1) {
    return nullptr;
  }

  for (x = 0; x < QUAT_SIZE; x++) {
    quat[x] = quat1->quat[x] - quat2->quat[x];
  }

  return Quaternion_CreatePyObject(quat, Py_TYPE(q1));
}

static PyObject *quat_mul_float(QuaternionObject *quat, const float scalar)
{
  float tquat[4];
  copy_qt_qt(tquat, quat->quat);
  mul_qt_fl(tquat, scalar);
  return Quaternion_CreatePyObject(tquat, Py_TYPE(quat));
}

/** Multiplication (element-wise or scalar): `object * object`. */
static PyObject *Quaternion_mul(PyObject *q1, PyObject *q2)
{
  float scalar;
  QuaternionObject *quat1 = nullptr, *quat2 = nullptr;

  if (QuaternionObject_Check(q1)) {
    quat1 = (QuaternionObject *)q1;
    if (BaseMath_ReadCallback(quat1) == -1) {
      return nullptr;
    }
  }
  if (QuaternionObject_Check(q2)) {
    quat2 = (QuaternionObject *)q2;
    if (BaseMath_ReadCallback(quat2) == -1) {
      return nullptr;
    }
  }

  if (quat1 && quat2) { /* QUAT * QUAT (element-wise product) */
    float quat[QUAT_SIZE];
    mul_vn_vnvn(quat, quat1->quat, quat2->quat, QUAT_SIZE);
    return Quaternion_CreatePyObject(quat, Py_TYPE(q1));
  }
  /* the only case this can happen (for a supported type is "FLOAT * QUAT") */
  if (quat2) { /* FLOAT * QUAT */
    if (((scalar = PyFloat_AsDouble(q1)) == -1.0f && PyErr_Occurred()) == 0) {
      return quat_mul_float(quat2, scalar);
    }
  }
  else if (quat1) { /* QUAT * FLOAT */
    if (((scalar = PyFloat_AsDouble(q2)) == -1.0f && PyErr_Occurred()) == 0) {
      return quat_mul_float(quat1, scalar);
    }
  }

  PyErr_Format(PyExc_TypeError,
               "Element-wise multiplication: "
               "not supported between '%.200s' and '%.200s' types",
               Py_TYPE(q1)->tp_name,
               Py_TYPE(q2)->tp_name);
  return nullptr;
}

/** Multiplication in-place (element-wise or scalar): `object *= object`. */
static PyObject *Quaternion_imul(PyObject *q1, PyObject *q2)
{
  float scalar;
  QuaternionObject *quat1 = nullptr, *quat2 = nullptr;

  if (QuaternionObject_Check(q1)) {
    quat1 = (QuaternionObject *)q1;
    if (BaseMath_ReadCallback(quat1) == -1) {
      return nullptr;
    }
  }
  if (QuaternionObject_Check(q2)) {
    quat2 = (QuaternionObject *)q2;
    if (BaseMath_ReadCallback(quat2) == -1) {
      return nullptr;
    }
  }

  if (quat1 && quat2) { /* QUAT *= QUAT (in-place element-wise product). */
    mul_vn_vn(quat1->quat, quat2->quat, QUAT_SIZE);
  }
  else if (quat1 && (((scalar = PyFloat_AsDouble(q2)) == -1.0f && PyErr_Occurred()) == 0)) {
    /* QUAT *= FLOAT */
    mul_qt_fl(quat1->quat, scalar);
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "Element-wise multiplication: "
                 "not supported between '%.200s' and '%.200s' types",
                 Py_TYPE(q1)->tp_name,
                 Py_TYPE(q2)->tp_name);
    return nullptr;
  }

  (void)BaseMath_WriteCallback(quat1);
  Py_INCREF(q1);
  return q1;
}

/** Multiplication (quaternion multiply): `object @ object`. */
static PyObject *Quaternion_matmul(PyObject *q1, PyObject *q2)
{
  float quat[QUAT_SIZE];
  QuaternionObject *quat1 = nullptr, *quat2 = nullptr;

  if (QuaternionObject_Check(q1)) {
    quat1 = (QuaternionObject *)q1;
    if (BaseMath_ReadCallback(quat1) == -1) {
      return nullptr;
    }
  }
  if (QuaternionObject_Check(q2)) {
    quat2 = (QuaternionObject *)q2;
    if (BaseMath_ReadCallback(quat2) == -1) {
      return nullptr;
    }
  }

  if (quat1 && quat2) { /* QUAT @ QUAT (cross product) */
    mul_qt_qtqt(quat, quat1->quat, quat2->quat);
    return Quaternion_CreatePyObject(quat, Py_TYPE(q1));
  }
  if (quat1) {
    /* QUAT @ VEC */
    if (VectorObject_Check(q2)) {
      VectorObject *vec2 = (VectorObject *)q2;
      float tvec[3];

      if (vec2->vec_num != 3) {
        PyErr_SetString(PyExc_ValueError,
                        "Vector multiplication: "
                        "only 3D vector rotations (with quats) "
                        "currently supported");
        return nullptr;
      }
      if (BaseMath_ReadCallback(vec2) == -1) {
        return nullptr;
      }

      copy_v3_v3(tvec, vec2->vec);
      mul_qt_v3(quat1->quat, tvec);

      return Vector_CreatePyObject(tvec, 3, Py_TYPE(vec2));
    }
  }

  PyErr_Format(PyExc_TypeError,
               "Quaternion multiplication: "
               "not supported between '%.200s' and '%.200s' types",
               Py_TYPE(q1)->tp_name,
               Py_TYPE(q2)->tp_name);
  return nullptr;
}

/** Multiplication in-place (quaternion multiply): `object @= object`. */
static PyObject *Quaternion_imatmul(PyObject *q1, PyObject *q2)
{
  float quat[QUAT_SIZE];
  QuaternionObject *quat1 = nullptr, *quat2 = nullptr;

  if (QuaternionObject_Check(q1)) {
    quat1 = (QuaternionObject *)q1;
    if (BaseMath_ReadCallback(quat1) == -1) {
      return nullptr;
    }
  }
  if (QuaternionObject_Check(q2)) {
    quat2 = (QuaternionObject *)q2;
    if (BaseMath_ReadCallback(quat2) == -1) {
      return nullptr;
    }
  }

  if (quat1 && quat2) { /* QUAT @ QUAT (cross product) */
    mul_qt_qtqt(quat, quat1->quat, quat2->quat);
    copy_qt_qt(quat1->quat, quat);
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "In place quaternion multiplication: "
                 "not supported between '%.200s' and '%.200s' types",
                 Py_TYPE(q1)->tp_name,
                 Py_TYPE(q2)->tp_name);
    return nullptr;
  }

  (void)BaseMath_WriteCallback(quat1);
  Py_INCREF(q1);
  return q1;
}

/** Negative (returns the negative of this object): `-object`. */
static PyObject *Quaternion_neg(QuaternionObject *self)
{
  float tquat[QUAT_SIZE];

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  negate_v4_v4(tquat, self->quat);
  return Quaternion_CreatePyObject(tquat, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: Protocol Declarations
 * \{ */

static PySequenceMethods Quaternion_SeqMethods = {
    /*sq_length*/ (lenfunc)Quaternion_len,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /*sq_item*/ (ssizeargfunc)Quaternion_item,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /*sq_ass_item*/ (ssizeobjargproc)Quaternion_ass_item,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ nullptr,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PyMappingMethods Quaternion_AsMapping = {
    /*mp_length*/ (lenfunc)Quaternion_len,
    /*mp_subscript*/ (binaryfunc)Quaternion_subscript,
    /*mp_ass_subscript*/ (objobjargproc)Quaternion_ass_subscript,
};

static PyNumberMethods Quaternion_NumMethods = {
    /*nb_add*/ (binaryfunc)Quaternion_add,
    /*nb_subtract*/ (binaryfunc)Quaternion_sub,
    /*nb_multiply*/ (binaryfunc)Quaternion_mul,
    /*nb_remainder*/ nullptr,
    /*nb_divmod*/ nullptr,
    /*nb_power*/ nullptr,
    /*nb_negative*/ (unaryfunc)Quaternion_neg,
    /*nb_positive*/ (unaryfunc)Quaternion_copy,
    /*nb_absolute*/ nullptr,
    /*nb_bool*/ nullptr,
    /*nb_invert*/ nullptr,
    /*nb_lshift*/ nullptr,
    /*nb_rshift*/ nullptr,
    /*nb_and*/ nullptr,
    /*nb_xor*/ nullptr,
    /*nb_or*/ nullptr,
    /*nb_int*/ nullptr,
    /*nb_reserved*/ nullptr,
    /*nb_float*/ nullptr,
    /*nb_inplace_add*/ nullptr,
    /*nb_inplace_subtract*/ nullptr,
    /*nb_inplace_multiply*/ (binaryfunc)Quaternion_imul,
    /*nb_inplace_remainder*/ nullptr,
    /*nb_inplace_power*/ nullptr,
    /*nb_inplace_lshift*/ nullptr,
    /*nb_inplace_rshift*/ nullptr,
    /*nb_inplace_and*/ nullptr,
    /*nb_inplace_xor*/ nullptr,
    /*nb_inplace_or*/ nullptr,
    /*nb_floor_divide*/ nullptr,
    /*nb_true_divide*/ nullptr,
    /*nb_inplace_floor_divide*/ nullptr,
    /*nb_inplace_true_divide*/ nullptr,
    /*nb_index*/ nullptr,
    /*nb_matrix_multiply*/ (binaryfunc)Quaternion_matmul,
    /*nb_inplace_matrix_multiply*/ (binaryfunc)Quaternion_imatmul,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: Get/Set Item Implementation
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_axis_doc,
    "Quaternion axis value.\n"
    "\n"
    ":type: float");
static PyObject *Quaternion_axis_get(QuaternionObject *self, void *type)
{
  return Quaternion_item(self, POINTER_AS_INT(type));
}

static int Quaternion_axis_set(QuaternionObject *self, PyObject *value, void *type)
{
  return Quaternion_ass_item(self, POINTER_AS_INT(type), value);
}

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_magnitude_doc,
    "Size of the quaternion (read-only).\n"
    "\n"
    ":type: float");
static PyObject *Quaternion_magnitude_get(QuaternionObject *self, void * /*closure*/)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  return PyFloat_FromDouble(sqrtf(dot_qtqt(self->quat, self->quat)));
}

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_angle_doc,
    "Angle of the quaternion.\n"
    "\n"
    ":type: float");
static PyObject *Quaternion_angle_get(QuaternionObject *self, void * /*closure*/)
{
  float tquat[4];
  float angle;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  normalize_qt_qt(tquat, self->quat);

  angle = 2.0f * safe_acosf(tquat[0]);

  quat__axis_angle_sanitize(nullptr, &angle);

  return PyFloat_FromDouble(angle);
}

static int Quaternion_angle_set(QuaternionObject *self, PyObject *value, void * /*closure*/)
{
  float tquat[4];
  float len;

  float axis[3], angle_dummy;
  float angle;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  len = normalize_qt_qt(tquat, self->quat);
  quat_to_axis_angle(axis, &angle_dummy, tquat);

  angle = PyFloat_AsDouble(value);

  if (angle == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
    PyErr_SetString(PyExc_TypeError, "Quaternion.angle = value: float expected");
    return -1;
  }

  angle = angle_wrap_rad(angle);

  quat__axis_angle_sanitize(axis, &angle);

  axis_angle_to_quat(self->quat, axis, angle);
  mul_qt_fl(self->quat, len);

  if (BaseMath_WriteCallback(self) == -1) {
    return -1;
  }

  return 0;
}

PyDoc_STRVAR(
    /* Wrap. */
    Quaternion_axis_vector_doc,
    "Quaternion axis as a vector.\n"
    "\n"
    ":type: :class:`Vector`");
static PyObject *Quaternion_axis_vector_get(QuaternionObject *self, void * /*closure*/)
{
  float tquat[4];

  float axis[3];
  float angle_dummy;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  normalize_qt_qt(tquat, self->quat);
  quat_to_axis_angle(axis, &angle_dummy, tquat);

  quat__axis_angle_sanitize(axis, nullptr);

  return Vector_CreatePyObject(axis, 3, nullptr);
}

static int Quaternion_axis_vector_set(QuaternionObject *self, PyObject *value, void * /*closure*/)
{
  float tquat[4];
  float len;

  float axis[3];
  float angle;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  len = normalize_qt_qt(tquat, self->quat);
  quat_to_axis_angle(axis, &angle, tquat); /* axis value is unused */

  if (mathutils_array_parse(axis, 3, 3, value, "quat.axis = other") == -1) {
    return -1;
  }

  quat__axis_angle_sanitize(axis, &angle);

  axis_angle_to_quat(self->quat, axis, angle);
  mul_qt_fl(self->quat, len);

  if (BaseMath_WriteCallback(self) == -1) {
    return -1;
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: Get/Set Item Definitions
 * \{ */

static PyGetSetDef Quaternion_getseters[] = {
    {"w",
     (getter)Quaternion_axis_get,
     (setter)Quaternion_axis_set,
     Quaternion_axis_doc,
     POINTER_FROM_INT(0)},
    {"x",
     (getter)Quaternion_axis_get,
     (setter)Quaternion_axis_set,
     Quaternion_axis_doc,
     POINTER_FROM_INT(1)},
    {"y",
     (getter)Quaternion_axis_get,
     (setter)Quaternion_axis_set,
     Quaternion_axis_doc,
     POINTER_FROM_INT(2)},
    {"z",
     (getter)Quaternion_axis_get,
     (setter)Quaternion_axis_set,
     Quaternion_axis_doc,
     POINTER_FROM_INT(3)},
    {"magnitude",
     (getter)Quaternion_magnitude_get,
     (setter) nullptr,
     Quaternion_magnitude_doc,
     nullptr},
    {"angle",
     (getter)Quaternion_angle_get,
     (setter)Quaternion_angle_set,
     Quaternion_angle_doc,
     nullptr},
    {"axis",
     (getter)Quaternion_axis_vector_get,
     (setter)Quaternion_axis_vector_set,
     Quaternion_axis_vector_doc,
     nullptr},
    {"is_wrapped",
     (getter)BaseMathObject_is_wrapped_get,
     (setter) nullptr,
     BaseMathObject_is_wrapped_doc,
     nullptr},
    {"is_frozen",
     (getter)BaseMathObject_is_frozen_get,
     (setter) nullptr,
     BaseMathObject_is_frozen_doc,
     nullptr},
    {"is_valid",
     (getter)BaseMathObject_is_valid_get,
     (setter) nullptr,
     BaseMathObject_is_valid_doc,
     nullptr},
    {"owner",
     (getter)BaseMathObject_owner_get,
     (setter) nullptr,
     BaseMathObject_owner_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: Method Definitions
 * \{ */

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef Quaternion_methods[] = {
    /* In place only. */
    {"identity", (PyCFunction)Quaternion_identity, METH_NOARGS, Quaternion_identity_doc},
    {"negate", (PyCFunction)Quaternion_negate, METH_NOARGS, Quaternion_negate_doc},

    /* Operate on original or copy. */
    {"conjugate", (PyCFunction)Quaternion_conjugate, METH_NOARGS, Quaternion_conjugate_doc},
    {"conjugated", (PyCFunction)Quaternion_conjugated, METH_NOARGS, Quaternion_conjugated_doc},

    {"invert", (PyCFunction)Quaternion_invert, METH_NOARGS, Quaternion_invert_doc},
    {"inverted", (PyCFunction)Quaternion_inverted, METH_NOARGS, Quaternion_inverted_doc},

    {"normalize", (PyCFunction)Quaternion_normalize, METH_NOARGS, Quaternion_normalize_doc},
    {"normalized", (PyCFunction)Quaternion_normalized, METH_NOARGS, Quaternion_normalized_doc},

    /* Return converted representation. */
    {"to_euler", (PyCFunction)Quaternion_to_euler, METH_VARARGS, Quaternion_to_euler_doc},
    {"to_matrix", (PyCFunction)Quaternion_to_matrix, METH_NOARGS, Quaternion_to_matrix_doc},
    {"to_axis_angle",
     (PyCFunction)Quaternion_to_axis_angle,
     METH_NOARGS,
     Quaternion_to_axis_angle_doc},
    {"to_swing_twist",
     (PyCFunction)Quaternion_to_swing_twist,
     METH_O,
     Quaternion_to_swing_twist_doc},
    {"to_exponential_map",
     (PyCFunction)Quaternion_to_exponential_map,
     METH_NOARGS,
     Quaternion_to_exponential_map_doc},

    /* Operation between 2 or more types. */
    {"cross", (PyCFunction)Quaternion_cross, METH_O, Quaternion_cross_doc},
    {"dot", (PyCFunction)Quaternion_dot, METH_O, Quaternion_dot_doc},
    {"rotation_difference",
     (PyCFunction)Quaternion_rotation_difference,
     METH_O,
     Quaternion_rotation_difference_doc},
    {"slerp", (PyCFunction)Quaternion_slerp, METH_VARARGS, Quaternion_slerp_doc},
    {"rotate", (PyCFunction)Quaternion_rotate, METH_O, Quaternion_rotate_doc},
    {"make_compatible",
     (PyCFunction)Quaternion_make_compatible,
     METH_O,
     Quaternion_make_compatible_doc},

    /* Base-math methods. */
    {"freeze", (PyCFunction)BaseMathObject_freeze, METH_NOARGS, BaseMathObject_freeze_doc},

    {"copy", (PyCFunction)Quaternion_copy, METH_NOARGS, Quaternion_copy_doc},
    {"__copy__", (PyCFunction)Quaternion_copy, METH_NOARGS, Quaternion_copy_doc},
    {"__deepcopy__", (PyCFunction)Quaternion_deepcopy, METH_VARARGS, Quaternion_copy_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: Python Object Definition
 * \{ */

#ifdef MATH_STANDALONE
#  define Quaternion_str nullptr
#endif

PyDoc_STRVAR(
    /* Wrap. */
    quaternion_doc,
    ".. class:: Quaternion(seq=(1.0, 0.0, 0.0, 0.0), angle=0.0, /)\n"
    "\n"
    "   This object gives access to Quaternions in Blender.\n"
    "\n"
    "   :arg seq: size 3 or 4\n"
    "   :type seq: :class:`Vector`\n"
    "   :arg angle: rotation angle, in radians\n"
    "   :type angle: float\n"
    "\n"
    "   The constructor takes arguments in various forms:\n"
    "\n"
    "   (), *no args*\n"
    "      Create an identity quaternion\n"
    "   (*wxyz*)\n"
    "      Create a quaternion from a ``(w, x, y, z)`` vector.\n"
    "   (*exponential_map*)\n"
    "      Create a quaternion from a 3d exponential map vector.\n"
    "\n"
    "      .. seealso:: :meth:`to_exponential_map`\n"
    "   (*axis, angle*)\n"
    "      Create a quaternion representing a rotation of *angle* radians over *axis*.\n"
    "\n"
    "      .. seealso:: :meth:`to_axis_angle`\n");
PyTypeObject quaternion_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Quaternion",
    /*tp_basicsize*/ sizeof(QuaternionObject),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)BaseMathObject_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)Quaternion_repr,
    /*tp_as_number*/ &Quaternion_NumMethods,
    /*tp_as_sequence*/ &Quaternion_SeqMethods,
    /*tp_as_mapping*/ &Quaternion_AsMapping,
    /*tp_hash*/ (hashfunc)Quaternion_hash,
    /*tp_call*/ nullptr,
    /*tp_str*/ (reprfunc)Quaternion_str,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    /*tp_doc*/ quaternion_doc,
    /*tp_traverse*/ (traverseproc)BaseMathObject_traverse,
    /*tp_clear*/ (inquiry)BaseMathObject_clear,
    /*tp_richcompare*/ (richcmpfunc)Quaternion_richcmpr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ Quaternion_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ Quaternion_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ Quaternion_new,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ (inquiry)BaseMathObject_is_gc,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

#ifdef MATH_STANDALONE
#  undef Quaternion_str
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Type: C/API Constructors
 * \{ */

PyObject *Quaternion_CreatePyObject(const float quat[4], PyTypeObject *base_type)
{
  QuaternionObject *self;
  float *quat_alloc;

  quat_alloc = static_cast<float *>(PyMem_Malloc(QUAT_SIZE * sizeof(float)));
  if (UNLIKELY(quat_alloc == nullptr)) {
    PyErr_SetString(PyExc_MemoryError,
                    "Quaternion(): "
                    "problem allocating data");
    return nullptr;
  }

  self = BASE_MATH_NEW(QuaternionObject, quaternion_Type, base_type);
  if (self) {
    self->quat = quat_alloc;
    /* init callbacks as nullptr */
    self->cb_user = nullptr;
    self->cb_type = self->cb_subtype = 0;

    /* NEW */
    if (!quat) { /* new empty */
      unit_qt(self->quat);
    }
    else {
      copy_qt_qt(self->quat, quat);
    }
    self->flag = BASE_MATH_FLAG_DEFAULT;
  }
  else {
    PyMem_Free(quat_alloc);
  }

  return (PyObject *)self;
}

PyObject *Quaternion_CreatePyObject_wrap(float quat[4], PyTypeObject *base_type)
{
  QuaternionObject *self;

  self = BASE_MATH_NEW(QuaternionObject, quaternion_Type, base_type);
  if (self) {
    /* init callbacks as nullptr */
    self->cb_user = nullptr;
    self->cb_type = self->cb_subtype = 0;

    /* WRAP */
    self->quat = quat;
    self->flag = BASE_MATH_FLAG_DEFAULT | BASE_MATH_FLAG_IS_WRAP;
  }
  return (PyObject *)self;
}

PyObject *Quaternion_CreatePyObject_cb(PyObject *cb_user, uchar cb_type, uchar cb_subtype)
{
  QuaternionObject *self = (QuaternionObject *)Quaternion_CreatePyObject(nullptr, nullptr);
  if (self) {
    Py_INCREF(cb_user);
    self->cb_user = cb_user;
    self->cb_type = cb_type;
    self->cb_subtype = cb_subtype;
    BLI_assert(!PyObject_GC_IsTracked((PyObject *)self));
    PyObject_GC_Track(self);
  }

  return (PyObject *)self;
}

/** \} */
