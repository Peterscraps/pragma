#include "stdafx_shared.h"
#include <pragma/engine.h>
#include <pragma/console/convars.h>
#include <pragma/console/s_convars.h>
#include <pragma/console/c_convars.h>
#include <luasystem.h>
#include <pragma/game/game.h>
#include <fsys/filesystem.h>
#include <mathutil/uvec.h>
#include <sharedutils/util_string.h>
#include <sharedutils/util_file.h>
#include <pragma/engine_version.h>
#include <pragma/util/profiling_stages.h>
#include <pragma/lua/lua_doc.hpp>
#include <map>

#define DLLSPEC_ISTEAMWORKS DLLNETWORK
#include <wv_steamworks.hpp>

extern DLLENGINE Engine *engine;

//////////////// LOGGING ////////////////
REGISTER_ENGINE_CONCOMMAND(log,[](NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string> &argv) {
	if(argv.empty()) return;
	engine->WriteToLog(argv[0]);
},ConVarFlags::None,"Adds the specified message to the engine log. Usage: log <msg>.");
REGISTER_ENGINE_CONCOMMAND(lua_help,[](NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string> &argv) {
	if(argv.empty())
		return;
	Lua::doc::print_documentation(argv.front());
},ConVarFlags::None,"Prints information about the specified function, library or enum (or the closest candiate). Usage: lua_help <function/library/enum>.");
REGISTER_ENGINE_CONCOMMAND(clear_cache,[](NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string> &argv) {
	if(argv.empty())
		return;
	engine->ClearCache();
},ConVarFlags::None,"Deletes all cache files.");
REGISTER_ENGINE_CONVAR(cache_version,"",ConVarFlags::Archive,"The engine version that the cache files are associated with. If this version doesn't match the current engine version, the cache will be cleared.");
REGISTER_ENGINE_CONVAR(log_enabled,"0",ConVarFlags::Archive,"0 = Log disabled; 1 = Log errors only; 2 = Log errors and warnings; 3 = Log all console output");
REGISTER_ENGINE_CONVAR(log_file,"log.txt",ConVarFlags::Archive,"The log-file the console output will be logged to.");
REGISTER_ENGINE_CONVAR(debug_profiling_enabled,"0",ConVarFlags::None,"Enables profiling timers.");
REGISTER_ENGINE_CONVAR(sh_mount_external_game_resources,"1",ConVarFlags::Archive,"If set to 1, the game will attempt to load missing resources from external games.");
REGISTER_ENGINE_CONVAR(sh_lua_remote_debugging,"0",ConVarFlags::Archive,"0 = Remote debugging is disabled; 1 = Remote debugging is enabled serverside; 2 = Remote debugging is enabled clientside.\nCannot be changed during an active game. Also requires the \"-luaext\" launch parameter.\nRemote debugging cannot be enabled clientside and serverside at the same time.");
REGISTER_ENGINE_CONVAR(lua_open_editor_on_error,"1",ConVarFlags::Archive,"1 = Whenever there's a Lua error, the engine will attempt to automatically open a Lua IDE and open the file and line which caused the error.");
REGISTER_ENGINE_CONVAR(steam_steamworks_enabled,"1",ConVarFlags::Archive,"Enables or disables steamworks.");
static void cvar_steam_steamworks_enabled(bool val)
{
	static std::weak_ptr<util::Library> wpSteamworks = {};
	static std::unique_ptr<ISteamworks> isteamworks = nullptr;
	auto *nwSv = engine->GetServerNetworkState();
	auto *nwCl = engine->GetClientState();
	if(val == true)
	{
		if(wpSteamworks.expired() == false && isteamworks != nullptr)
			return;
		const std::string libSteamworksPath {"steamworks/pr_steamworks"};
		std::shared_ptr<util::Library> libSteamworks = nullptr;
		if(nwSv != nullptr)
			libSteamworks = nwSv->InitializeLibrary(libSteamworksPath);
		if(nwCl != nullptr)
		{
			auto libCl = nwCl->InitializeLibrary(libSteamworksPath);
			if(libSteamworks == nullptr)
				libSteamworks = libCl;
		}
		if(libSteamworks != nullptr)
		{
			isteamworks = std::make_unique<ISteamworks>(*libSteamworks);
			if(isteamworks->initialize() == true)
			{
				isteamworks->subscribe_item(1684401267); // Automatically subscribe to pragma demo addon
				isteamworks->update_subscribed_items();
				if(nwSv != nullptr)
					nwSv->CallCallbacks<void,std::reference_wrapper<ISteamworks>>("OnSteamworksInitialized",*isteamworks);
				if(nwCl != nullptr)
					nwCl->CallCallbacks<void,std::reference_wrapper<ISteamworks>>("OnSteamworksInitialized",*isteamworks);
			}
			else
				isteamworks = nullptr;
		}
		else
			isteamworks = nullptr;
		wpSteamworks = libSteamworks;
		return;
	}
	if(wpSteamworks.expired() || isteamworks == nullptr)
		return;
	isteamworks->shutdown();
	wpSteamworks = {};
	isteamworks = nullptr;
	if(nwSv != nullptr)
		nwSv->CallCallbacks<void>("OnSteamworksShutdown");
	if(nwCl != nullptr)
		nwCl->CallCallbacks<void>("OnSteamworksShutdown");
}
REGISTER_ENGINE_CONVAR_CALLBACK(steam_steamworks_enabled,[](NetworkState*,ConVar*,bool prev,bool val) {
	cvar_steam_steamworks_enabled(val);
});

