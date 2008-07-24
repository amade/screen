#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"

#include "screen.h"
#include "extern.h"
#include "logfile.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define PACKAGE  "screen"

extern struct win *windows, *fore;
extern struct display *displays, *display;

/** Template {{{ */

/* Much of the following code comes from:
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
typedef const struct Xet_reg {
  const char *name;  /* member name */
  Xet_func func;     /* get or set function for type of member */
  size_t offset;     /* offset of member within your_t */
} *Xet_reg;

static void Xet_add (lua_State *L, Xet_reg l)
{
  if (!l)
    return;
  for (; l->name; l++) {
    lua_pushstring(L, l->name);
    lua_pushlightuserdata(L, (void*)l);
    lua_settable(L, -3);
  }
}

static int Xet_call (lua_State *L)
{
  /* for get: stack has userdata, index, lightuserdata */
  /* for set: stack has userdata, index, value, lightuserdata */
  Xet_reg m = (Xet_reg)lua_touserdata(L, -1);  /* member info */
  lua_pop(L, 1);                               /* drop lightuserdata */
  luaL_checktype(L, 1, LUA_TUSERDATA);
  return m->func(L, lua_touserdata(L, 1) + m->offset);
}

static int index_handler (lua_State *L)
{
  /* stack has userdata, index */
  lua_pushvalue(L, 2);                     /* dup index */
  lua_rawget(L, lua_upvalueindex(1));      /* lookup member by name */
  if (!lua_islightuserdata(L, -1)) {
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

/** }}} */

/** Window {{{ */

static void
window_push(lua_State *L, struct win *w)
{
  struct win *r;

  if (!w)
    {
      lua_pushnil(L);
    }
  else
    {
      r = (struct win *)lua_newuserdata(L, sizeof(struct win));
      *r = *w;
      luaL_getmetatable(L, "window");
      lua_setmetatable(L, -2);
    }
}

static int get_window(lua_State *L, void *v)
{
  window_push(L, *(struct win **)v);
  return 1;
}

static struct win*
check_window(lua_State *L, int index)
{
  struct win *win;
  luaL_checktype(L, index, LUA_TUSERDATA);
  win = (struct win*)luaL_checkudata(L, index, "window");
  if (!win)
    luaL_typerror(L, index, "window");
  return win;
}

static const luaL_reg window_methods[] = {
  {0, 0}
};

static const luaL_reg window_metamethods[] = {
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
  {0, 0}
};


/** }}} */

/** AclUser {{{ */

static void
user_push(lua_State *L, struct acluser *u)
{
  struct acluser *r;

  if (!u)
    {
      lua_pushnil(L);
    }
  else
    {
      r = (struct acluser *)lua_newuserdata(L, sizeof(struct acluser));
      *r = *u;
      luaL_getmetatable(L, "user");
      lua_setmetatable(L, -2);
    }
}

static int
get_user(lua_State *L, void *v)
{
  user_push(L, *(struct acluser **)v);
  return 1;
}

static const luaL_reg user_methods[] = {
  {0, 0}
};

static const luaL_reg user_metamethods[] = {
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

static void
canvas_push(lua_State *L, struct canvas *c)
{
  struct canvas *r;

  if (!c)
    {
      lua_pushnil(L);
    }
  else
    {
      r = (struct canvas *)lua_newuserdata(L, sizeof(struct canvas));
      *r = *c;
      luaL_getmetatable(L, "canvas");
      lua_setmetatable(L, -2);
    }
}

static int
get_canvas(lua_State *L, void *v)
{
  user_push(L, *(struct canvas **)v);
  return 1;
}

static const luaL_reg canvas_methods[] = {
  {0, 0}
};

static const luaL_reg canvas_metamethods[] = {
  {0, 0}
};

static const struct Xet_reg canvas_setters[] = {
  {0, 0}
};

static const struct Xet_reg canvas_getters[] = {
  {"next", get_canvas, offsetof(struct canvas, c_next)},
  {"xoff", get_int, offsetof(struct canvas, c_xoff)},
  {"yoff", get_int, offsetof(struct canvas, c_yoff)},
  {"xs", get_int, offsetof(struct canvas, c_xs)},
  {"ys", get_int, offsetof(struct canvas, c_ys)},
  {"xe", get_int, offsetof(struct canvas, c_xe)},
  {"ye", get_int, offsetof(struct canvas, c_ye)},
  {0, 0}
};

/** }}} */

/** Display {{{ */

static void
display_push(lua_State *L, struct display *d)
{
  struct display *r;

  if (!d)
    {
      lua_pushnil(L);
    }
  else
    {
      r = (struct display *)lua_newuserdata(L, sizeof(struct display));
      *r = *d;
      luaL_getmetatable(L, "display");
      lua_setmetatable(L, -2);
    }
}

static int
display_get_canvases(lua_State *L)
{
  struct display *d;
  struct canvas *iter;
  int count;

  luaL_checktype(L, 1, LUA_TUSERDATA);

  d = lua_touserdata(L, 1);

  for (iter = d->d_cvlist, count = 0; iter; iter = iter->c_next, count++)
    {
      canvas_push(L, iter);
    }
  return count;
}

static const luaL_reg display_methods[] = {
  {"get_canvases", display_get_canvases},
  {0, 0}
};

static const luaL_reg display_metamethods[] = {
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

/** Global functions {{{ */

static int
get_windows(lua_State *L)
{
  struct win *iter;
  int count;
  for (iter = windows, count = 0; iter; iter = iter->w_next, count++)
    {
      window_push(L, iter);
    }

  return count;
}

static int
get_displays(lua_State *L)
{
  struct display *iter;
  int count;
  for (iter = displays, count = 0; iter; iter = iter->d_next, count++)
    {
      display_push(L, iter);
    }

  return count;
}

static int
exec_command(lua_State *L)
{
  const char *command;
  unsigned int len;

  command = luaL_checklstring(L, 1, &len);
  if (command)
    RcLine(command, len);

  return 0;
}

static const luaL_reg screen_methods[] = {
  {"windows", get_windows},
  {"displays", get_displays},
  {"command", exec_command},
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

/* Ripped from http://lua-users.org/wiki/BindingWithMembersAndMethods */
static int
struct_register(lua_State *L, const char *name, const luaL_reg fn_methods[], const luaL_reg meta_methods[],
    const struct Xet_reg setters[], const struct Xet_reg getters[])
{
  int metatable, methods;

  /* create methods table, & add it to the table of globals */
  luaL_register(L, name, fn_methods);
  methods = lua_gettop(L);

  /* create metatable for your_t, & add it to the registry */
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

static lua_State *L;
void LuaInit(void)
{
  L = luaL_newstate();

  luaL_openlibs(L);

#define REGISTER(X) struct_register(L, #X , X ## _methods, X##_metamethods, X##_setters, X##_getters)

  REGISTER(screen);
  REGISTER(window);
  REGISTER(display);
  REGISTER(user);
  REGISTER(canvas);

  luaL_dofile(L, "/tmp/sc.lua");
}

void LuaForeWindowChanged(void)
{
  lua_getfield(L, LUA_GLOBALSINDEX, "fore_changed");
  display_push(L, display);
  window_push(L, display ? D_fore : fore);
  lua_call(L, 2, 0);
}

void LuaSource(const char *file)
{
  struct stat st;
  if (stat(file, &st) == -1)
    Msg(errno, "Error loading lua script file '%s'", file);
  else
    luaL_dofile(L, file);
}

