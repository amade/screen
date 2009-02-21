/* Lua scripting support
 *
 * Copyright (c) 2008 Sadrul Habib Chowdhury (sadrul@users.sf.net)
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
#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "extern.h"
#include "logfile.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern struct win *windows, *fore;
extern struct display *displays, *display;
extern struct LayFuncs WinLf;
extern struct layer *flayer;

/** Template {{{ */

#define CHECK_TYPE(name, type) \
static type * \
check_##name(lua_State *L, int index) \
{ \
  type **var; \
  luaL_checktype(L, index, LUA_TUSERDATA); \
  var = (type **) luaL_checkudata(L, index, #name); \
  if (!var || !*var) \
    luaL_typerror(L, index, #name); \
  return *var; \
}

#define PUSH_TYPE(name, type) \
static void \
push_##name(lua_State *L, type **t) \
{ \
  if (!t || !*t) \
    lua_pushnil(L); \
  else \
    { \
      type **r; \
      r = (type **)lua_newuserdata(L, sizeof(type *)); \
      *r = *t; \
      luaL_getmetatable(L, #name); \
      lua_setmetatable(L,-2); \
    } \
}

/* Much of the following template comes from:
 *	http://lua-users.org/wiki/BindingWithMembersAndMethods
 */

static int get_int (lua_State *L, void *v)
{
  lua_pushinteger (L, *(int*)v);
  return 1;
}

static int set_int (lua_State *L, void *v)
{
  *(int*)v = luaL_checkint(L, 3);
  return 0;
}

static int get_number (lua_State *L, void *v)
{
  lua_pushnumber(L, *(lua_Number*)v);
  return 1;
}

static int set_number (lua_State *L, void *v)
{
  *(lua_Number*)v = luaL_checknumber(L, 3);
  return 0;
}

static int get_string (lua_State *L, void *v)
{
  lua_pushstring(L, (char*)v );
  return 1;
}

static int set_string (lua_State *L, void *v)
{
  *(const char**)v = luaL_checkstring(L, 3);
  return 0;
}

typedef int (*Xet_func) (lua_State *L, void *v);

/* member info for get and set handlers */
struct Xet_reg
{
  const char *name;  /* member name */
  Xet_func func;     /* get or set function for type of member */
  size_t offset;     /* offset of member within the struct */
  int (*absolute)(lua_State *);
};

static void Xet_add (lua_State *L, const struct Xet_reg *l)
{
  if (!l)
    return;
  for (; l->name; l++)
    {
      lua_pushstring(L, l->name);
      lua_pushlightuserdata(L, (void*)l);
      lua_settable(L, -3);
    }
}

static int Xet_call (lua_State *L)
{
  /* for get: stack has userdata, index, lightuserdata */
  /* for set: stack has userdata, index, value, lightuserdata */
  const struct Xet_reg *m = (const struct Xet_reg *)lua_touserdata(L, -1);  /* member info */
  lua_pop(L, 1);                               /* drop lightuserdata */
  luaL_checktype(L, 1, LUA_TUSERDATA);
  if (m->absolute)
    return m->absolute(L);
  return m->func(L, *(char**)lua_touserdata(L, 1) + m->offset);
}

static int index_handler (lua_State *L)
{
  /* stack has userdata, index */
  lua_pushvalue(L, 2);                     /* dup index */
  lua_rawget(L, lua_upvalueindex(1));      /* lookup member by name */
  if (!lua_islightuserdata(L, -1))
    {
      lua_pop(L, 1);                         /* drop value */
      lua_pushvalue(L, 2);                   /* dup index */
      lua_gettable(L, lua_upvalueindex(2));  /* else try methods */
      if (lua_isnil(L, -1))                  /* invalid member */
	luaL_error(L, "cannot get member '%s'", lua_tostring(L, 2));
      return 1;
    }
  return Xet_call(L);                      /* call get function */
}

static int newindex_handler (lua_State *L)
{
  /* stack has userdata, index, value */
  lua_pushvalue(L, 2);                     /* dup index */
  lua_rawget(L, lua_upvalueindex(1));      /* lookup member by name */
  if (!lua_islightuserdata(L, -1))         /* invalid member */
    luaL_error(L, "cannot set member '%s'", lua_tostring(L, 2));
  return Xet_call(L);                      /* call set function */
}

static int
struct_register(lua_State *L, const char *name, const luaL_reg fn_methods[], const luaL_reg meta_methods[],
    const struct Xet_reg setters[], const struct Xet_reg getters[])
{
  int metatable, methods;

  /* create methods table & add it to the table of globals */
  luaL_register(L, name, fn_methods);
  methods = lua_gettop(L);

  /* create metatable & add it to the registry */
  luaL_newmetatable(L, name);
  luaL_register(L, 0, meta_methods);  /* fill metatable */
  metatable = lua_gettop(L);

  lua_pushliteral(L, "__metatable");
  lua_pushvalue(L, methods);    /* dup methods table*/
  lua_rawset(L, metatable);     /* hide metatable:
				   metatable.__metatable = methods */

  lua_pushliteral(L, "__index");
  lua_pushvalue(L, metatable);  /* upvalue index 1 */
  Xet_add(L, getters);     /* fill metatable with getters */
  lua_pushvalue(L, methods);    /* upvalue index 2 */
  lua_pushcclosure(L, index_handler, 2);
  lua_rawset(L, metatable);     /* metatable.__index = index_handler */

  lua_pushliteral(L, "__newindex");
  lua_newtable(L);              /* table for members you can set */
  Xet_add(L, setters);     /* fill with setters */
  lua_pushcclosure(L, newindex_handler, 1);
  lua_rawset(L, metatable);     /* metatable.__newindex = newindex_handler */

  lua_pop(L, 1);                /* drop metatable */
  return 1;                     /* return methods on the stack */
}

/** }}} */

/** Window {{{ */

PUSH_TYPE(window, struct win)

CHECK_TYPE(window, struct win)

static int get_window(lua_State *L, void *v)
{
  push_window(L, (struct win **)v);
  return 1;
}

static int
window_change_title(lua_State *L)
{
  struct win *w = check_window(L, 1);
  unsigned int len;
  const char *title = luaL_checklstring(L, 2, &len);
  ChangeAKA(w, title, len);
  return 0;
}

static const luaL_reg window_methods[] = {
  {"change_title", window_change_title},
  {0, 0}
};

static int
window_tostring(lua_State *L)
{
  char str[128];
  struct win *w = check_window(L, 1);
  snprintf(str, sizeof(str), "{window #%d: '%s'}", w->w_number, w->w_title);
  lua_pushstring(L, str);
  return 1;
}

static int
window_equality(lua_State *L)
{
  struct win *w1 = check_window(L, 1), *w2 = check_window(L, 2);
  lua_pushboolean(L, (w1 == w2 || (w1 && w2 && w1->w_number == w2->w_number)));
  return 1;
}

static const luaL_reg window_metamethods[] = {
  {"__tostring", window_tostring},
  {"__eq", window_equality},
  {0, 0}
};

static const struct Xet_reg window_setters[] = {
  {0, 0}
};

static const struct Xet_reg window_getters[] = {
  {"title", get_string, offsetof(struct win, w_title) + 8},
  {"number", get_int, offsetof(struct win, w_number)},
  {"dir", get_string, offsetof(struct win, w_dir)},
  {"tty", get_string, offsetof(struct win, w_tty)},
  {"pid", get_int, offsetof(struct win, w_pid)},
  {"group", get_window, offsetof(struct win, w_group)},
  {"bell", get_int, offsetof(struct win, w_bell)},
  {"monitor", get_int, offsetof(struct win, w_monitor)},
  {0, 0}
};


/** }}} */

/** AclUser {{{ */

PUSH_TYPE(user, struct acluser)

CHECK_TYPE(user, struct acluser)

static int
get_user(lua_State *L, void *v)
{
  push_user(L, (struct acluser **)v);
  return 1;
}

static const luaL_reg user_methods[] = {
  {0, 0}
};

static int
user_tostring(lua_State *L)
{
  char str[128];
  struct acluser *u = check_user(L, 1);
  snprintf(str, sizeof(str), "{user '%s'}", u->u_name);
  lua_pushstring(L, str);
  return 1;
}

static const luaL_reg user_metamethods[] = {
  {"__tostring", user_tostring},
  {0, 0}
};

static const struct Xet_reg user_setters[] = {
  {0, 0}
};

static const struct Xet_reg user_getters[] = {
  {"name", get_string, offsetof(struct acluser, u_name)},
  {"password", get_string, offsetof(struct acluser, u_password)},
  {0, 0}
};

/** }}} */

/** Canvas {{{ */

PUSH_TYPE(canvas, struct canvas)

CHECK_TYPE(canvas, struct canvas)

static int
get_canvas(lua_State *L, void *v)
{
  push_canvas(L, (struct canvas **)v);
  return 1;
}

static int
canvas_select(lua_State *L)
{
  struct canvas *c = check_canvas(L, 1);
  if (!display || D_forecv == c)
    return 0;
  SetCanvasWindow(c, Layer2Window(c->c_layer));
  D_forecv = c;

  /* XXX: the following all is duplicated from process.c:DoAction.
   * Should these be in some better place?
   */
  ResizeCanvas(&D_canvas);
  RecreateCanvasChain();
  RethinkDisplayViewports();
  ResizeLayersToCanvases();	/* redisplays */
  fore = D_fore = Layer2Window(D_forecv->c_layer);
  flayer = D_forecv->c_layer;
#ifdef RXVT_OSC
  if (D_xtermosc[2] || D_xtermosc[3])
    {
      Activate(-1);
      break;
    }
#endif
  RefreshHStatus();
#ifdef RXVT_OSC
  RefreshXtermOSC();
#endif
  flayer = D_forecv->c_layer;
  CV_CALL(D_forecv, LayRestore();LaySetCursor());
  WindowChanged(0, 'F');
  return 1;
}

static const luaL_reg canvas_methods[] = {
  {"select", canvas_select},
  {0, 0}
};

static const luaL_reg canvas_metamethods[] = {
  {0, 0}
};

static const struct Xet_reg canvas_setters[] = {
  {0, 0}
};

static int
canvas_get_window(lua_State *L)
{
  struct canvas *c = check_canvas(L, 1);
  struct win *win = Layer2Window(c->c_layer);
  if (win)
    push_window(L, &win);
  else
    lua_pushnil(L);
  return 1;
}

static const struct Xet_reg canvas_getters[] = {
  {"next", get_canvas, offsetof(struct canvas, c_next)},
  {"xoff", get_int, offsetof(struct canvas, c_xoff)},
  {"yoff", get_int, offsetof(struct canvas, c_yoff)},
  {"xs", get_int, offsetof(struct canvas, c_xs)},
  {"ys", get_int, offsetof(struct canvas, c_ys)},
  {"xe", get_int, offsetof(struct canvas, c_xe)},
  {"ye", get_int, offsetof(struct canvas, c_ye)},
  {"window", 0, 0, canvas_get_window},
  {0, 0}
};

/** }}} */

/** Display {{{ */

PUSH_TYPE(display, struct display)

CHECK_TYPE(display, struct display)

static int
display_get_canvases(lua_State *L)
{
  struct display *d;
  struct canvas *iter;
  int count;

  d = check_display(L, 1);
  for (iter = d->d_cvlist, count = 0; iter; iter = iter->c_next, count++)
    push_canvas(L, &iter);

  return count;
}

static const luaL_reg display_methods[] = {
  {"get_canvases", display_get_canvases},
  {0, 0}
};

static int
display_tostring(lua_State *L)
{
  char str[128];
  struct display *d = check_display(L, 1);
  snprintf(str, sizeof(str), "{display: tty = '%s', term = '%s'}", d->d_usertty, d->d_termname);
  lua_pushstring(L, str);
  return 1;
}

static const luaL_reg display_metamethods[] = {
  {"__tostring", display_tostring},
  {0, 0}
};

static const struct Xet_reg display_setters[] = {
  {0, 0}
};

static const struct Xet_reg display_getters[] = {
  {"tty", get_string, offsetof(struct display, d_usertty)},
  {"term", get_string, offsetof(struct display, d_termname)},
  {"fore", get_window, offsetof(struct display, d_fore)},
  {"other", get_window, offsetof(struct display, d_other)},
  {"width", get_int, offsetof(struct display, d_width)},
  {"height", get_int, offsetof(struct display, d_height)},
  {"user", get_user, offsetof(struct display, d_user)},
  {0, 0}
};

/** }}} */

/** Screen {{{ */

static int
screen_get_windows(lua_State *L)
{
  struct win *iter;
  int count;

  for (iter = windows, count = 0; iter; iter = iter->w_next, count++)
    push_window(L, &iter);

  return count;
}

static int
screen_get_displays(lua_State *L)
{
  struct display *iter;
  int count;

  for (iter = displays, count = 0; iter; iter = iter->d_next, count++)
    push_display(L, &iter);

  return count;
}

static int
screen_get_display(lua_State *L)
{
  push_display(L, &display);
  return 1;
}

static int
screen_exec_command(lua_State *L)
{
  const char *command;
  unsigned int len;

  command = luaL_checklstring(L, 1, &len);
  if (command)
    RcLine(command, len);

  return 0;
}

static int
screen_append_msg(lua_State *L)
{
  const char *msg, *color;
  int len;
  msg = luaL_checklstring(L, 1, &len);
  if (lua_isnil(L, 2))
    color = NULL;
  else
    color = luaL_checklstring(L, 2, &len);
  AppendWinMsgRend(msg, color);
  return 0;
}

static const luaL_reg screen_methods[] = {
  {"windows", screen_get_windows},
  {"displays", screen_get_displays},
  {"display", screen_get_display},
  {"command", screen_exec_command},
  {"append_msg", screen_append_msg},
  {0, 0}
};

static const luaL_reg screen_metamethods[] = {
  {0, 0}
};

static const struct Xet_reg screen_setters[] = {
  {0, 0}
};

static const struct Xet_reg screen_getters[] = {
  {0, 0}
};

/** }}} */

/** Public functions {{{ */
static lua_State *L;
int LuaInit(void)
{
  L = luaL_newstate();

  luaL_openlibs(L);

#define REGISTER(X) struct_register(L, #X , X ## _methods, X##_metamethods, X##_setters, X##_getters)

  REGISTER(screen);
  REGISTER(window);
  REGISTER(display);
  REGISTER(user);
  REGISTER(canvas);

  return 0;
}

struct fn_def
{
  void (*push_fn)(lua_State *, void*);
  void *value;
};

static int
LuaCallProcess(const char *name, struct fn_def defs[])
{
  int argc = 0;

  lua_settop(L, 0);
  lua_getfield(L, LUA_GLOBALSINDEX, name);
  if (lua_isnil(L, -1))
    return 0;
  for (argc = 0; defs[argc].push_fn; argc++)
    defs[argc].push_fn(L, defs[argc].value);
  if (lua_pcall(L, argc, 0, 0) == LUA_ERRRUN && lua_isstring(L, -1))
    {
      struct display *d = display;
      unsigned int len;
      char *message = luaL_checklstring(L, -1, &len);
      LMsg(1, "%s", message ? message : "Unknown error");
      lua_pop(L, 1);
      display = d;
      return 0;
    }
  return 1;
}

int LuaForeWindowChanged(void)
{
  if (!L)
    return 0;
  lua_getfield(L, LUA_GLOBALSINDEX, "fore_changed");
  if (lua_isnil(L, -1))
    return 0;
  push_display(L, &display);
  push_window(L, display ? &D_fore : &fore);
  if (lua_pcall(L, 2, 0, 0) == LUA_ERRRUN)
    {
      if(lua_isstring(L, -1))
	{
	  unsigned int len;
	  char *message = luaL_checklstring(L, -1, &len);
	  LMsg(1, "%s", message ? message : "Unknown error");
	  lua_pop(L, 1);
	}
    }
  return 0;
}

int LuaSource(const char *file)
{
  if (!L)
    return 0;
  struct stat st;
  if (stat(file, &st) == -1)
    Msg(errno, "Error loading lua script file '%s'", file);
  else
    {
      int len = strlen(file);
      if (len < 4 || strncmp(file + len - 4, ".lua", 4) != 0)
	return 0;
      luaL_dofile(L, file);
      return 1;
    }
  return 0;
}

int LuaFinit(void)
{
  if (!L)
    return 0;
  lua_close(L);
  L = (lua_State*)0;
  return 0;
}

int LuaCall(char **argv)
{
  int argc;
  if (!L)
    return 0;

  lua_getfield(L, LUA_GLOBALSINDEX, *argv);
  for (argc = 0, argv++; *argv; argv++, argc++)
    {
      lua_pushstring(L, *argv);
    }
  if (lua_pcall(L, argc, 0, 0) == LUA_ERRRUN)
    {
      if(lua_isstring(L, -1))
	{
	  unsigned int len;
	  char *message = luaL_checklstring(L, -1, &len);
	  LMsg(1, "%s", message ? message : "Unknown error");
	  lua_pop(L, 1);
	  return 0;
	}
    }
  return 1;
}

int
LuaProcessCaption(const char *caption, struct win *win, int len)
{
  if (!L)
    return 0;
  struct fn_def params[] = {
    {lua_pushstring, caption},
    {push_window, &win},
    {lua_pushinteger, len},
    {NULL, NULL}
  };
  return LuaCallProcess("process_caption", params);
}

static void
push_stringarray(lua_State *L, void *data)
{
  char **args = (char **)data;
  int i;
  lua_newtable(L);
  for (i = 1; args && *args; i++) {
    lua_pushinteger(L, i);
    lua_pushstring(L, *args++);
    lua_settable(L, -3);
  }
}

int
LuaCommandExecuted(const char *command, const char **args, int argc)
{
  if (!L)
    return 0;
  struct fn_def params[] = {
      {lua_pushstring, command},
      {push_stringarray, args},
      {NULL, NULL}
  };
  return LuaCallProcess("command_executed", params);
}

/** }}} */

struct ScriptFuncs LuaFuncs =
{
  LuaInit,
  LuaFinit,
  LuaForeWindowChanged,
  LuaSource,
  LuaProcessCaption,
  LuaCommandExecuted
};