REGISTER_ENGINE_CONVAR_CALLBACK(sh_mount_external_game_resources,[](NetworkState*,ConVar*,bool prev,bool val) {
	engine->SetMountExternalGameResources(val);
});
REGISTER_ENGINE_CONCOMMAND(toggle,[](NetworkState *nw,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv) {
	if(argv.empty() == true)
		return;
	auto &cvName = argv.front();
	auto *cf = engine->GetConVar(cvName);
	if(cf == nullptr || cf->GetType() != ConType::Var)
		return;
	auto *cvar = static_cast<ConVar*>(cf);
	std::vector<std::string> args = {(cvar->GetBool() == true) ? "0" : "1"};
	engine->RunConsoleCommand(cvName,args);
},ConVarFlags::None,"Toggles the specified console variable between 0 and 1.");

REGISTER_ENGINE_CONVAR_CALLBACK(log_enabled,[](NetworkState*,ConVar*,int prev,int val) {
	//if(!engine->IsActiveState(state))
	//	return;
	if(prev == 0 && val != 0)
		engine->StartLogging();
	else if(prev != 0 && val == 0)
		engine->EndLogging();
});

REGISTER_ENGINE_CONVAR_CALLBACK(log_file,[](NetworkState *state,ConVar*,std::string prev,std::string val) {
	//if(!engine->IsActiveState(state))
	//	return;
	std::string lprev = prev;
	std::string lval = val;
	std::transform(lprev.begin(),lprev.end(),lprev.begin(),::tolower);
	std::transform(lval.begin(),lval.end(),lval.begin(),::tolower);
	if(lprev == lval)
		return;
	if(!state->GetConVarBool("log_enabled"))
		return;
	engine->StartLogging();
});

REGISTER_SHARED_CONVAR_CALLBACK(sv_gravity,[](NetworkState *state,ConVar*,std::string prev,std::string val) {
	if(!state->IsGameActive())
		return;
	Vector3 gravity = uvec::create(val);
	Game *game = state->GetGameState();
	game->SetGravity(gravity);
});

////////////////////////////////
////////////////////////////////

