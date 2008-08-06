/* Copyright (c) 2008 Sadrul Habib Chowdhury (sadrul@users.sf.net)
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

#include "config.h"
#include "screen.h"

struct scripts
{
  struct scripts *s_next;
  struct ScriptFuncs *fns;
};

struct scripts *scripts;

static void
AddScript(struct ScriptFuncs *sf)
{
  struct scripts *ns = (struct scripts *)calloc(1, sizeof(*ns));
  if (!ns)
    return;
  ns->fns = sf;
  ns->s_next = scripts;
  scripts = ns;
}

#define ALL_SCRIPTS(fn, params, stop) do { \
  struct scripts *iter; \
  for (iter = scripts; iter; iter = iter->s_next) \
    { \
      if (iter->fns->fn && (ret = (iter->fns->fn params)) && stop) \
	break; \
    } \
} while (0)

void ScriptInit(void)
{
  int ret;
  ALL_SCRIPTS(sf_Init, (), 0);
}

void ScriptFinit(void)
{
  int ret;
  ALL_SCRIPTS(sf_Finit, (), 0);
}

void ScriptForeWindowChanged(void)
{
  int ret;
  ALL_SCRIPTS(sf_ForeWindowChanged, (), 0);
}

void ScriptSource(const char *path)
{
  int ret;
  /* If one script loader accepts the file, we don't send it to any other loader */
  ALL_SCRIPTS(sf_Source, (path), 1);
}

int ScriptProcessCaption(const char *str, struct win *win, int len)
{
  int ret = 0;
  ALL_SCRIPTS(sf_ProcessCaption, (str, win, len), 1);
  return ret;
}

#define HAVE_LUA 1   /* XXX: Remove */
#if HAVE_LUA
extern struct ScriptFuncs LuaFuncs;
#endif

void LoadScripts(void)
{
  /* XXX: We could load the script loaders dynamically */
#if HAVE_LUA
  AddScript(&LuaFuncs);
#endif
}

