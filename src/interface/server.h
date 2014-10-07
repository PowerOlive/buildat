// http://www.apache.org/licenses/LICENSE-2.0
// Copyright 2014 Perttu Ahola <celeron55@gmail.com>
#pragma once
#include "core/types.h"
#include "interface/event.h"
#include <functional>

namespace Urho3D
{
	class Scene;
	class StringHash;
}

namespace interface
{
	namespace magic = Urho3D;

	struct ModuleInfo;
	struct Module;

	struct TickEvent: public interface::Event::Private {
		float dtime;
		TickEvent(float dtime): dtime(dtime){}
	};

	struct SocketEvent: public interface::Event::Private {
		int fd;
		SocketEvent(int fd): fd(fd){}
	};

	struct ModuleModifiedEvent: public interface::Event::Private {
		ss_ name;
		ss_ path;
		ModuleModifiedEvent(const ss_ &name, const ss_ &path):
			name(name), path(path){}
	};

	struct ModuleLoadedEvent: public interface::Event::Private {
		ss_ name;
		ModuleLoadedEvent(const ss_ &name): name(name){}
	};

	struct ModuleUnloadedEvent: public interface::Event::Private {
		ss_ name;
		ModuleUnloadedEvent(const ss_ &name): name(name){}
	};

	struct Server
	{
		virtual ~Server(){}

		virtual void shutdown(int exit_status = 0, const ss_ &reason = "") = 0;

		virtual bool load_module(const interface::ModuleInfo &info) = 0;
		virtual void unload_module(const ss_ &module_name) = 0;
		virtual void reload_module(const interface::ModuleInfo &info) = 0;
		virtual void reload_module(const ss_ &module_name) = 0;
		virtual ss_ get_modules_path() = 0;
		virtual ss_ get_builtin_modules_path() = 0;
		virtual ss_ get_module_path(const ss_ &module_name) = 0;
		virtual bool has_module(const ss_ &module_name) = 0;
		virtual sv_<ss_> get_loaded_modules() = 0;
		virtual bool access_module(const ss_ &module_name,
				std::function<void(interface::Module*)> cb) = 0;

		virtual void sub_event(struct Module *module, const Event::Type &type) = 0;
		virtual void emit_event(Event event) = 0;
		template<typename TypeT, typename PrivateT>
		void emit_event(const TypeT &type, PrivateT *p){
			emit_event(std::move(Event(type, up_<Event::Private>(p))));
		}

		virtual void access_scene(std::function<void(magic::Scene*)> cb) = 0;
		virtual void sub_magic_event(struct interface::Module *module,
				const magic::StringHash &event_type,
				const Event::Type &buildat_event_type) = 0;

		virtual void add_socket_event(int fd, const Event::Type &event_type) = 0;
		virtual void remove_socket_event(int fd) = 0;

		virtual void tmp_store_data(const ss_ &name, const ss_ &data) = 0;
		virtual ss_ tmp_restore_data(const ss_ &name) = 0;

		// Add resource file path (to make a mirror of the client)
		virtual void add_file_path(const ss_ &name, const ss_ &path) = 0;
	};
}
// vim: set noet ts=4 sw=4:
