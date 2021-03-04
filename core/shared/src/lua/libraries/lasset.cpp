/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_shared.h"
#include "pragma/lua/libraries/lasset.hpp"
#include "pragma/model/modelmanager.h"
#include "pragma/asset/util_asset.hpp"
#include <luainterface.hpp>

extern DLLNETWORK Engine *engine;


void Lua::asset::register_library(Lua::Interface &lua,bool extended)
{
	auto modAsset = luabind::module_(lua.GetState(),"asset");
	modAsset[
		luabind::def("clear_unused_models",static_cast<uint32_t(*)(lua_State*)>([](lua_State *l) -> uint32_t {
			auto *nw = engine->GetNetworkState(l);
			return nw->GetModelManager().ClearUnused();
		})),
		luabind::def("clear_flagged_models",static_cast<uint32_t(*)(lua_State*)>([](lua_State *l) -> uint32_t {
			auto *nw = engine->GetNetworkState(l);
			return nw->GetModelManager().ClearFlagged();
		})),
		luabind::def("flag_model_for_cache_removal",static_cast<void(*)(lua_State*)>([](lua_State *l) {
			auto *nw = engine->GetNetworkState(l);
			auto &mdl = Lua::Check<Model>(l,1);
			nw->GetModelManager().FlagForRemoval(mdl);
		})),
		luabind::def("clear_unused_materials",static_cast<uint32_t(*)(lua_State*)>([](lua_State *l) -> uint32_t {
			auto *nw = engine->GetNetworkState(l);
			return nw->GetMaterialManager().ClearUnused();
		})),
		luabind::def("lock_asset_watchers",&Lua::asset::lock_asset_watchers),
		luabind::def("unlock_asset_watchers",&Lua::asset::unlock_asset_watchers),
		luabind::def("get_supported_import_file_extensions",&Lua::asset::get_supported_import_file_extensions),
		luabind::def("get_supported_export_file_extensions",&Lua::asset::get_supported_export_file_extensions),
		luabind::def("matches",&pragma::asset::matches),
		luabind::def("get_normalized_path",&pragma::asset::get_normalized_path)
	];

	if(extended)
	{
		modAsset[
			luabind::def("clear_unused_models",Lua::asset::exists),
			luabind::def("clear_unused_models",Lua::asset::find_file),
			luabind::def("is_loaded",Lua::asset::is_loaded)
		];
	}

	Lua::RegisterLibraryEnums(lua.GetState(),"asset",{
		{"TYPE_MODEL",umath::to_integral(pragma::asset::Type::Model)},
		{"TYPE_MAP",umath::to_integral(pragma::asset::Type::Map)},
		{"TYPE_MATERIAL",umath::to_integral(pragma::asset::Type::Material)},
		{"TYPE_TEXTURE",umath::to_integral(pragma::asset::Type::Texture)},
		{"TYPE_AUDIO",umath::to_integral(pragma::asset::Type::Sound)},
		{"TYPE_PARTICLE_SYSTEM",umath::to_integral(pragma::asset::Type::ParticleSystem)}
	});
	static_assert(umath::to_integral(pragma::asset::Type::Count) == 6,"Update this list!");

	Lua::RegisterLibraryValue<std::string>(lua.GetState(),"asset","MODEL_FILE_EXTENSION","wmd");
	Lua::RegisterLibraryValue<std::string>(lua.GetState(),"asset","MATERIAL_FILE_EXTENSION","wmi");
	Lua::RegisterLibraryValue<std::string>(lua.GetState(),"asset","PARTICLE_SYSTEM_FILE_EXTENSION","wpt");
}
bool Lua::asset::exists(lua_State *l,const std::string &name,pragma::asset::Type type)
{
	auto *nw = engine->GetNetworkState(l);
	return pragma::asset::exists(*nw,name,type);
}
luabind::object Lua::asset::find_file(lua_State *l,const std::string &name,pragma::asset::Type type)
{
	auto *nw = engine->GetNetworkState(l);
	auto path = pragma::asset::find_file(*nw,name,type);
	if(path.has_value() == false)
		return {};
	return luabind::object{l,*path};
}
bool Lua::asset::is_loaded(lua_State *l,const std::string &name,pragma::asset::Type type)
{
	auto *nw = engine->GetNetworkState(l);
	return pragma::asset::is_loaded(*nw,name,type);
}
void Lua::asset::lock_asset_watchers(lua_State *l)
{
	engine->LockResourceWatchers();
}
void Lua::asset::unlock_asset_watchers(lua_State *l)
{
	engine->UnlockResourceWatchers();
}
luabind::object Lua::asset::get_supported_import_file_extensions(lua_State *l,pragma::asset::Type type)
{
	auto t = luabind::newtable(l);
	auto &assetManager = engine->GetAssetManager();
	auto n = assetManager.GetImporterCount(type);
	int32_t idx = 1;
	for(auto i=decltype(n){0u};i<n;++i)
	{
		for(auto &ext : assetManager.GetImporterInfo(type,i)->fileExtensions)
			t[idx++] = ext;
	}
	if(type == pragma::asset::Type::Model)
	{
		// These are implemented using the old importer system, so they're not included in the import information
		// retrieved above. We'll add them to the list manually for now.
		// TODO: Move these to the new importer system and remove these entries!
		t[idx++] = "mdl";
		t[idx++] = "vmdl_c";
		t[idx++] = "nif";
	}
	return t;
}
luabind::object Lua::asset::get_supported_export_file_extensions(lua_State *l,pragma::asset::Type type)
{
	auto t = luabind::newtable(l);
	auto &assetManager = engine->GetAssetManager();
	auto n = assetManager.GetExporterCount(type);
	int32_t idx = 1;
	for(auto i=decltype(n){0u};i<n;++i)
	{
		for(auto &ext : assetManager.GetExporterInfo(type,i)->fileExtensions)
			t[idx++] = ext;
	}
	return t;
}
