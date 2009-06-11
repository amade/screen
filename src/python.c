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

/** Window {{{ */
typedef struct
{
  PyObject_HEAD
  struct win w;
} PyWindow;

static PyMemberDef py_window_members[] =
{
  {"title", T_STRING, offsetof(PyWindow, w) + offsetof(struct win, w_title), 0, "Title of the window"},
  {"number", T_INT, offsetof(PyWindow, w) + offsetof(struct win, w_number), 0, "The window number"},
  {NULL}
};

static PyTypeObject PyWindowType =
{
  PyObject_HEAD_INIT(NULL)
  .ob_size = 0,
  .tp_name = "screen.window",
  .tp_basicsize = sizeof(PyWindow),
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Window object",
  .tp_methods = NULL,
  .tp_members = py_window_members
};

static PyObject *
PyWindow_FromWindow(struct win *w)
{
  PyWindow *obj = PyWindowType.tp_alloc(&PyWindowType, 0);
  obj->w = *w;
  return (PyObject *)obj;
}

/** }}} */

static PyObject *
screen_windows(PyObject *self)
{
  struct win *w = windows;
  int count = 0;
  for (; w; w = w->w_next)
    ++count;
  PyObject *tuple = PyTuple_New(count);

  for (w = windows, count = 0; w; w = w->w_next, ++count)
    {
      PyTuple_SetItem(tuple, count, PyWindow_FromWindow(w));
    }

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

  PyType_Ready(&PyWindowType);

  m = Py_InitModule3 ("screen", py_methods, NULL);

  Py_INCREF(&PyWindowType);
  PyModule_AddObject(m, "Window", (PyObject *)&PyWindowType);
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

