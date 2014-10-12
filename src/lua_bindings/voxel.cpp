// http://www.apache.org/licenses/LICENSE-2.0
// Copyright 2014 Perttu Ahola <celeron55@gmail.com>
#include "lua_bindings/util.h"
#include "core/log.h"
#include "client/app.h"
#include "interface/mesh.h"
#include <tolua++.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#include <Scene.h>
#include <StaticModel.h>
#include <Model.h>
#include <CustomGeometry.h>
#pragma GCC diagnostic pop
#define MODULE "lua_bindings"

namespace magic = Urho3D;

// Just do this; Urho3D's stuff doesn't really clash with anything in buildat
using namespace Urho3D;

namespace lua_bindings {

#define GET_TOLUA_STUFF(result_name, index, type)\
	if(!tolua_isusertype(L, index, #type, 0, &tolua_err)){\
		tolua_error(L, __PRETTY_FUNCTION__, &tolua_err);\
		return 0;\
	}\
	type *result_name = (type*)tolua_tousertype(L, index, 0);

// NOTE: This API is designed this way because otherwise ownership management of
//       objects sucks
// set_simple_voxel_model(node, w, h, d, buffer: VectorBuffer)
static int l_set_simple_voxel_model(lua_State *L)
{
	tolua_Error tolua_err;

	GET_TOLUA_STUFF(node, 1, Node);
	int w = lua_tointeger(L, 2);
	int h = lua_tointeger(L, 3);
	int d = lua_tointeger(L, 4);
	GET_TOLUA_STUFF(buf, 5, const VectorBuffer);

	log_d(MODULE, "set_simple_voxel_model(): buf=%p", buf);
	log_d(MODULE, "set_simple_voxel_model(): node=%p", node);

	ss_ data((const char*)&buf->GetBuffer()[0], buf->GetBuffer().Size());

	if((int)data.size() != w * h * d){
		log_e(MODULE, "set_simple_voxel_model(): Data size does not match "
				"with dimensions (%zu vs. %i)", data.size(), w*h*d);
		return 0;
	}

	lua_getfield(L, LUA_REGISTRYINDEX, "__buildat_app");
	app::App *buildat_app = (app::App*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	Context *context = buildat_app->get_scene()->GetContext();

	SharedPtr<Model> fromScratchModel(
			interface::create_simple_voxel_model(context, w, h, d, data));

	StaticModel *object = node->CreateComponent<StaticModel>();
	object->SetModel(fromScratchModel);

	return 0;
}

// set_8bit_voxel_geometry(node, w, h, d, buffer: VectorBuffer)
static int l_set_8bit_voxel_geometry(lua_State *L)
{
	tolua_Error tolua_err;

	GET_TOLUA_STUFF(node, 1, Node);
	int w = lua_tointeger(L, 2);
	int h = lua_tointeger(L, 3);
	int d = lua_tointeger(L, 4);
	GET_TOLUA_STUFF(buf, 5, const VectorBuffer);
	log_d(MODULE, "set_simple_voxel_model(): buf=%p", buf);
	log_d(MODULE, "set_8bit_voxel_geometry(): node=%p", node);

	ss_ data((const char*)&buf->GetBuffer()[0], buf->GetBuffer().Size());

	if((int)data.size() != w * h * d){
		log_e(MODULE, "set_8bit_voxel_geometry(): Data size does not match "
				"with dimensions (%zu vs. %i)", data.size(), w*h*d);
		return 0;
	}

	lua_getfield(L, LUA_REGISTRYINDEX, "__buildat_app");
	app::App *buildat_app = (app::App*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	Context *context = buildat_app->get_scene()->GetContext();
	auto *voxel_reg = buildat_app->get_voxel_registry();

	CustomGeometry *cg = node->CreateComponent<CustomGeometry>();

	interface::set_8bit_voxel_geometry(cg, context, w, h, d, data, voxel_reg);

	// Maybe appropriate
	cg->SetOccluder(true);

	// TODO: Don't do this here; allow the caller to do this
	cg->SetCastShadows(true);

	return 0;
}

void init_voxel(lua_State *L)
{
#define DEF_BUILDAT_FUNC(name){ \
		lua_pushcfunction(L, l_##name); \
		lua_setglobal(L, "__buildat_" #name); \
}
	DEF_BUILDAT_FUNC(set_simple_voxel_model);
	DEF_BUILDAT_FUNC(set_8bit_voxel_geometry);
}

}	// namespace lua_bindingss

// vim: set noet ts=4 sw=4:
