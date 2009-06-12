/* Python scripting support
 *
 * Copyright (c) 2009 Sadrul Habib Chowdhury (sadrul@users.sf.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 ****************************************************************
 */
#include <sys/types.h>

#include "config.h"
#include "screen.h"
#include "script.h"
#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "extern.h"
#include "logfile.h"

#include <Python.h>
#include <structmember.h>

extern struct win *windows;

static PyObject * SPy_Get(PyObject *obj, void *closure);
static PyObject * SPy_Set(PyObject *obj, PyObject *value, void *closure);

typedef struct
{
  char *name;
  char *doc;

  int type;
  size_t offset1;
  size_t offset2;
  PyObject * (*conv)(void *);
} SPyClosure;

#define REGISTER_TYPE(type, Type, closures) \
static int \
register_##type(PyObject *module) \
{ \
  static PyGetSetDef getsets[sizeof(closures)]; \
  int i, count = sizeof(closures); \
  for (i = 0; i < count; i++) \
    { \
      getsets[i].name = closures[i].name; \
      getsets[i].doc = closures[i].doc; \
      getsets[i].closure = &closures[i]; \
      getsets[i].get = SPy_Get; \
      getsets[i].set = SPy_Set; \
    } \
  PyType##Type.tp_getset = getsets; \
  PyType_Ready(&PyType##Type); \
  Py_INCREF(&PyType##Type); \
  PyModule_AddObject(module, #Type, (PyObject *)&PyType##Type); \
  return 1; \
}

#define DEFINE_TYPE(str, Type) \
typedef struct \
{ \
  PyObject_HEAD \
  str *_obj; \
} Py##Type; \
\
static PyTypeObject PyType##Type = \
{ \
  PyObject_HEAD_INIT(NULL) \
  .ob_size = 0, \
  .tp_name = "screen." #Type, \
  .tp_basicsize = sizeof(Py##Type), \
  .tp_flags = Py_TPFLAGS_DEFAULT, \
  .tp_doc = #Type " object", \
  .tp_methods = NULL, \
  .tp_getset = NULL, \
}; \
\
static PyObject * \
PyObject_From##Type(str *_obj) \
{ \
  Py##Type *obj = PyType##Type.tp_alloc(&PyType##Type, 0); \
  obj->_obj = _obj; \
  return (PyObject *)obj; \
}

static PyObject *
PyString_FromStringSafe(const char *str)
{
  if (str)
    return PyString_FromString(str);
  Py_INCREF(Py_None);
  return Py_None;
}

/** Window {{{ */
DEFINE_TYPE(struct win, Window)

#define SPY_CLOSURE(name, doc, type, member, func) \
    {name, doc, type, offsetof(PyWindow, _obj), offsetof(struct win, member), func}
static SPyClosure wclosures[] =
{
  SPY_CLOSURE("title", "Window title", T_STRING, w_title, NULL),
  SPY_CLOSURE("number", "Window number", T_INT, w_number, NULL),
  SPY_CLOSURE("dir", "Window directory", T_STRING, w_dir, NULL),
  SPY_CLOSURE("tty", "TTY belonging to the window", T_STRING_INPLACE, w_tty, NULL),
  SPY_CLOSURE("group", "The group the window belongs to", T_OBJECT_EX, w_group, PyObject_FromWindow),
  SPY_CLOSURE("pid", "Window pid", T_INT, w_pid, NULL),
  {NULL}
};
REGISTER_TYPE(window, Window, wclosures)
#undef SPY_CLOSURE

/** }}} */

static PyObject *
SPy_Get(PyObject *obj, void *closure)
{
  SPyClosure *sc = closure;
  char **first = (char *)obj + sc->offset1;
  char **second = (char *)*first + sc->offset2;
  PyObject *(*cb)(void *) = sc->conv;
  void *data = *second;

  if (!cb)
    {
      switch (sc->type)
	{
	  case T_STRING:
	    cb = PyString_FromStringSafe;
	    data = *second;
	    break;
	  case T_STRING_INPLACE:
	    cb = PyString_FromStringSafe;
	    data = second;
	    break;
	  case T_INT:
	    cb = PyInt_FromLong;
	    data = *second;
	    break;
	}
    }
  return cb(data);
}

static PyObject *
SPy_Set(PyObject *obj, PyObject *value, void *closure)
{
  return NULL;
}

static PyObject *
screen_windows(PyObject *self)
{
  struct win *w = windows;
  int count = 0;
  for (; w; w = w->w_next)
    ++count;
  PyObject *tuple = PyTuple_New(count);

  for (w = windows, count = 0; w; w = w->w_next, ++count)
    PyTuple_SetItem(tuple, count, PyObject_FromWindow(w));

  return tuple;
}

const PyMethodDef py_methods[] = {
  {"windows", (PyCFunction)screen_windows, METH_NOARGS, NULL},
  {NULL, NULL, 0, NULL}
};

static int
SPyInit(void)
{
  PyObject *m;

  Py_Initialize();

  m = Py_InitModule3 ("screen", py_methods, NULL);
  register_window(m);

  return 0;
}

static int
SPyFinit(void)
{
  /*Py_Finalize(); // Crashes -- why? */
  return 0;
}

static int
SPySource(const char *file, int async)
{
  FILE *f = fopen(file, "rb");
  int ret = PyRun_SimpleFile(f, file);
  fclose(f);

  if (ret == 0)
      return 1; /* Success */

  if (PyErr_Occurred())
    {
      PyErr_Print();
      return 0;
    }

  return 1;
}

struct binding py_binding =
{
  "python",
  0,
  0,
  SPyInit,
  SPyFinit,
  0,
  SPySource,
  0,
  0
};

