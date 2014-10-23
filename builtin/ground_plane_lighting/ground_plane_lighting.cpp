// http://www.apache.org/licenses/LICENSE-2.0
// Copyright 2014 Perttu Ahola <celeron55@gmail.com>
#include "ground_plane_lighting/api.h"
// TODO: Clean up
#include "network/api.h"
#include "client_file/api.h"
#include "voxelworld/api.h"
#include "replicate/api.h"
#include "network/api.h"
#include "core/log.h"
#include "interface/module.h"
#include "interface/server.h"
#include "interface/event.h"
#include "interface/mesh.h"
#include "interface/voxel.h"
#include "interface/block.h"
#include "interface/voxel_volume.h"
#include "interface/polyvox_numeric.h"
#include "interface/polyvox_cereal.h"
#include "interface/os.h"
#include <PolyVoxCore/RawVolume.h>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/tuple.hpp>
#include <Node.h>
#include <deque>
#include <algorithm>
#include <climits>

using interface::Event;
namespace magic = Urho3D;
namespace pv = PolyVox;
using namespace Urho3D;
using interface::VoxelInstance;
using interface::container_coord;
using interface::container_coord16;

namespace std {

// TODO: Move to a header (core/types_polyvox.h or something)
template<> struct hash<pv::Vector<2u, int16_t>>{
	std::size_t operator()(const pv::Vector<2u, int16_t> &v) const {
		return ((std::hash<int16_t>() (v.getX()) << 0) ^
				   (std::hash<int16_t>() (v.getY()) << 1));
	}
};

}

// TODO: Move to a header (core/types_polyvox.h or something)
#define PV3I_FORMAT "(%i, %i, %i)"
#define PV3I_PARAMS(p) p.getX(), p.getY(), p.getZ()

namespace ground_plane_lighting {

// Y-seethrough data of a sector (size defined by voxelworld)
struct YSTSector
{
	static constexpr const char *MODULE = "YSTSector";
	pv::Vector<2, int16_t> sector_p; // In sectors
	pv::Vector<2, int16_t> sector_size; // In voxels
	sp_<pv::RawVolume<int32_t>> volume; // Voxel columns (region in global coords)

	YSTSector():
		sector_size(0, 0) // This is used to detect uninitialized instance
	{}
	YSTSector(const pv::Vector<2, int16_t> &sector_p,
			const pv::Vector<2, int16_t> &sector_size):
		sector_p(sector_p),
		sector_size(sector_size),
		volume(new pv::RawVolume<int32_t>(pv::Region(
				sector_p.getX() * sector_size.getX(),
				0,
				sector_p.getY() * sector_size.getY(),
				sector_p.getX() * sector_size.getX() + sector_size.getX() - 1,
				0,
				sector_p.getY() * sector_size.getY() + sector_size.getY() - 1
		)))
	{
		pv::Region region = volume->getEnclosingRegion();
		auto lc = region.getLowerCorner();
		auto uc = region.getUpperCorner();
		/*log_v(MODULE, "YSTSector volume lc=" PV3I_FORMAT ", uc=" PV3I_FORMAT
		        ", size=%zu", PV3I_PARAMS(lc), PV3I_PARAMS(uc),
		        volume->m_dataSize);*/
		for(int z = lc.getZ(); z <= uc.getZ(); z++){
			for(int x = lc.getX(); x <= uc.getX(); x++){
				volume->setVoxelAt(x, 0, z, INT_MIN);
			}
		}
	}
};

// Global Y-seethrough map
struct GlobalYSTMap
{
	pv::Vector<2, int16_t> m_sector_size;
	sm_<pv::Vector<2, int16_t>, YSTSector> m_sectors;
	// Cache of last used sectors (add to end, remove from beginning)
	std::deque<YSTSector*> m_last_used_sectors;
	// Set of sectors that have been modified
	// (as a sorted array in descending order)
	sv_<YSTSector*> m_dirty_sectors;

	GlobalYSTMap(const pv::Vector<2, int16_t> &sector_size):
		m_sector_size(sector_size)
	{}

