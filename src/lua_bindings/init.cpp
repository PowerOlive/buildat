// http://www.apache.org/licenses/LICENSE-2.0
// Copyright 2014 Perttu Ahola <celeron55@gmail.com>
#include "lua_bindings/init.h"
#include "core/log.h"
#include "interface/fs.h"
#define MODULE "lua_bindings"

namespace lua_bindings {

extern void init_misc(lua_State *L);
extern void init_cereal(lua_State *L);

void init(lua_State *L)
{
	init_misc(L);
	init_cereal(L);
}

} // namespace lua_bindingss
// vim: set noet ts=4 sw=4: