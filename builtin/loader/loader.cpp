// http://www.apache.org/licenses/LICENSE-2.0
// Copyright 2014 Perttu Ahola <celeron55@gmail.com>
#include "loader/api.h"
#include "core/log.h"
#include "core/json.h"
#include "interface/module.h"
#include "interface/server.h"
#include "interface/fs.h"
#include "interface/event.h"
#include "interface/module_info.h"
#include <fstream>

using interface::Event;

namespace loader {

static interface::ModuleDependency load_module_dependency(const json::Value &v)
{
	interface::ModuleDependency r;
	r.module = v.get("module").as_string();
	r.optional = v.get("optional").as_boolean();
	return r;
}

static interface::ModuleMeta load_module_meta(const json::Value &v)
{
	interface::ModuleMeta r;
	r.cxxflags = v.get("cxxflags").as_string();
	r.ldflags = v.get("ldflags").as_string();
	const json::Value &deps_v = v.get("dependencies");
	for(unsigned int i = 0; i < deps_v.size(); i++){
		const json::Value &dep_v = deps_v.at(i);
		interface::ModuleDependency dep = load_module_dependency(deps_v.at(i));
		r.dependencies.push_back(dep);
	}
	const json::Value &rev_deps_v = v.get("reverse_dependencies");
	for(unsigned int i = 0; i < rev_deps_v.size(); i++){
		interface::ModuleDependency dep = load_module_dependency(rev_deps_v.at(i));
		r.reverse_dependencies.push_back(dep);
	}
	return r;
}

struct ResolveState
{
	const char *MODULE = "loader";

	Interface *m_loader;

	bool m_failed = false;
	ss_ m_error_message = "No errors";

	// The goal is to get this set of modules into m_module_load_order
	set_<ss_> m_required_modules;
	// This will contain the order in which modules will be loaded
	sv_<ss_> m_module_load_order;
	// The previous vector as a set
	set_<ss_> m_promised_modules;
	// Reverse dependencies to each module (listed in a forward way)
	sm_<ss_, sv_<interface::ModuleDependency>> m_reverse_dependencies;

	ResolveState(Interface *loader):
		m_loader(loader)
	{}

	bool set_error(const ss_ &message)
	{
		m_failed = true;
		m_error_message = message;
		return false;
	}

	// On error sets m_failed, m_error_message and returns false
	bool require_modules(const set_<ss_> &modules)
	{
		for(const ss_ &name : modules){
			if(!require_module(name, false)){
				m_error_message = ss_()+"Error adding required module: "+
						m_error_message;
				return false;
			}
		}
		return true;
	}

	// On error sets m_failed, m_error_message and returns false
	bool require_module(const ss_ &name, bool optional,
			const ss_ &log_extra_info = "")
	{
		if(m_required_modules.count(name))
			return true;
		log_d(MODULE, "require_module(): New requirement: \"%s\"", cs(name));

		interface::ModuleInfo *info = m_loader->get_module_info(name);
		if(info == nullptr){
			if(optional){
				log_d(MODULE, "require_module(): "
						"Optional module info not found: \"%s\"", cs(name));
				return true;
			}
			return set_error(ss_()+"Couldn't get module info for \""+name+"\""+
					(log_extra_info == "" ? "" : ss_()+" ("+log_extra_info+")"));
		}

		m_required_modules.insert(name);

		// Require dependencies
		for(auto &dep : info->meta.dependencies){
			log_d(MODULE, "require_module(): [\"%s\" depends on \"%s\"]",
					cs(name), cs(dep.module));
			if(!require_module(dep.module, dep.optional,
					ss_()+"required by \""+name+"\""))
				return false;
		}

		// Handle reverse dependencies
		for(auto &dep : info->meta.reverse_dependencies){
			log_d(MODULE, "require_module(): [\"%s\" depends on \"%s\"]"
					" (defined in reverse)",
					cs(dep.module), cs(name));

			// Warn about deficiency in implementation
			if(m_promised_modules.count(dep.module)){
				log_w(MODULE, "%s: Reverse dependency %s ignored (already "
						"marked to be loaded)", cs(name), cs(dep.module));
				continue; // Adding the dependency would have no effect
			}

			// Store dependency information
			interface::ModuleDependency forward_dep;
			forward_dep = dep; // Base dependency on reverted one
			forward_dep.module = name; // The other module depends now on this
			// dep.module is the other module which should depeend on this one
			m_reverse_dependencies[dep.module].push_back(forward_dep);

			if(!require_module(dep.module, dep.optional,
					ss_()+"required by \""+name+"\" as a reverse dependency"))
				return false;
		}
		return true;
	}

	// On error sets m_failed, m_error_message and returns false
	bool load_module(const ss_ &name)
	{
		if(m_promised_modules.count(name)){
			throw Exception(ss_()+"Logic error in ResolveState: "
					"load_module(\""+name+"\"): already promised");
		}
		log_d(MODULE, "Marking \"%s\" to be loaded", cs(name));
		m_module_load_order.push_back(name);
		m_promised_modules.insert(name);
	}

	// Return value: false if nothing can be done anymore
	bool step(bool follow_optdepends)
	{
		log_d(MODULE, "step(): follow_optdepends=%s",
				follow_optdepends ? "true" : "false");
		// Pick a required module that isn't already loaded and which has all
		// dependencies promised
		interface::ModuleInfo *info_to_load = nullptr;
		bool all_promised = true;
		for(const ss_ &name : m_required_modules){
			if(m_promised_modules.count(name))
				continue;
			log_d(MODULE, "step(): Checking \"%s\"", cs(name));
			all_promised = false;
			interface::ModuleInfo *info = m_loader->get_module_info(name);
			if(!info)
				return set_error(ss_()+"Couldn't get module info for \""+name+"\"");
			bool deps_promised = true;
			for(auto &dep : info->meta.dependencies){
				if(m_promised_modules.count(dep.module) == 0 &&
						(!dep.optional || !follow_optdepends)){
					log_d(MODULE, "step(): * [\"%s\" depends on \"%s\"]: "
							"Dependency not promised yet",
							cs(name), cs(dep.module));
					deps_promised = false;
					break;
				}
			}
			for(auto &dep : m_reverse_dependencies[name]){
				if(m_promised_modules.count(dep.module) == 0 &&
						(!dep.optional || !follow_optdepends)){
					log_d(MODULE, "step(): * [\"%s\" depends on \"%s\"]: "
							"Dependency not promised yet (defined in reverse)",
							cs(name), cs(dep.module));
					deps_promised = false;
					break;
				}
			}
			if(!deps_promised){
				log_d(MODULE, "step(): -> Dependencies not promised yet for "
						"\"%s\"", cs(name));
				continue;
			}
			// Found a suitable module
			log_d(MODULE, "step(): -> Can be marked to be loaded: \"%s\"",
					cs(name));
			info_to_load = info;
			break;
		}
		if(info_to_load){
			// Mark it to be loaded
			load_module(info_to_load->name);
			return true;
		}
		if(all_promised){
			log_d(MODULE, "step(): Dependencies fulfilled");
			return false;
		}
		log_v(MODULE, "step(): Could not satisfy dependencies");
		return false;
	}

	// On error sets m_failed, m_error_message and returns false
	bool step_through()
	{
		log_d(MODULE, "step_through()");

		while(step(true));
		if(m_failed) return false;

		while(step(false));
		if(m_failed) return false;

		for(const ss_ &name : m_required_modules){
			if(m_promised_modules.count(name))
				continue;
			interface::ModuleInfo *info = m_loader->get_module_info(name);
			if(!info)
				return set_error(ss_()+"Couldn't get module info for \""+name+"\"");
			set_<ss_> missing_deps;
			set_<ss_> missing_rev_deps;
			for(auto &dep : info->meta.dependencies){
				if(m_promised_modules.count(dep.module) == 0)
					missing_deps.insert(dep.module);
			}
			for(auto &dep : m_reverse_dependencies[name]){
				if(m_promised_modules.count(dep.module) == 0)
					missing_rev_deps.insert(dep.module);
			}
			if(!missing_deps.empty())
				log_w(MODULE, "[\"%s\" depends on %s]: Missing dependencies",
						cs(name), cs(dump(missing_deps)));
			if(!missing_rev_deps.empty())
				log_w(MODULE, "[\"%s\" depends on %s]: Missing dependencies"
						" (defined in reverse)",
						cs(name), cs(dump(missing_rev_deps)));
			if(!missing_deps.empty() || !missing_rev_deps.empty())
				set_error("Missing dependencies");
		}

		return !m_failed; // Make sure to return any leftover failure as false
	}
};

struct Module: public interface::Module, public loader::Interface
{
	interface::Server *m_server;
	bool m_activated = false;
	sv_<ss_> m_module_load_paths; // In order of preference

	Module(interface::Server *server):
		interface::Module("loader"),
		m_server(server)
	{
		log_v(MODULE, "loader construct");

		m_module_load_paths.push_back(m_server->get_modules_path());
		m_module_load_paths.push_back(m_server->get_builtin_modules_path());
	}

	~Module()
	{
		log_v(MODULE, "loader destruct");
	}

	void init()
	{
		log_v(MODULE, "loader init");
		m_server->sub_event(this, Event::t("core:module_modified"));
	}

	void event(const Event::Type &type, const Event::Private *p)
	{
		EVENT_TYPEN("core:module_modified", on_module_modified,
				interface::ModuleModifiedEvent)
	}

	sm_<ss_, interface::ModuleInfo> m_module_info;

	interface::ModuleInfo* get_module_info(const ss_ &name)
	{
		auto it = m_module_info.find(name);
		if(it != m_module_info.end()){
			return &it->second;
		}
		// Load module info
		for(const ss_ &base_path : m_module_load_paths){
			ss_ module_path = base_path+"/"+name;
			ss_ meta_path = module_path+"/meta.txt";
			std::ifstream f(meta_path, std::ios::binary);
			if(!f.good()){
				log_t(MODULE, "%s: Could not open", cs(meta_path));
				continue;
			} else {
				log_t(MODULE, "%s: Opened", cs(meta_path));
			}
			std::string meta_content((std::istreambuf_iterator<char>(f)),
					std::istreambuf_iterator<char>());
			json::json_error_t json_error;
			json::Value meta_v = json::load_string(meta_content.c_str(), &json_error);
			if(meta_v.is_undefined()){
				log_e(MODULE, "Invalid JSON: %s:%i: %s", cs(meta_path),
						json_error.line, json_error.text);
				ss_ reason = ss_()+"loader: Error in module "+name+" meta.txt:"+
						itos(json_error.line)+": "+json_error.text;
				m_server->shutdown(1, reason);
				return nullptr;
			}
			// meta.txt is valid; read information
			interface::ModuleInfo &info = m_module_info[name];
			info.name = name;
			info.path = module_path;
			info.meta = load_module_meta(meta_v);
			return &info;
		}
		return nullptr;
	}

	void load_modules()
	{
		log_v(MODULE, "loader::load_modules()");
		auto *fs = interface::getGlobalFilesystem();
		ss_ builtin = m_server->get_builtin_modules_path();
		ss_ current = m_server->get_modules_path();

		// Get a list of required modules; that is, everything in the main
		// module path
		set_<ss_> required_modules;
		auto list = fs->list_directory(current);
		for(const interface::Filesystem::Node &n : list){
			if(n.name == "__loader")
				continue;
			required_modules.insert(n.name);
		}

		ResolveState resolve(this);

		if(!resolve.require_modules(required_modules)){
			log_w(MODULE, "Failed to resolve dependencies: %s",
					cs(resolve.m_error_message));
			m_server->shutdown(1, ss_()+"loader: "+resolve.m_error_message);
			return;
		}

		if(!resolve.step_through()){
			log_w(MODULE, "Failed to resolve dependencies: %s",
					cs(resolve.m_error_message));
			m_server->shutdown(1, ss_()+"loader: "+resolve.m_error_message);
			return;
		}

		log_i(MODULE, "Module load order: %s", cs(dump(resolve.m_module_load_order)));

		for(const ss_ &name : resolve.m_module_load_order){
			interface::ModuleInfo *info = get_module_info(name);
			if(!info)
				throw Exception(ss_()+"Couldn't get module info for \""+name+"\"");
			if(!m_server->load_module(*info)){
				m_server->shutdown(1, ss_()+"loader: Error loading module "+name);
				return;
			}
		}
	}

	void on_module_modified(const interface::ModuleModifiedEvent &event)
	{
		if(!m_activated)
			return;
		log_v(MODULE, "loader::on_module_modified(): %s", cs(event.name));
		if(event.name == "loader")
			return;
		m_server->reload_module(event.name);
	}

	// Interface

	void activate()
	{
		if(m_activated)
			return;
		m_activated = true;
		load_modules();
	}

	void* get_interface()
	{
		return dynamic_cast<Interface*>(this);
	}
};

extern "C" {
	EXPORT void* createModule_loader(interface::Server *server){
		return (void*)(new Module(server));
	}
}
}
// vim: set noet ts=4 sw=4:
