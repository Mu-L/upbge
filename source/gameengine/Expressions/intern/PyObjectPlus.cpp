/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Expressions/EXP_PyObjectPlus.cpp
 *  \ingroup expressions
 */

/*------------------------------
 * EXP_PyObjectPlus cpp
 *
 * C++ library routines for Crawl 3.2
 *
 * Derived from work by
 * David Redish
 * graduate student
 * Computer Science Department
 * Carnegie Mellon University (CMU)
 * Center for the Neural Basis of Cognition (CNBC)
 * http://www.python.org/doc/PyCPP.html
 *
 * ----------------------------- */

#include "EXP_PyObjectPlus.h"


#include "CM_Message.h"

EXP_PyObjectPlus::EXP_PyObjectPlus()
{
#ifdef WITH_PYTHON
  m_proxy = nullptr;
#endif
}

EXP_PyObjectPlus::~EXP_PyObjectPlus()
{
#ifdef WITH_PYTHON
  InvalidateProxy();
#endif
}

void EXP_PyObjectPlus::ProcessReplica()
{
#ifdef WITH_PYTHON
  /* Clear the proxy, will be created again if needed with GetProxy()
   * otherwise the PyObject will point to the wrong reference */
  m_proxy = nullptr;
#endif
}

/* Sometimes we might want to manually invalidate a BGE type even if
 * it hasn't been released by the BGE, say for example when an object
 * is removed from a scene, accessing it may cause problems.
 *
 * In this case the current proxy is made invalid, disowned,
 * and will raise an error on access. However if python can get access
 * to this class again it will make a new proxy and work as expected.
 */
void EXP_PyObjectPlus::InvalidateProxy()  // check typename of each parent
{
#ifdef WITH_PYTHON
  if (m_proxy) {
    EXP_PROXY_REF(m_proxy) = nullptr;
    // Decrement proxy only if python doesn't own it.
    if (!EXP_PROXY_PYOWNS(m_proxy)) {
      Py_DECREF(m_proxy);
    }
    m_proxy = nullptr;
  }
#endif
}

void EXP_PyObjectPlus::DestructFromPython()
{
#ifdef WITH_PYTHON
  delete this;
#endif
}

#ifdef WITH_PYTHON

/*------------------------------
 * EXP_PyObjectPlus Type		-- Every class, even the abstract one should have a Type
 * ----------------------------- */

PyTypeObject EXP_PyObjectPlus::Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "EXP_PyObjectPlus", /*tp_name*/
    sizeof(EXP_PyObjectPlus_Proxy),                       /*tp_basicsize*/
    0,                                                    /*tp_itemsize*/
    /* methods */
    py_base_dealloc, /* tp_dealloc */
    0,               /* printfunc tp_print; */
    0,               /* getattrfunc tp_getattr; */
    0,               /* setattrfunc tp_setattr; */
    0,
    /* tp_compare */ /* DEPRECATED in python 3.0! */
    py_base_repr,    /* tp_repr */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,                                        /* Method suites for standard classes */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* long tp_flags; */
    0,
    0,
    0,
    0,
/* weak reference enabler */
#  ifdef USE_WEAKREFS
    offsetof(EXP_PyObjectPlus_Proxy, in_weakreflist), /* long tp_weaklistoffset; */
#  else
    0,
#  endif
    0,
    0,
    Methods,
    0,
    0,
    nullptr  // no subtype
};

/// This should be the entry in Type.
PyObject *EXP_PyObjectPlus::py_base_repr(PyObject *self)
{
  EXP_PyObjectPlus *self_plus = EXP_PROXY_REF(self);
  if (self_plus == nullptr) {
    PyErr_SetString(PyExc_SystemError, EXP_PROXY_ERROR_MSG);
    return nullptr;
  }
  return self_plus->py_repr();
}

PyObject *EXP_PyObjectPlus::py_base_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PyTypeObject *base_type;

  // One or more args is needed.
  if (!PyTuple_GET_SIZE(args)) {
    PyErr_SetString(PyExc_TypeError, "Expected at least one argument");
    return nullptr;
  }

  EXP_PyObjectPlus_Proxy *base = (EXP_PyObjectPlus_Proxy *)PyTuple_GET_ITEM(args, 0);

  /**
   * the 'base' PyObject may be subclassed (multiple times even)
   * we need to find the first C++ defined class to check 'type'
   * is a subclass of the base arguments type.
   *
   * This way we can share one tp_new function for every EXP_PyObjectPlus
   *
   * eg.
   *
   * # CustomOb is called 'type' in this C code
   * \code{.py}
   * class CustomOb(GameTypes.KX_GameObject):
   *     pass
   *
   * # this calls py_base_new(...), the type of 'CustomOb' is checked to be a subclass of the
   * 'cont.owner' type ob = CustomOb(cont.owner) \endcode
   * */
  base_type = Py_TYPE(base);
  while (base_type && !EXP_PROXY_CHECK_TYPE(base_type)) {
    base_type = base_type->tp_base;
  }

  if (base_type == nullptr || !EXP_PROXY_CHECK_TYPE(base_type)) {
    PyErr_SetString(PyExc_TypeError,
                    "can't subclass from a blender game type because the argument given is not a "
                    "game class or subclass");
    return nullptr;
  }

  // Use base_type rather than Py_TYPE(base) because we could already be subtyped.
  if (!PyType_IsSubtype(type, base_type)) {
    PyErr_Format(PyExc_TypeError,
                 "can't subclass blender game type <%s> from <%s> because it is not a subclass",
                 base_type->tp_name,
                 type->tp_name);
    return nullptr;
  }

  /* invalidate the existing base and return a new subclassed one,
   * this is a bit dodgy in that it also attaches its self to the existing object
   * which is not really 'correct' python OO but for our use its OK. */

  EXP_PyObjectPlus_Proxy *ret = (EXP_PyObjectPlus_Proxy *)type->tp_alloc(
      type, 0);  // Starts with 1 ref, used for the return ref'.
  ret->ref = base->ref;
  ret->ptr = base->ptr;
  ret->py_owns = base->py_owns;
  ret->py_ref = base->py_ref;

  if (ret->py_ref) {
    base->ref = nullptr;  // Invalidate! disallow further access.
    base->ptr = nullptr;
    if (ret->ref) {
      ret->ref->m_proxy = nullptr;
    }
    /* 'base' may be freed after this func finished but not necessarily
     * there is no reference to the BGE data now so it will throw an error on access */
    Py_DECREF(base);
    if (ret->ref) {
      ret->ref->m_proxy = (PyObject *)ret;

      // Incref the proxy in case the python doesn't own the ref.
      if (!ret->py_owns) {
        Py_INCREF(ret);
      }
    }
  }
  else {
    // Generic structures don't hold a reference to this proxy, so don't increment ref count.
    if (ret->py_owns) {
      // But if the proxy owns the structure, there can be only one owner.
      base->ptr = nullptr;
    }
  }

  return (PyObject *)ret;
}

/**
 * \param self A EXP_PyObjectPlus_Proxy
 */
void EXP_PyObjectPlus::py_base_dealloc(PyObject *self)
{
#  ifdef USE_WEAKREFS
  if (EXP_PROXY_WKREF(self) != nullptr) {
    PyObject_ClearWeakRefs((PyObject *)self);
  }
#  endif

  if (EXP_PROXY_PYREF(self)) {
    EXP_PyObjectPlus *self_plus = EXP_PROXY_REF(self);
    if (self_plus) {
      // Does python own this?, then delete it.
      if (EXP_PROXY_PYOWNS(self)) {
        self_plus->DestructFromPython();
      }
      EXP_PROXY_REF(self) = nullptr;  // Not really needed.
    }
    // The generic pointer is not deleted directly, only through self_plus.
    EXP_PROXY_PTR(self) = nullptr;  // Not really needed.
  }
  else {
    void *ptr = EXP_PROXY_PTR(self);
    if (ptr) {
      // Does python own this?, then delete it.
      if (EXP_PROXY_PYOWNS(self)) {
        // Generic structure owned by python MUST be created though MEM_alloc.
        MEM_freeN(ptr);
      }
      EXP_PROXY_PTR(self) = nullptr;  // Not really needed.
    }
  }
  /* is ok normally but not for subtyping, use tp_free instead.
   * PyObject_DEL(self);
   */
  Py_TYPE(self)->tp_free(self);
};

/*------------------------------
* EXP_PyObjectPlus Methods  -- Every class, even the abstract one should have a Methods
   ------------------------------*/
PyMethodDef EXP_PyObjectPlus::Methods[] = {
    {nullptr, nullptr}  // Sentinel
};

#  define BGE_PY_ATTR_INVALID (&(EXP_PyObjectPlus::Attributes[0]))
PyAttributeDef EXP_PyObjectPlus::Attributes[] = {
    EXP_PYATTRIBUTE_RO_FUNCTION("invalid", EXP_PyObjectPlus, pyattr_get_invalid),
    EXP_PYATTRIBUTE_NULL  // Sentinel
};

PyObject *EXP_PyObjectPlus::pyattr_get_invalid(EXP_PyObjectPlus *self_v,
                                               const EXP_PYATTRIBUTE_DEF *attrdef)
{
  return PyBool_FromLong(self_v ? 0 : 1);
}

// Note, this is called as a python 'getset, where the PyAttributeDef is the closure.
PyObject *EXP_PyObjectPlus::py_get_attrdef(PyObject *self_py, const PyAttributeDef *attrdef)
{
  EXP_PyObjectPlus *ref = (EXP_PROXY_REF(self_py));
  void *vptr = (attrdef->m_usePtr) ? (char *)EXP_PROXY_PTR(self_py) : (char *)ref;
  if (vptr == nullptr || (EXP_PROXY_PYREF(self_py) && (ref == nullptr || !ref->py_is_valid()))) {
    if (attrdef == BGE_PY_ATTR_INVALID) {
      Py_RETURN_TRUE;  // Don't bother running the function.
    }
    PyErr_SetString(PyExc_SystemError, EXP_PROXY_ERROR_MSG);
    return nullptr;
  }

  if (attrdef->m_type == EXP_PYATTRIBUTE_TYPE_FUNCTION) {
    // The attribute has no field correspondence, handover processing to function.
    if (attrdef->m_getFunction == nullptr) {
      return nullptr;
    }
    return (*attrdef->m_getFunction)(static_cast<EXP_PyObjectPlus *>(vptr), attrdef);
  }
  char *ptr = static_cast<char *>(vptr);
  ptr += attrdef->m_offset;
  if (attrdef->m_length > 1) {
    PyObject *resultlist = PyList_New(attrdef->m_length);
    for (unsigned int i = 0; i < attrdef->m_length; i++) {
      switch (attrdef->m_type) {
        case EXP_PYATTRIBUTE_TYPE_BOOL: {
          bool *val = reinterpret_cast<bool *>(ptr);
          ptr += sizeof(bool);
          PyList_SET_ITEM(resultlist, i, PyBool_FromLong(*val));
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_SHORT: {
          short int *val = reinterpret_cast<short int *>(ptr);
          ptr += sizeof(short int);
          PyList_SET_ITEM(resultlist, i, PyLong_FromLong(*val));
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_ENUM: {
          // Enum are like int, just make sure the field size is the same.
          if (sizeof(int) != attrdef->m_size) {
            Py_DECREF(resultlist);
            return nullptr;
          }
          ATTR_FALLTHROUGH;
        }
        case EXP_PYATTRIBUTE_TYPE_INT: {
          int *val = reinterpret_cast<int *>(ptr);
          ptr += sizeof(int);
          PyList_SET_ITEM(resultlist, i, PyLong_FromLong(*val));
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_FLOAT: {
          float *val = reinterpret_cast<float *>(ptr);
          ptr += sizeof(float);
          PyList_SET_ITEM(resultlist, i, PyFloat_FromDouble(*val));
          break;
        }
        default:
          // No support for array of complex data.
          Py_DECREF(resultlist);
          return nullptr;
      }
    }
    return resultlist;
  }
  else {
    switch (attrdef->m_type) {
      case EXP_PYATTRIBUTE_TYPE_FLAG: {
        bool bval;
        switch (attrdef->m_size) {
          case 1: {
            unsigned char *val = reinterpret_cast<unsigned char *>(ptr);
            bval = (*val & attrdef->m_imin);
            break;
          }
          case 2: {
            unsigned short *val = reinterpret_cast<unsigned short *>(ptr);
            bval = (*val & attrdef->m_imin);
            break;
          }
          case 4: {
            unsigned int *val = reinterpret_cast<unsigned int *>(ptr);
            bval = (*val & attrdef->m_imin);
            break;
          }
          default:
            return nullptr;
        }
        if (attrdef->m_imax) {
          bval = !bval;
        }
        return PyBool_FromLong(bval);
      }
      case EXP_PYATTRIBUTE_TYPE_BOOL: {
        bool *val = reinterpret_cast<bool *>(ptr);
        return PyBool_FromLong(*val);
      }
      case EXP_PYATTRIBUTE_TYPE_SHORT: {
        short int *val = reinterpret_cast<short int *>(ptr);
        return PyLong_FromLong(*val);
      }
      case EXP_PYATTRIBUTE_TYPE_ENUM: {
        // Enum are like int, just make sure the field size is the same.
        if (sizeof(int) != attrdef->m_size) {
          return nullptr;
        }
      }
        ATTR_FALLTHROUGH;
      case EXP_PYATTRIBUTE_TYPE_INT: {
        int *val = reinterpret_cast<int *>(ptr);
        return PyLong_FromLong(*val);
      }
      case EXP_PYATTRIBUTE_TYPE_FLOAT: {
        float *val = reinterpret_cast<float *>(ptr);
        if (attrdef->m_imin == 0) {
          if (attrdef->m_imax == 0) {
            return PyFloat_FromDouble(*val);
          }
          else {
            // Vector, verify size.
            if (attrdef->m_size != attrdef->m_imax * sizeof(float)) {
              return nullptr;
            }
#  ifdef USE_MATHUTILS
            return Vector_CreatePyObject(val, attrdef->m_imax, nullptr);
#  else
            PyObject *resultlist = PyList_New(attrdef->m_imax);
            for (unsigned int i = 0; i < attrdef->m_imax; i++) {
              PyList_SET_ITEM(resultlist, i, PyFloat_FromDouble(val[i]));
            }
            return resultlist;
#  endif
          }
        }
        else {
          // Matrix case.
          if (attrdef->m_size != attrdef->m_imax * attrdef->m_imin * sizeof(float)) {
            return nullptr;
          }
#  ifdef USE_MATHUTILS
          return Matrix_CreatePyObject_wrap(val, attrdef->m_imin, attrdef->m_imax, nullptr);
#  else
          PyObject *collist = PyList_New(attrdef->m_imin);
          for (unsigned int i = 0; i < attrdef->m_imin; i++) {
            PyObject *col = PyList_New(attrdef->m_imax);
            for (unsigned int j = 0; j < attrdef->m_imax; j++) {
              PyList_SET_ITEM(col, j, PyFloat_FromDouble(val[j]));
            }
            PyList_SET_ITEM(collist, i, col);
            val += attrdef->m_imax;
          }
          return collist;
#  endif
        }
      }
      case EXP_PYATTRIBUTE_TYPE_VECTOR: {
        MT_Vector3 *val = reinterpret_cast<MT_Vector3 *>(ptr);
#  ifdef USE_MATHUTILS
        float fval[3];
        val->getValue(fval);
        return Vector_CreatePyObject(fval, 3, nullptr);
#  else
        PyObject *resultlist = PyList_New(3);
        for (unsigned int i = 0; i < 3; i++) {
          PyList_SET_ITEM(resultlist, i, PyFloat_FromDouble((*val)[i]));
        }
        return resultlist;
#  endif
      }
      case EXP_PYATTRIBUTE_TYPE_STRING: {
        std::string *val = reinterpret_cast<std::string *>(ptr);
        return PyUnicode_FromStdString(*val);
      }
      case EXP_PYATTRIBUTE_TYPE_CHAR: {
        return PyUnicode_FromString(ptr);
      }
      default:
        return nullptr;
    }
  }
}

static bool py_check_attr_float(float *var, PyObject *value, const PyAttributeDef *attrdef)
{
  float val = PyFloat_AsDouble(value);
  if (val == -1.0f && PyErr_Occurred()) {
    PyErr_Format(
        PyExc_TypeError, "expected float value for attribute \"%s\"", attrdef->m_name.c_str());
    return false;
  }
  if (attrdef->m_clamp) {
    if (val < attrdef->m_fmin) {
      val = attrdef->m_fmin;
    }
    else if (val > attrdef->m_fmax) {
      val = attrdef->m_fmax;
    }
  }
  else if (val < attrdef->m_fmin || val > attrdef->m_fmax) {
    PyErr_Format(
        PyExc_ValueError, "value out of range for attribute \"%s\"", attrdef->m_name.c_str());
    return false;
  }
  *var = (float)val;
  return true;
}

// Note, this is called as a python getset.
int EXP_PyObjectPlus::py_set_attrdef(PyObject *self_py,
                                     PyObject *value,
                                     const PyAttributeDef *attrdef)
{
  EXP_PyObjectPlus *ref = (EXP_PROXY_REF(self_py));
  char *ptr = (attrdef->m_usePtr) ? (char *)EXP_PROXY_PTR(self_py) : (char *)ref;
  if (ref == nullptr || !ref->py_is_valid() || ptr == nullptr) {
    PyErr_SetString(PyExc_SystemError, EXP_PROXY_ERROR_MSG);
    return PY_SET_ATTR_FAIL;
  }

  void *undoBuffer = nullptr;
  void *sourceBuffer = nullptr;
  size_t bufferSize = 0;
  PyObject *item = nullptr;  // To store object that must be dereferenced in case of error.
  PyObject *list = nullptr;  // To store object that must be dereferenced in case of error.

  ptr += attrdef->m_offset;
  if (attrdef->m_length > 1) {
    if (!PySequence_Check(value)) {
      PyErr_Format(
          PyExc_TypeError, "expected a sequence for attribute \"%s\"", attrdef->m_name.c_str());
      return PY_SET_ATTR_FAIL;
    }
    if (PySequence_Size(value) != attrdef->m_length) {
      PyErr_Format(PyExc_TypeError,
                   "incorrect number of elements in sequence for attribute \"%s\"",
                   attrdef->m_name.c_str());
      return PY_SET_ATTR_FAIL;
    }
    switch (attrdef->m_type) {
      case EXP_PYATTRIBUTE_TYPE_FUNCTION: {
        if (attrdef->m_setFunction == nullptr) {
          PyErr_Format(
              PyExc_AttributeError,
              "function attribute without function for attribute \"%s\", report to blender.org",
              attrdef->m_name.c_str());
          return PY_SET_ATTR_FAIL;
        }
        return (*attrdef->m_setFunction)(ref, attrdef, value);
      }
      case EXP_PYATTRIBUTE_TYPE_BOOL: {
        bufferSize = sizeof(bool);
        break;
      }
      case EXP_PYATTRIBUTE_TYPE_SHORT: {
        bufferSize = sizeof(short int);
        break;
      }
      case EXP_PYATTRIBUTE_TYPE_ENUM:
      case EXP_PYATTRIBUTE_TYPE_INT: {
        bufferSize = sizeof(int);
        break;
      }
      case EXP_PYATTRIBUTE_TYPE_FLOAT: {
        bufferSize = sizeof(float);
        break;
      }
      default:
        // Should not happen.
        PyErr_Format(PyExc_AttributeError,
                     "Unsupported attribute type for attribute \"%s\", report to blender.org",
                     attrdef->m_name.c_str());
        return PY_SET_ATTR_FAIL;
    }
    // Let's implement a smart undo method.
    bufferSize *= attrdef->m_length;
    undoBuffer = malloc(bufferSize);
    sourceBuffer = ptr;
    if (undoBuffer) {
      memcpy(undoBuffer, sourceBuffer, bufferSize);
    }
    for (int i = 0; i < attrdef->m_length; i++) {
      item = PySequence_GetItem(value, i);  // new ref
      switch (attrdef->m_type) {
        case EXP_PYATTRIBUTE_TYPE_BOOL: {
          bool *var = reinterpret_cast<bool *>(ptr);
          ptr += sizeof(bool);
          if (PyLong_Check(item)) {
            *var = (PyLong_AsLong(item) != 0);
          }
          else if (PyBool_Check(item)) {
            *var = (item == Py_True);
          }
          else {
            PyErr_Format(PyExc_TypeError,
                         "expected an integer or a bool for attribute \"%s\"",
                         attrdef->m_name.c_str());
            goto UNDO_AND_ERROR;
          }
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_SHORT: {
          short int *var = reinterpret_cast<short int *>(ptr);
          ptr += sizeof(short int);
          if (PyLong_Check(item)) {
            int val = PyLong_AsLong(item);
            if (attrdef->m_clamp) {
              if (val < attrdef->m_imin) {
                val = attrdef->m_imin;
              }
              else if (val > attrdef->m_imax) {
                val = attrdef->m_imax;
              }
            }
            else if (val < attrdef->m_imin || val > attrdef->m_imax) {
              PyErr_Format(PyExc_ValueError,
                           "item value out of range for attribute \"%s\"",
                           attrdef->m_name.c_str());
              goto UNDO_AND_ERROR;
            }
            *var = (short int)val;
          }
          else {
            PyErr_Format(PyExc_TypeError,
                         "expected an integer for attribute \"%s\"",
                         attrdef->m_name.c_str());
            goto UNDO_AND_ERROR;
          }
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_ENUM: {
          // Enum are equivalent to int, just make sure that the field size matches.
          if (sizeof(int) != attrdef->m_size) {
            PyErr_Format(PyExc_AttributeError,
                         "Size check error for attribute, \"%s\", report to blender.org",
                         attrdef->m_name.c_str());
            goto UNDO_AND_ERROR;
          }
          ATTR_FALLTHROUGH;
        }
        case EXP_PYATTRIBUTE_TYPE_INT: {
          int *var = reinterpret_cast<int *>(ptr);
          ptr += sizeof(int);
          if (PyLong_Check(item)) {
            int val = PyLong_AsLong(item);
            if (attrdef->m_clamp) {
              if (val < attrdef->m_imin) {
                val = attrdef->m_imin;
              }
              else if (val > attrdef->m_imax) {
                val = attrdef->m_imax;
              }
            }
            else if (val < attrdef->m_imin || val > attrdef->m_imax) {
              PyErr_Format(PyExc_ValueError,
                           "item value out of range for attribute \"%s\"",
                           attrdef->m_name.c_str());
              goto UNDO_AND_ERROR;
            }
            *var = (int)val;
          }
          else {
            PyErr_Format(PyExc_TypeError,
                         "expected an integer for attribute \"%s\"",
                         attrdef->m_name.c_str());
            goto UNDO_AND_ERROR;
          }
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_FLOAT: {
          float *var = reinterpret_cast<float *>(ptr);
          ptr += sizeof(float);
          float val = PyFloat_AsDouble(item);
          if (val == -1.0f && PyErr_Occurred()) {
            PyErr_Format(
                PyExc_TypeError, "expected a float for attribute \"%s\"", attrdef->m_name.c_str());
            goto UNDO_AND_ERROR;
          }
          else if (attrdef->m_clamp) {
            if (val < attrdef->m_fmin) {
              val = attrdef->m_fmin;
            }
            else if (val > attrdef->m_fmax) {
              val = attrdef->m_fmax;
            }
          }
          else if (val < attrdef->m_fmin || val > attrdef->m_fmax) {
            PyErr_Format(PyExc_ValueError,
                         "item value out of range for attribute \"%s\"",
                         attrdef->m_name.c_str());
            goto UNDO_AND_ERROR;
          }
          *var = (float)val;
          break;
        }
        default:
          // Should not happen.
          PyErr_Format(PyExc_AttributeError,
                       "type check error for attribute \"%s\", report to blender.org",
                       attrdef->m_name.c_str());
          goto UNDO_AND_ERROR;
      }
      // Finished using item, release.
      Py_DECREF(item);
      item = nullptr;
    }
    // No error, call check function if any.
    if (attrdef->m_checkFunction != nullptr) {
      if ((*attrdef->m_checkFunction)(ref, attrdef) != 0) {
        // If the checing function didnt set an error then set a generic one here so we don't set
        // an error with no exception.
        if (PyErr_Occurred() == 0) {
          PyErr_Format(PyExc_AttributeError,
                       "type check error for attribute \"%s\", reasion unknown",
                       attrdef->m_name.c_str());
        }

        // Post check returned an error, restore values.
      UNDO_AND_ERROR:
        if (undoBuffer) {
          memcpy(sourceBuffer, undoBuffer, bufferSize);
          free(undoBuffer);
        }
        if (item) {
          Py_DECREF(item);
        }
        return PY_SET_ATTR_FAIL;
      }
    }
    if (undoBuffer) {
      free(undoBuffer);
    }
    return PY_SET_ATTR_SUCCESS;
  }
  else {
    if (attrdef->m_type == EXP_PYATTRIBUTE_TYPE_FUNCTION) {
      if (attrdef->m_setFunction == nullptr) {
        PyErr_Format(PyExc_AttributeError,
                     "function attribute without function \"%s\", report to blender.org",
                     attrdef->m_name.c_str());
        return PY_SET_ATTR_FAIL;
      }
      return (*attrdef->m_setFunction)(ref, attrdef, value);
    }
    if (attrdef->m_checkFunction != nullptr || attrdef->m_type == EXP_PYATTRIBUTE_TYPE_VECTOR) {
      // Post check function is provided, prepare undo buffer.
      sourceBuffer = ptr;
      switch (attrdef->m_type) {
        case EXP_PYATTRIBUTE_TYPE_BOOL: {
          bufferSize = sizeof(bool);
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_SHORT: {
          bufferSize = sizeof(short);
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_ENUM:
        case EXP_PYATTRIBUTE_TYPE_FLAG:
        case EXP_PYATTRIBUTE_TYPE_CHAR: {
          bufferSize = attrdef->m_size;
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_INT: {
          bufferSize = sizeof(int);
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_FLOAT: {
          bufferSize = sizeof(float);
          if (attrdef->m_imax) {
            bufferSize *= attrdef->m_imax;
          }
          if (attrdef->m_imin) {
            bufferSize *= attrdef->m_imin;
          }
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_STRING: {
          sourceBuffer = (char *)reinterpret_cast<std::string *>(ptr)->c_str();
          if (sourceBuffer) {
            bufferSize = strlen(reinterpret_cast<char *>(sourceBuffer)) + 1;
          }
          break;
        }
        case EXP_PYATTRIBUTE_TYPE_VECTOR: {
          bufferSize = sizeof(MT_Vector3);
          break;
        }
        default:
          PyErr_Format(PyExc_AttributeError,
                       "unknown type for attribute \"%s\", report to blender.org",
                       attrdef->m_name.c_str());
          return PY_SET_ATTR_FAIL;
      }
      if (bufferSize) {
        undoBuffer = malloc(bufferSize);
        if (undoBuffer) {
          memcpy(undoBuffer, sourceBuffer, bufferSize);
        }
      }
    }

    switch (attrdef->m_type) {
      case EXP_PYATTRIBUTE_TYPE_BOOL: {
        bool *var = reinterpret_cast<bool *>(ptr);
        if (PyLong_Check(value)) {
          *var = (PyLong_AsLong(value) != 0);
        }
        else if (PyBool_Check(value)) {
          *var = (value == Py_True);
        }
        else {
          PyErr_Format(PyExc_TypeError,
                       "expected an integer or a bool for attribute \"%s\"",
                       attrdef->m_name.c_str());
          goto FREE_AND_ERROR;
        }
        break;
      }
      case EXP_PYATTRIBUTE_TYPE_FLAG: {
        bool bval;
        if (PyLong_Check(value)) {
          bval = (PyLong_AsLong(value) != 0);
        }
        else if (PyBool_Check(value)) {
          bval = (value == Py_True);
        }
        else {
          PyErr_Format(PyExc_TypeError,
                       "expected an integer or a bool for attribute \"%s\"",
                       attrdef->m_name.c_str());
          goto FREE_AND_ERROR;
        }
        if (attrdef->m_imax) {
          bval = !bval;
        }
        switch (attrdef->m_size) {
          case 1: {
            unsigned char *val = reinterpret_cast<unsigned char *>(ptr);
            *val = (*val & ~attrdef->m_imin) | ((bval) ? attrdef->m_imin : 0);
            break;
          }
          case 2: {
            unsigned short *val = reinterpret_cast<unsigned short *>(ptr);
            *val = (*val & ~attrdef->m_imin) | ((bval) ? attrdef->m_imin : 0);
            break;
          }
          case 4: {
            unsigned int *val = reinterpret_cast<unsigned int *>(ptr);
            *val = (*val & ~attrdef->m_imin) | ((bval) ? attrdef->m_imin : 0);
            break;
          }
          default:
            PyErr_Format(PyExc_TypeError,
                         "internal error: unsupported flag field \"%s\"",
                         attrdef->m_name.c_str());
            goto FREE_AND_ERROR;
        }
        break;
      }
      case EXP_PYATTRIBUTE_TYPE_SHORT: {
        short int *var = reinterpret_cast<short int *>(ptr);
        if (PyLong_Check(value)) {
          int val = PyLong_AsLong(value);
          if (attrdef->m_clamp) {
            if (val < attrdef->m_imin) {
              val = attrdef->m_imin;
            }
            else if (val > attrdef->m_imax) {
              val = attrdef->m_imax;
            }
          }
          else if (val < attrdef->m_imin || val > attrdef->m_imax) {
            PyErr_Format(PyExc_ValueError,
                         "value out of range for attribute \"%s\"",
                         attrdef->m_name.c_str());
            goto FREE_AND_ERROR;
          }
          *var = (short int)val;
        }
        else {
          PyErr_Format(PyExc_TypeError,
                       "expected an integer for attribute \"%s\"",
                       attrdef->m_name.c_str());
          goto FREE_AND_ERROR;
        }
        break;
      }
      case EXP_PYATTRIBUTE_TYPE_ENUM: {
        // Enum are equivalent to int, just make sure that the field size matches:
        if (sizeof(int) != attrdef->m_size) {
          PyErr_Format(PyExc_AttributeError,
                       "attribute size check error for attribute \"%s\", report to blender.org",
                       attrdef->m_name.c_str());
          goto FREE_AND_ERROR;
        }
        ATTR_FALLTHROUGH;
      }
      case EXP_PYATTRIBUTE_TYPE_INT: {
        int *var = reinterpret_cast<int *>(ptr);
        if (PyLong_Check(value)) {
          int val = PyLong_AsLong(value);
          if (attrdef->m_clamp) {
            if (val < attrdef->m_imin) {
              val = attrdef->m_imin;
            }
            else if (val > attrdef->m_imax) {
              val = attrdef->m_imax;
            }
          }
          else if (val < attrdef->m_imin || val > attrdef->m_imax) {
            PyErr_Format(PyExc_ValueError,
                         "value out of range for attribute \"%s\"",
                         attrdef->m_name.c_str());
            goto FREE_AND_ERROR;
          }
          *var = (int)val;
        }
        else {
          PyErr_Format(PyExc_TypeError,
                       "expected an integer for attribute \"%s\"",
                       attrdef->m_name.c_str());
          goto FREE_AND_ERROR;
        }
        break;
      }
      case EXP_PYATTRIBUTE_TYPE_FLOAT: {
        float *var = reinterpret_cast<float *>(ptr);
        if (attrdef->m_imin != 0) {
          if (attrdef->m_size != attrdef->m_imin * attrdef->m_imax * sizeof(float)) {
            PyErr_Format(PyExc_TypeError,
                         "internal error: incorrect field size for attribute \"%s\"",
                         attrdef->m_name.c_str());
            goto FREE_AND_ERROR;
          }
          if (!PySequence_Check(value) || PySequence_Size(value) != attrdef->m_imin) {
            PyErr_Format(PyExc_TypeError,
                         "expected a sequence of [%d][%d] floats for attribute \"%s\"",
                         attrdef->m_imin,
                         attrdef->m_imax,
                         attrdef->m_name.c_str());
            goto FREE_AND_ERROR;
          }
          for (int i = 0; i < attrdef->m_imin; i++) {
            list = PySequence_GetItem(value, i); /* new ref */
            if (!PySequence_Check(list) || PySequence_Size(list) != attrdef->m_imax) {
              PyErr_Format(PyExc_TypeError,
                           "expected a sequence of [%d][%d] floats for attribute \"%s\"",
                           attrdef->m_imin,
                           attrdef->m_imax,
                           attrdef->m_name.c_str());
              goto RESTORE_AND_ERROR;
            }
            for (int j = 0; j < attrdef->m_imax; j++) {
              item = PySequence_GetItem(list, j); /* new ref */
              if (!py_check_attr_float(var, item, attrdef)) {
                PyErr_Format(PyExc_TypeError,
                             "expected a sequence of [%d][%d] floats for attribute \"%s\"",
                             attrdef->m_imin,
                             attrdef->m_imax,
                             attrdef->m_name.c_str());
                goto RESTORE_AND_ERROR;
              }
              Py_DECREF(item);
              item = nullptr;
              ++var;
            }
            Py_DECREF(list);
            list = nullptr;
          }
        }
        else if (attrdef->m_imax != 0) {
          if (attrdef->m_size != attrdef->m_imax * sizeof(float)) {
            PyErr_Format(PyExc_TypeError,
                         "internal error: incorrect field size for attribute \"%s\"",
                         attrdef->m_name.c_str());
            goto FREE_AND_ERROR;
          }
          if (!PySequence_Check(value) || PySequence_Size(value) != attrdef->m_imax) {
            PyErr_Format(PyExc_TypeError,
                         "expected a sequence of [%d] floats for attribute \"%s\"",
                         attrdef->m_imax,
                         attrdef->m_name.c_str());
            goto FREE_AND_ERROR;
          }
          for (int i = 0; i < attrdef->m_imax; i++) {
            item = PySequence_GetItem(value, i); /* new ref */
            if (!py_check_attr_float(var, item, attrdef)) {
              goto RESTORE_AND_ERROR;
            }
            Py_DECREF(item);
            item = nullptr;
            ++var;
          }
        }
        else {
          if (!py_check_attr_float(var, value, attrdef)) {
            goto FREE_AND_ERROR;
          }
        }
        break;
      }
      case EXP_PYATTRIBUTE_TYPE_VECTOR: {
        if (!PySequence_Check(value) || PySequence_Size(value) != 3) {
          PyErr_Format(PyExc_TypeError,
                       "expected a sequence of 3 floats for attribute \"%s\"",
                       attrdef->m_name.c_str());
          goto FREE_AND_ERROR;
        }
        MT_Vector3 *var = reinterpret_cast<MT_Vector3 *>(ptr);
        for (int i = 0; i < 3; i++) {
          item = PySequence_GetItem(value, i);  // new ref
          float val = PyFloat_AsDouble(item);
          Py_DECREF(item);
          item = nullptr;
          if (val == -1.0f && PyErr_Occurred()) {
            PyErr_Format(PyExc_TypeError,
                         "expected a sequence of 3 floats for attribute \"%s\"",
                         attrdef->m_name.c_str());
            goto RESTORE_AND_ERROR;
          }
          else if (attrdef->m_clamp) {
            if (val < attrdef->m_fmin) {
              val = attrdef->m_fmin;
            }
            else if (val > attrdef->m_fmax) {
              val = attrdef->m_fmax;
            }
          }
          else if (val < attrdef->m_fmin || val > attrdef->m_fmax) {
            PyErr_Format(PyExc_ValueError,
                         "value out of range for attribute \"%s\"",
                         attrdef->m_name.c_str());
            goto RESTORE_AND_ERROR;
          }
          (*var)[i] = (MT_Scalar)val;
        }
        break;
      }
      case EXP_PYATTRIBUTE_TYPE_CHAR: {
        if (PyUnicode_Check(value)) {
          Py_ssize_t val_size;
          const char *val = PyUnicode_AsUTF8AndSize(value, &val_size);
          strncpy(ptr, val, attrdef->m_size);
          ptr[attrdef->m_size - 1] = 0;
        }
        else {
          PyErr_Format(
              PyExc_TypeError, "expected a string for attribute \"%s\"", attrdef->m_name.c_str());
          goto FREE_AND_ERROR;
        }
        break;
      }
      case EXP_PYATTRIBUTE_TYPE_STRING: {
        std::string *var = reinterpret_cast<std::string *>(ptr);
        if (PyUnicode_Check(value)) {
          Py_ssize_t val_len;
          const char *val = PyUnicode_AsUTF8AndSize(
              value, &val_len); /* XXX, should be 'const' but we do a silly trick to have a shorter
                                   string */
          if (attrdef->m_clamp) {
            if (val_len < attrdef->m_imin) {
              // Can't increase the length of the string.
              PyErr_Format(PyExc_ValueError,
                           "string length too short for attribute \"%s\"",
                           attrdef->m_name.c_str());
              goto FREE_AND_ERROR;
            }
            else if (val_len > attrdef->m_imax) {
              // Trim the string.
              *var = var->substr(0, attrdef->m_imax);
              break;
            }
          }
          else if (val_len < attrdef->m_imin || val_len > attrdef->m_imax) {
            PyErr_Format(PyExc_ValueError,
                         "string length out of range for attribute \"%s\"",
                         attrdef->m_name.c_str());
            goto FREE_AND_ERROR;
          }
          *var = val;
        }
        else {
          PyErr_Format(
              PyExc_TypeError, "expected a string for attribute \"%s\"", attrdef->m_name.c_str());
          goto FREE_AND_ERROR;
        }
        break;
      }
      default:
        // Should not happen
        PyErr_Format(PyExc_AttributeError,
                     "unknown type for attribute \"%s\", report to blender.org",
                     attrdef->m_name.c_str());
        goto FREE_AND_ERROR;
    }
  }
  // Check if post processing is needed.
  if (attrdef->m_checkFunction != nullptr) {
    if ((*attrdef->m_checkFunction)(ref, attrdef) != 0) {
      // Restore value.
    RESTORE_AND_ERROR:
      if (undoBuffer) {
        if (attrdef->m_type == EXP_PYATTRIBUTE_TYPE_STRING) {
          // Special case for std::string: restore the string.
          std::string *var = reinterpret_cast<std::string *>(ptr);
          *var = reinterpret_cast<char *>(undoBuffer);
        }
        else {
          // Other field type have direct values.
          memcpy(ptr, undoBuffer, bufferSize);
        }
      }
    FREE_AND_ERROR:
      if (undoBuffer) {
        free(undoBuffer);
      }
      if (list) {
        Py_DECREF(list);
      }
      if (item) {
        Py_DECREF(item);
      }
      return 1;
    }
  }
  if (undoBuffer) {
    free(undoBuffer);
  }
  return 0;
}

/*------------------------------
* EXP_PyObjectPlus repr		-- representations
   ------------------------------*/
PyObject *EXP_PyObjectPlus::py_repr(void)
{
  PyErr_SetString(PyExc_SystemError, "Representation not overridden by object.");
  return nullptr;
}

bool EXP_PyObjectPlus::py_is_valid(void)
{
  return true;
}

PyObject *EXP_PyObjectPlus::GetProxyPlus_Ext(EXP_PyObjectPlus *self, PyTypeObject *tp, void *ptr)
{
  if (self->m_proxy == nullptr) {
    self->m_proxy = reinterpret_cast<PyObject *> PyObject_NEW(EXP_PyObjectPlus_Proxy, tp);
    EXP_PROXY_PYOWNS(self->m_proxy) = false;
    EXP_PROXY_PYREF(self->m_proxy) = true;
#  ifdef USE_WEAKREFS
    EXP_PROXY_WKREF(self->m_proxy) = nullptr;
#  endif
  }

  EXP_PROXY_REF(
      self->m_proxy) = self;  // Its possible this was set to nullptr, so set it back here.
  EXP_PROXY_PTR(self->m_proxy) = ptr;
  Py_INCREF(self->m_proxy);  // We own one, thos ones fore the return.
  return self->m_proxy;
}

PyObject *EXP_PyObjectPlus::NewProxyPlus_Ext(EXP_PyObjectPlus *self,
                                             PyTypeObject *tp,
                                             void *ptr,
                                             bool py_owns)
{
  if (!self) {
    // In case of proxy without reference to game object.
    PyObject *proxy = reinterpret_cast<PyObject *> PyObject_NEW(EXP_PyObjectPlus_Proxy, tp);
    EXP_PROXY_PYREF(proxy) = false;
    EXP_PROXY_PYOWNS(proxy) = py_owns;
    EXP_PROXY_REF(proxy) = nullptr;
    EXP_PROXY_PTR(proxy) = ptr;
#  ifdef USE_WEAKREFS
    EXP_PROXY_WKREF(proxy) = nullptr;
#  endif
    return proxy;
  }
  if (self->m_proxy) {
    if (py_owns) {  // Free
      EXP_PROXY_REF(self->m_proxy) = nullptr;
      Py_DECREF(self->m_proxy);
      self->m_proxy = nullptr;
    }
    else {
      Py_INCREF(self->m_proxy);
      return self->m_proxy;
    }
  }

  GetProxyPlus_Ext(self, tp, ptr);
  if (py_owns) {
    EXP_PROXY_PYOWNS(self->m_proxy) = py_owns;
    Py_DECREF(self->m_proxy);  // Could avoid thrashing here but for now its ok.
  }
  return self->m_proxy;
}

PyObject *PyUnicode_FromStdString(const std::string &str)
{
  return PyUnicode_FromStringAndSize(str.c_str(), str.size());
}

// Deprecation warning management.
bool EXP_PyObjectPlus::m_ignore_deprecation_warnings(false);

bool EXP_PyObjectPlus::GetDeprecationWarnings()
{
  return m_ignore_deprecation_warnings;
}

void EXP_PyObjectPlus::SetDeprecationWarnings(bool ignoreDeprecationWarnings)
{
  m_ignore_deprecation_warnings = ignoreDeprecationWarnings;
}

void EXP_PyObjectPlus::ShowDeprecationWarning_func(const std::string &old_way,
                                                   const std::string &new_way)
{
  CM_PythonWarning("method " << old_way << " is deprecated, please use " << new_way
                             << " instead.");
}

void EXP_PyObjectPlus::ClearDeprecationWarning()
{
  EXP_WarnLink *wlink_next;
  EXP_WarnLink *wlink = GetDeprecationWarningLinkFirst();

  while (wlink) {
    wlink->warn_done = false;  // No need to nullptr the link, its cleared before adding to the
                               // list next time round.
    wlink_next = reinterpret_cast<EXP_WarnLink *>(wlink->link);
    wlink->link = nullptr;
    wlink = wlink_next;
  }
  NullDeprecationWarning();
}

static EXP_WarnLink *m_base_wlink_first = nullptr;
static EXP_WarnLink *m_base_wlink_last = nullptr;

EXP_WarnLink *EXP_PyObjectPlus::GetDeprecationWarningLinkFirst(void)
{
  return m_base_wlink_first;
}

EXP_WarnLink *EXP_PyObjectPlus::GetDeprecationWarningLinkLast(void)
{
  return m_base_wlink_last;
}

void EXP_PyObjectPlus::SetDeprecationWarningFirst(EXP_WarnLink *wlink)
{
  m_base_wlink_first = wlink;
}

void EXP_PyObjectPlus::SetDeprecationWarningLinkLast(EXP_WarnLink *wlink)
{
  m_base_wlink_last = wlink;
}

void EXP_PyObjectPlus::NullDeprecationWarning()
{
  m_base_wlink_first = m_base_wlink_last = nullptr;
}

#endif  // WITH_PYTHON