static void compile_lua_file(lua_State *l,Game &game,std::string f)
{
	StringToLower(f);
	std::string subPath = ufile::get_path_from_filename(f);
	std::string cur = "";
	std::string path = cur +f;
	path = FileManager::GetNormalizedPath(path);
	auto s = game.LoadLuaFile(path);
	if(s != Lua::StatusCode::Ok)
		return;
	if(path.length() > 3 && path.substr(path.length() -4) == ".lua")
		path = path.substr(0,path.length() -4);
	path += ".clua";
	auto r = Lua::compile_file(l,path);
	if(r == false)
		Con::cwar<<"WARNING: Unable to write file '"<<path.c_str()<<"'..."<<Con::endl;
	else
		Con::cout<<"Successfully compiled as '"<<path.c_str()<<"'."<<Con::endl;
}

static void CMD_lua_compile(NetworkState *state,pragma::BasePlayerComponent*,std::vector<std::string> &argv) {
	if(argv.empty() || !state->IsGameActive()) return;
	Game *game = state->GetGameState();
	auto *l = game->GetLuaState();
	std::string arg = argv[0];
	if(FileManager::IsDir("lua/" +arg))
	{
		std::function<void(const std::string&)> fCompileFiles = nullptr;
		fCompileFiles = [l,game,&fCompileFiles](const std::string &path) {
			std::vector<std::string> files {};
			std::vector<std::string> dirs {};
			FileManager::FindFiles(("lua/" +path +"/*").c_str(),&files,&dirs);
			for(auto &f : files)
			{
				std::string ext;
				if(ufile::get_extension(f,&ext) == false || ustring::compare(ext,"lua",false) == false)
					continue;
				compile_lua_file(l,*game,path +'/' +f);
			}
			for(auto &d : dirs)
				fCompileFiles(path +'/' +d);
		};
		fCompileFiles(arg);
		return;
	}
	compile_lua_file(l,*game,arg);

}
REGISTER_ENGINE_CONCOMMAND(lua_compile,CMD_lua_compile,ConVarFlags::None,"Opens the specified lua-file and outputs a precompiled file with the same name (And the extension '.clua').");

REGISTER_ENGINE_CONCOMMAND(toggleconsole,[](NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string>&) {
	if(engine->IsServerOnly())
		return;
	if(engine->IsConsoleOpen())
		engine->CloseConsole();
	else
		engine->OpenConsole();
},ConVarFlags::None,"Toggles the developer console.");

REGISTER_ENGINE_CONCOMMAND(echo,[](NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string> &argv) {
	if(argv.empty())
		return;
	Con::cout<<argv[0]<<Con::endl;
},ConVarFlags::None,"Prints something to the console. Usage: echo <message>");

static void CMD_exit(NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string>&) {engine->ShutDown();}
REGISTER_ENGINE_CONCOMMAND(exit,CMD_exit,ConVarFlags::None,"Exits the game.");
REGISTER_ENGINE_CONCOMMAND(quit,CMD_exit,ConVarFlags::None,"Exits the game.");

REGISTER_SHARED_CONCOMMAND(list,[](NetworkState *state,pragma::BasePlayerComponent*,std::vector<std::string>&) {
	auto &convars = state->GetConVars();
	std::vector<std::string> cvars(convars.size());
	size_t idx = 0;
	for(auto it=convars.begin();it!=convars.end();++it)
	{
		cvars[idx] = it->first;
		idx++;
	}
	std::sort(cvars.begin(),cvars.end());
	std::vector<std::string>::iterator it;
	for(it=cvars.begin();it!=cvars.end();it++)
	{
		if(*it != "credits")
			Con::cout<<*it<<Con::endl;
	}
},ConVarFlags::None,"Prints a list of all serverside console commands to the console.");

REGISTER_SHARED_CONCOMMAND(find,[](NetworkState *state,pragma::BasePlayerComponent*,std::vector<std::string> &argv) {
	if(argv.empty())
	{
		Con::cwar<<"WARNING: No argument given!"<<Con::endl;
		return;
	}
	auto similar = state->FindSimilarConVars(argv.front());
	if(similar.empty())
	{
		Con::cout<<"No potential candidates found!"<<Con::endl;
		return;
	}
	Con::cout<<"Found "<<similar.size()<<" potential candidates:"<<Con::endl;
	for(auto &name : similar)
		Con::cout<<"- "<<name<<Con::endl;
},ConVarFlags::None,"Finds similar console commands to whatever was given as argument.");

REGISTER_SHARED_CONCOMMAND(exec,[](NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string> &argv) {
	if(argv.empty())
		return;
	engine->ExecConfig(argv[0]);
},ConVarFlags::None,"Executes a config file. Usage exec <fileName>");

REGISTER_ENGINE_CONCOMMAND(listmaps,[](NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string>&) {
	std::vector<std::string> resFiles;
	FileManager::FindFiles("maps\\*.wld",&resFiles,nullptr);
	std::vector<std::string>::iterator it;
	for(it=resFiles.begin();it!=resFiles.end();it++)
		Con::cout<<*it<<Con::endl;
},ConVarFlags::None,"");

REGISTER_ENGINE_CONCOMMAND(clear,[](NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string>&) {
	std::system("cls");
},ConVarFlags::None,"Clears everything in the console.");

REGISTER_ENGINE_CONCOMMAND(help,[](NetworkState *state,pragma::BasePlayerComponent*,std::vector<std::string> &argv) {
	if(argv.empty())
	{
		Con::cout<<"Usage: help <cvarname>"<<Con::endl;
		return;
	}
	ConConf *cv = state->GetConVar(argv[0]);
	if(cv == NULL)
	{
		Con::cout<<"help: no cvar or command named "<<argv[0]<<Con::endl;
		return;
	}
	cv->Print(argv[0]);
},ConVarFlags::None," - Find help about a convar/concommand.");

REGISTER_ENGINE_CONCOMMAND(credits,[](NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string>&) {
	Con::cout<<"Florian Weischer aka Silverlan"<<Con::endl;
	Con::cout<<"Contact: "<<engine_info::get_author_mail_address()<<Con::endl;
	Con::cout<<"Website: "<<engine_info::get_website_url()<<Con::endl;
},ConVarFlags::None,"Prints a list of developers.");

REGISTER_ENGINE_CONCOMMAND(version,[](NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string>&) {
	Con::cout<<get_pretty_engine_version()<<Con::endl;
},ConVarFlags::None,"Prints the current engine version to the console.");

struct ProfilingDuration
{
	ProfilingDuration(ProfilingStage s,double d)
		: stage(s),duration(d)
	{}
	ProfilingStage stage;
	double duration;
};

REGISTER_ENGINE_CONCOMMAND(debug_profiling_print,[](NetworkState*,pragma::BasePlayerComponent*,std::vector<std::string>&) {
	auto &stages = engine->GetProfilingStages();
	std::vector<ProfilingDuration> durations;
	durations.reserve(stages.size());
	for(auto &it : stages)
		durations.push_back(ProfilingDuration{static_cast<ProfilingStage>(it.first),static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(it.second->duration).count()) /1'000'000.0});
	std::sort(durations.begin(),durations.end(),[](const ProfilingDuration &a,const ProfilingDuration &b) {
		return (a.duration > b.duration) ? true : false;
	});
	for(auto &d : durations)
		Con::cout<<profiling_stage_to_string(d.stage)<<": "<<d.duration<<"ms"<<Con::endl;
},ConVarFlags::None,"Prints the last profiled times.");

//////////////// SERVER ////////////////

REGISTER_SHARED_CONVAR(rcon_password,"",ConVarFlags::None,"Specifies a password which can be used to run console commands remotely on a server. If no password is specified, this feature is disabled.");

#ifdef PHYS_ENGINE_PHYSX
#ifdef _DEBUG
REGISTER_CONCOMMAND(pvd_connect,[](NetworkState *state,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv) {
	engine->OpenPVDConnection();
},"Connects with the PVD, if it is started. Only available in debug configuration!");

REGISTER_CONCOMMAND(pvd_disconnect,[](NetworkState *state,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv) {
	engine->ClosePVDConnection();
},"Disconnects from the PVD, if currently connected. Only available in debug configuration!");
#endif
#endif
