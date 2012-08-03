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

#define RETURN_NONE  do { Py_INCREF(Py_None); return Py_None; } while (0)

static PyObject *SPy_Get(PyObject * obj, void *closure);
static int SPy_Set(PyObject * obj, PyObject * value, void *closure);
static int PyDispatch(void *handler, const char *params, va_list va);
static PyObject *register_event_hook(PyObject * self, PyObject * args, PyObject * kw, void *object);

typedef struct {
	PyObject *callback;
	struct listener *listener;
} SPyCallback;

typedef struct {
	char *name;
	char *doc;

	int type;
	size_t offset1;
	size_t offset2;
	PyObject *(*conv) (void *);
	int (*setter) (void *, PyObject *);
} SPyClosure;

#define compare_callback NULL
#define repr_callback NULL

#define REGISTER_TYPE(type, Type, closures, methods) \
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
  PyType##Type.tp_base = &ScreenObjectType; \
  PyType##Type.tp_getset = getsets; \
  PyType##Type.tp_methods = methods; \
  PyType##Type.tp_compare = compare_##type; \
  PyType##Type.tp_repr = repr_##type; \
  PyType_Ready(&PyType##Type); \
  Py_INCREF(&PyType##Type); \
  PyModule_AddObject(module, #Type, (PyObject *)&PyType##Type); \
  return 0; \
}

#define DEFINE_TYPE(str, Type) \
typedef struct \
{ \
  ScreenObject __parent; \
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
  Py##Type *obj = (Py##Type *)PyType##Type.tp_alloc(&PyType##Type, 0); \
  obj->_obj = _obj; \
  return (PyObject *)obj; \
}

static PyObject *PyString_FromStringSafe(const char *str)
{
	if (str)
		return PyString_FromString(str);
	RETURN_NONE;
}

/** Generic Object {{{ */
typedef struct {
	PyObject_HEAD char *name;
} ScreenObject;

static PyObject *object_hook(PyObject * self, PyObject * args, PyObject * kw)
{
	char *object;
	object = *(char **)((char *)self + sizeof(ScreenObject));	/* ugliness */
	return register_event_hook(self, args, kw, object);
}

static PyMethodDef omethods[] = {
	{"hook", (PyCFunction) object_hook, METH_VARARGS | METH_KEYWORDS, "Hook to an event on the object."},
	{NULL}
};

static PyTypeObject ScreenObjectType = {
	PyObject_HEAD_INIT(NULL)
	    .ob_size = 0,
	.tp_name = "screen.Object",
	.tp_basicsize = sizeof(ScreenObject),
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_doc = "Generic object",
	.tp_methods = omethods,
	.tp_getset = NULL,
};

static int register_object(PyObject * module)
{
	PyType_Ready(&ScreenObjectType);
	Py_INCREF(&ScreenObjectType);
	PyModule_AddObject(module, "Generic Object", (PyObject *) & ScreenObjectType);
	return 0;
}

/** }}} */

/** Window {{{ */
DEFINE_TYPE(struct win, Window)

static int window_set_title(void *win, PyObject * value)
{
	char *string = PyString_AsString(value);
	if (!string || !*string)
		return -1;
	ChangeAKA((struct win *)win, string, strlen(string));
	return 0;
}

#define SPY_CLOSURE(name, doc, type, member, func, setter) \
    {name, doc, type, offsetof(PyWindow, _obj), offsetof(struct win, member), func, setter}
static SPyClosure wclosures[] = {
	SPY_CLOSURE("title", "Window title", T_STRING, w_title, NULL, window_set_title),
	SPY_CLOSURE("number", "Window number", T_INT, w_number, NULL, NULL),
	SPY_CLOSURE("dir", "Window directory", T_STRING, w_dir, NULL, NULL),
	SPY_CLOSURE("tty", "TTY belonging to the window", T_STRING_INPLACE, w_tty, NULL, NULL),
	SPY_CLOSURE("group", "The group the window belongs to", T_OBJECT_EX, w_group, (PyObject *(*)(void *))PyObject_FromWindow, NULL),
	SPY_CLOSURE("pid", "Window pid", T_INT, w_pid, NULL, NULL),
	{NULL}
};

static PyObject *window_select(PyObject * self)
{
	PyWindow *win = self;
	struct win *w = win->_obj;
	SwitchWindow(w->w_number);
	RETURN_NONE;
}

static PyMethodDef wmethods[] = {
	{"select", (PyCFunction) window_select, METH_NOARGS, "Select the window."},
	{NULL},
};

static int compare_window(PyWindow * one, PyWindow * two)
{
	struct win *wone = one->_obj;
	struct win *wtwo = two->_obj;

	return wtwo->w_number - wone->w_number;
}

static PyObject *repr_window(PyObject * obj)
{
	PyWindow *w = obj;
	struct win *win = w->_obj;
	return PyString_FromFormat("window (title: %s, number: %d)", win->w_title, win->w_number);
}

REGISTER_TYPE(window, Window, wclosures, wmethods)
#undef SPY_CLOSURE
/** }}} */
/** Display {{{ */
    DEFINE_TYPE(struct display, Display)
#define SPY_CLOSURE(name, doc, type, member, func) \
    {name, doc, type, offsetof(PyDisplay, _obj), offsetof(struct display, member), func}
static SPyClosure dclosures[] = {
	SPY_CLOSURE("tty", "Display TTY", T_STRING_INPLACE, d_usertty, NULL),
	SPY_CLOSURE("term", "Display Term", T_STRING_INPLACE, d_termname, NULL),
	SPY_CLOSURE("fore", "Foreground window of the display", T_OBJECT_EX, d_fore, PyObject_FromWindow),
	SPY_CLOSURE("width", "Display width", T_INT, d_width, NULL),
	SPY_CLOSURE("height", "Display height", T_INT, d_height, NULL),
	{NULL}
};

static PyMethodDef dmethods[] = {
	{NULL}
};

static int compare_display(PyDisplay * one, PyDisplay * two)
{
	struct display *done = one->_obj;
	struct display *dtwo = two->_obj;
	struct display *iter;

	if (done->d_userpid == dtwo->d_userpid)
		return 0;

	for (iter = displays; iter; iter = iter->d_next)
		if (iter == done)
			return -1;
		else if (iter == dtwo)
			return 1;

	return 0;
}

static PyObject *repr_display(PyObject * obj)
{
	PyDisplay *d = obj;
	struct display *disp = d->_obj;
	return PyString_FromFormat("display (term: %s, tty: %s)", disp->d_termname, disp->d_usertty);
}

REGISTER_TYPE(display, Display, dclosures, dmethods)
#undef SPY_CLOSURE
/** }}} */
/** Callback {{{ */
    DEFINE_TYPE(SPyCallback, Callback)
static SPyClosure cclosures[] = { {NULL} };

static void FreeCallback(SPyCallback * scallback)
{
	Py_XDECREF(scallback->callback);
	Free(scallback);
}

static PyObject *callback_unhook(PyObject * obj)
{
	PyCallback *cb = obj;
	SPyCallback *scallback = cb->_obj;
	if (!scallback)
		return NULL;
	unregister_listener(scallback->listener);
	FreeCallback(scallback);
	cb->_obj = NULL;
	RETURN_NONE;
}

static PyMethodDef cmethods[] = {
	{"unhook", (PyCFunction) callback_unhook, METH_NOARGS, "Unhook this event callback."},
	{NULL}
};

REGISTER_TYPE(callback, Callback, cclosures, cmethods)
/** }}} */
static PyObject *SPy_Get(PyObject * obj, void *closure)
{
	SPyClosure *sc = closure;
	char **first = (char *)obj + sc->offset1;
	char **second = (char *)*first + sc->offset2;
	PyObject *(*cb) (void *) = sc->conv;
	void *data = *second;

	if (!cb) {
		switch (sc->type) {
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

static int SPy_Set(PyObject * obj, PyObject * value, void *closure)
{
	SPyClosure *sc = closure;
	char **first = (char *)obj + sc->offset1;
	if (value == NULL) {
		PyErr_SetString(PyExc_TypeError, "Cannot remove an attribute value.");
		return -1;
	}
	if (sc->setter == NULL) {
		char str[256];
		snprintf(str, sizeof(str) - 1, "Cannot change '%s'.", sc->name);
		PyErr_SetString(PyExc_TypeError, str);
		return -1;
	}

	return sc->setter(*(void **)first, value);
}

static int PyDispatch(void *handler, const char *params, va_list va)
{
	PyCallback *callback = handler;
	PyObject *args, *ret;
	int count, retval;
	const char *p;
	SPyCallback *scallback = callback->_obj;

	for (count = 0, p = params; *p; p++, count++) ;
	if (count > 0)
		args = PyTuple_New(count);
	else
		args = NULL;

	for (count = 0, p = params; *p; p++, count++) {
		PyObject *item = NULL;
		switch (*p) {
		case 's':
			item = PyString_FromStringSafe(va_arg(va, char *));
			break;
		case 'S':
			{
				char **ls = va_arg(va, char **), **iter;
				int c = 0;
				for (iter = ls; iter && *iter; iter++, c++) ;
				if (c == 0)
					break;
				item = PyTuple_New(c);
				for (c = 0, iter = ls; iter && *iter; iter++, c++)
					PyTuple_SetItem(item, c, PyString_FromStringSafe(*iter));
			}
			break;
		case 'i':
			item = PyInt_FromLong(va_arg(va, int));
			break;
		case 'd':
			item = PyObject_FromDisplay(va_arg(va, struct display *));
			break;
		}

		if (!item) {
			item = Py_None;
			Py_INCREF(Py_None);
		}
		PyTuple_SetItem(args, count, item);
	}

	ret = PyObject_CallObject(scallback->callback, args);
	Py_DECREF(args);
	if (!ret)
		return 0;

	retval = (int)PyInt_AsLong(ret);
	Py_DECREF(ret);
	return retval;
}

static PyObject *register_event_hook(PyObject * self, PyObject * args, PyObject * kw, void *object)
{
	static char *kwlist[] = { "event", "callback", NULL };

	PyObject *callback;
	char *name;

	struct script_event *sev;
	struct listener *l;
	SPyCallback *scallback;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "sO:screen.hook", kwlist, &name, &callback))
		return NULL;	/* Return Py_None instead? */

	if (!PyCallable_Check(callback)) {
		PyErr_SetString(PyExc_TypeError, "The event-callback functions must be callable.");
		LMsg(0, "The event-callback functions must be callable.");
		return NULL;
	}

	sev = object_get_event(object, name);
	if (!sev) {
		LMsg(0, "No event named '%s'", name);
		return NULL;
	}

	l = malloc(sizeof(struct listener));

	scallback = malloc(sizeof(SPyCallback));
	scallback->callback = callback;
	scallback->listener = l;
	Py_INCREF(scallback->callback);

	l->handler = PyObject_FromCallback(scallback);
	l->priv = 0;
	l->dispatcher = PyDispatch;
	if (register_listener(sev, l) == 0) {
		Py_DECREF((PyObject *) l->handler);
		FreeCallback(scallback);
		Free(l);

		LMsg(0, "Hook could not be registered.");

		RETURN_NONE;
	}

	Py_INCREF((PyObject *) l->handler);
	return l->handler;

	RETURN_NONE;
}

/** Screen {{{ */
static PyObject *screen_display(PyObject * self)
{
	if (!display) {
		RETURN_NONE;
	}
	return PyObject_FromDisplay(display);
}

static PyObject *screen_displays(PyObject * self)
{
	struct display *d = displays;
	int count = 0;
	for (; d; d = d->d_next)
		++count;
	PyObject *tuple = PyTuple_New(count);

	for (d = displays, count = 0; d; d = d->d_next, ++count)
		PyTuple_SetItem(tuple, count, PyObject_FromDisplay(d));

	return tuple;
}

static PyObject *screen_windows(PyObject * self)
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

static PyObject *hook_event(PyObject * self, PyObject * args, PyObject * kw)
{
	return register_event_hook(self, args, kw, NULL);
}

static void screen_input_cb(char *buf, int len, char *p)
{
	PyObject *callback = p;
	PyObject *str = PyTuple_New(1);
	PyTuple_SetItem(str, 0, PyString_FromStringSafe(buf));
	PyObject_CallObject(callback, str);
	Py_DECREF(str);
	Py_DECREF(callback);
}

static PyObject *screen_input(PyObject * self, PyObject * args, PyObject * kw)
{
	static char *kwlist[] = { "prompt", "callback", "value (optional)", NULL };
	char *prompt, *pre = NULL;
	PyObject *callback;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "sO|s:screen.input", kwlist, &prompt, &callback, &pre)) {
		LMsg(0, "Could not parse all the parameters to screen.input call.");
		return NULL;
	}

	if (!PyCallable_Check(callback)) {
		LMsg(0, "Input callback must be a callable object.");
		return NULL;
	}

	Py_INCREF(callback);
	Input(prompt, 100 /* huh? */ ,
	      INP_COOKED, screen_input_cb, callback, 0);

	if (pre && *pre) {
		int len = strlen(pre);
		LayProcess(&pre, &len);
	}

	RETURN_NONE;
}

PyMethodDef py_methods[] = {
	{"display", (PyCFunction) screen_display, METH_NOARGS, "Get the current display."},
	{"displays", (PyCFunction) screen_displays, METH_NOARGS, "Get the list of displays."},
	{"hook", (PyCFunction) hook_event, METH_VARARGS | METH_KEYWORDS, "Hook a callback to an event."},
	{"input", (PyCFunction) screen_input, METH_VARARGS | METH_KEYWORDS, "Read user input interactively."},
	{"windows", (PyCFunction) screen_windows, METH_NOARGS, "Get the list of windows."},
	{NULL, NULL, 0, NULL}
};

/** }}} */

static PyObject *module;
static int SPyInit(void)
{
	PyObject *m;

	Py_Initialize();

	m = Py_InitModule3("screen", py_methods, NULL);
	register_object(m);
	register_window(m);
	register_display(m);
	register_callback(m);
	module = m;

	return 0;
}

static int SPyFinit(void)
{
	Py_Finalize();
	return 0;
}

static int SPySource(const char *file, int async)
{
	FILE *f = fopen(file, "rb");
	int ret = PyRun_SimpleFile(f, file);
	fclose(f);

	if (ret == 0)
		return 1;	/* Success */

	if (PyErr_Occurred()) {
		PyErr_Print();
		return 0;
	}

	return 1;
}

static int SPyCall(const char *func, const char **argv)
{
	PyObject *dict = PyModule_GetDict(module);
	PyObject *f = PyDict_GetItemString(dict, func);
	PyObject *ret;
	if (!f) {
		return 0;
	}
	ret = PyEval_CallFunction(f, "s", argv[0]);
	Py_XDECREF(ret);
	return 1;
}

struct binding py_binding = {
	"python",
	0,
	0,
	SPyInit,
	SPyFinit,
	SPyCall,
	SPySource,
	0,
	0
};