	YSTSector* get_sector(pv::Vector<2, int16_t> &sector_p, bool create)
	{
		// Check cache
		for(YSTSector *sector : m_last_used_sectors){
			if(sector->sector_p == sector_p)
				return sector;
		}
		// Not in cache
		YSTSector *sector = nullptr;
		auto it = m_sectors.find(sector_p);
		if(it == m_sectors.end()){
			if(!create)
				return nullptr;
			sector = &m_sectors[sector_p];
			*sector = YSTSector(sector_p, m_sector_size);
		} else {
			sector = &it->second;
		}
		// Add to cache and return
		m_last_used_sectors.push_back(sector);
		if(m_last_used_sectors.size() > 2) // 2 is maybe optimal-ish
			m_last_used_sectors.pop_front();
		return sector;
	}

	pv::Vector<2, int16_t> get_sector_p(int32_t x, int32_t z)
	{
		if(m_sector_size.getX() == 0)
			throw Exception("GlobalYSTMap not initialized");
		return container_coord16(pv::Vector<2, int32_t>(x, z), m_sector_size);
	}

	void set_yst(int32_t x, int32_t z, int32_t yst)
	{
		auto sector_p = get_sector_p(x, z);
		YSTSector *sector = get_sector(sector_p, true);
		sector->volume->setVoxelAt(x, 0, z, yst);

		// Set sector dirty flag
		auto it = std::lower_bound(m_dirty_sectors.begin(),
					m_dirty_sectors.end(), sector,
					std::greater<YSTSector*>()); // position in descending order
		if(it == m_dirty_sectors.end() || *it != sector)
			m_dirty_sectors.insert(it, sector);
	}

	int32_t get_yst(int32_t x, int32_t z)
	{
		auto sector_p = get_sector_p(x, z);
		YSTSector *sector = get_sector(sector_p, false);
		if(!sector)
			return INT_MIN;
		return sector->volume->getVoxelAt(x, 0, z);
	}
};

struct Module: public interface::Module, public ground_plane_lighting::Interface
{
	interface::Server *m_server;

	up_<GlobalYSTMap> m_global_yst;

	Module(interface::Server *server):
		interface::Module("ground_plane_lighting"),
		m_server(server)
	{
	}

	~Module()
	{
	}

	void init()
	{
		m_server->sub_event(this, Event::t("core:start"));
		m_server->sub_event(this, Event::t("core:unload"));
		m_server->sub_event(this, Event::t("core:continue"));
		m_server->sub_event(this, Event::t("core:tick"));
		m_server->sub_event(this, Event::t("network:client_connected"));
		m_server->sub_event(this, Event::t("client_file:files_transmitted"));
		m_server->sub_event(this, Event::t("voxelworld:node_volume_updated"));

		voxelworld::access(m_server, [&](voxelworld::Interface *ivoxelworld)
		{
			pv::Vector3DInt16 section_size =
				ivoxelworld->get_section_size_voxels();
			pv::Vector<2, int16_t> sector_size(
						section_size.getX(), section_size.getZ());
			m_global_yst.reset(new GlobalYSTMap(sector_size));
		});
	}

	void event(const Event::Type &type, const Event::Private *p)
	{
		EVENT_VOIDN("core:start", on_start)
		EVENT_VOIDN("core:unload", on_unload)
		EVENT_VOIDN("core:continue", on_continue)
		EVENT_TYPEN("core:tick", on_tick, interface::TickEvent)
		EVENT_TYPEN("network:client_connected", on_client_connected,
				network::NewClient)
		EVENT_TYPEN("client_file:files_transmitted", on_files_transmitted,
				client_file::FilesTransmitted)
		EVENT_TYPEN("voxelworld:node_volume_updated",
				on_node_volume_updated, voxelworld::NodeVolumeUpdated)
	}

	void on_start()
	{
	}

	void on_unload()
	{
	}

	void on_continue()
	{
	}

	void on_tick(const interface::TickEvent &event)
	{
		if(!m_global_yst->m_dirty_sectors.empty()){
			sv_<YSTSector*> dirty_sectors;
			dirty_sectors.swap(m_global_yst->m_dirty_sectors);
			for(YSTSector *sector : dirty_sectors){
				ss_ s = interface::serialize_volume_compressed(*sector->volume);
				std::ostringstream os(std::ios::binary);
				{
					cereal::PortableBinaryOutputArchive ar(os);
					ar(sector->sector_p);
					ar(s);
				}
				network::access(m_server, [&](network::Interface *inetwork){
					sv_<network::PeerInfo::Id> peers = inetwork->list_peers();
					for(auto &peer: peers){
						inetwork->send(peer, "ground_plane_lighting:update",
								os.str());
					}
				});
			}
		}
	}

	void on_client_connected(const network::NewClient &client_connected)
	{
	}

	void on_files_transmitted(const client_file::FilesTransmitted &event)
	{
		int peer = event.recipient;
		network::access(m_server, [&](network::Interface *inetwork){
			inetwork->send(peer, "core:run_script",
					"require(\"buildat/module/ground_plane_lighting\")");
		});
		std::ostringstream os(std::ios::binary);
		{
			cereal::PortableBinaryOutputArchive ar(os);
			ar(m_global_yst->m_sector_size);
		}
		network::access(m_server, [&](network::Interface *inetwork){
			inetwork->send(peer, "ground_plane_lighting:init", os.str());
		});

		send_initial_sectors(peer);
	}

	void on_node_volume_updated(const voxelworld::NodeVolumeUpdated &event)
	{
		if(!event.is_static_chunk)
			return;
		log_v(MODULE, "on_node_volume_updated(): " PV3I_FORMAT,
				PV3I_PARAMS(event.chunk_p));
		const pv::Vector3DInt32 &chunk_p = event.chunk_p;
		voxelworld::access(m_server, [&](voxelworld::Interface *ivoxelworld)
		{
			interface::VoxelRegistry *voxel_reg = ivoxelworld->get_voxel_reg();
			//const auto &chunk_size_voxels = ivoxelworld->get_chunk_size_voxels();
			pv::Region chunk_region = ivoxelworld->get_chunk_region_voxels(chunk_p);

			ground_plane_lighting::access(m_server,
					[&](ground_plane_lighting::Interface *igpl)
			{
				auto lc = chunk_region.getLowerCorner();
				auto uc = chunk_region.getUpperCorner();
				//log_nv(MODULE, "yst=[");
				for(int z = lc.getZ(); z <= uc.getZ(); z++){
					for(int x = lc.getX(); x <= uc.getX(); x++){
						int32_t yst0 = igpl->get_yst(x, z);
						if(yst0 > uc.getY()){
							// Y-seethrough doesn't reach here
							continue;
						}
						int y = uc.getY();
						for(;; y--){
							VoxelInstance v = ivoxelworld->get_voxel(
										pv::Vector3DInt32(x, y, z), true);
							if(v.get_id() == interface::VOXELTYPEID_UNDEFINED){
								// NOTE: This leaves the chunks below unhandled;
								// there would have to be some kind of a dirty
								// flag based on which this seach would be
								// continued at a later point when the chunk
								// gets loaded
								break;
							}
							const auto *def = voxel_reg->get_cached(v);
							if(!def)
								throw Exception(ss_()+"Undefined voxel: "+
										itos(v.get_id()));
							bool light_passes = (!def || !def->physically_solid);
							if(!light_passes)
								break;
						}
						// The first voxel downwards from the top of the world that
						// doesn't pass light
						int32_t yst1 = y;
						//log_nv(MODULE, "%i -> %i, ", yst0, yst1);
						igpl->set_yst(x, z, yst1);
					}
				}
				//log_v(MODULE, "]");
			});
		});
	}

	void send_initial_sectors(int peer)
	{
		for(auto &pair : m_global_yst->m_sectors){
			YSTSector *sector = &pair.second;
			ss_ s = interface::serialize_volume_compressed(*sector->volume);
			std::ostringstream os(std::ios::binary);
			{
				cereal::PortableBinaryOutputArchive ar(os);
				ar(sector->sector_p);
				ar(s);
			}
			network::access(m_server, [&](network::Interface *inetwork){
				inetwork->send(peer, "ground_plane_lighting:update", os.str());
			});
		}
	}

	// Interface

	void set_yst(int32_t x, int32_t z, int32_t yst)
	{
		return m_global_yst->set_yst(x, z, yst);
	}

	int32_t get_yst(int32_t x, int32_t z)
	{
		return m_global_yst->get_yst(x, z);
	}

	void* get_interface()
	{
		return dynamic_cast<Interface*>(this);
	}
};

extern "C" {
	BUILDAT_EXPORT void* createModule_ground_plane_lighting(
			interface::Server *server){
		return (void*)(new Module(server));
	}
}
}

// vim: set noet ts=4 sw=4:
