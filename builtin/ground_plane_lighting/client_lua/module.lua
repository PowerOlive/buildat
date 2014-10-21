-- Buildat: builtin/ground_plane_lighting/client_lua/module.lua
-- http://www.apache.org/licenses/LICENSE-2.0
-- Copyright 2014 Perttu Ahola <celeron55@gmail.com>
local dump = buildat.dump
local log = buildat.Logger("ground_plane_lighting")
local magic = require("buildat/extension/urho3d")
local replicate = require("buildat/extension/replicate")
local cereal = require("buildat/extension/cereal")
local voxelworld = require("buildat/module/voxelworld")
local M = {}

-- NOTE: This should actually be always equal to the voxelworld chunk size
-- because each chunk will be rendered as a whole according to the zone its
-- center position is in
--local ideal_zone_size = 32
-- NOTE: Actually, when zone overrides are used, this can be lower; but it will
--       require more processing as all renderable things are in the same octree
local ideal_zone_size = 16

M.sector_size = buildat.Vector2(0, 0)
M.zone_div = buildat.Vector2(0, 0)
M.zone_size = buildat.Vector2(0, 0)

local function setup_sizes(sector_size)
	M.sector_size = sector_size
	M.zone_div = (M.sector_size / ideal_zone_size):floor()
	M.zone_size = buildat.Vector2(ideal_zone_size, ideal_zone_size)
	log:info("sector_size: "..M.sector_size:dump())
	log:info("zone_div: "..M.zone_div:dump())
	log:info("zone_size: "..M.zone_size:dump())
end

buildat.sub_packet("ground_plane_lighting:init", function(data)
	local values = cereal.binary_input(data, {"object",
		{"sector_size", {"object",
			{"x", "int16_t"},
			{"y", "int16_t"},
		}},
	})
	log:info("ground_plane_lighting:init: "..dump(values))
	setup_sizes(buildat.Vector2(values.sector_size))
end)

local dark_zones = {} -- {sector_y: {sector_x: {zone nodes}}}

local function get_sector_zones(x, y)
	local ytable = dark_zones[y]
	if ytable == nil then
		ytable = {}
		dark_zones[y] = ytable
	end
	local xtable = ytable[x]
	if xtable == nil then
		xtable = {}
		ytable[x] = xtable
	end
	return xtable
end

local function set_dark_zones(sector_p, yst_volume)
	log:debug("set_dark_zones(): sector_p="..sector_p:dump())
	local scene = replicate.main_scene -- TODO: More flexibility
	local sector_zones = get_sector_zones(sector_p.x, sector_p.y)
	local zone_i = 1
	for y_div = 0, M.zone_div.y  - 1 do
		for x_div = 0, M.zone_div.x - 1 do
			local zone_node = sector_zones[zone_i]
			if zone_node == nil then
				zone_node = scene:CreateChild("GPLZone")
				zone_node:CreateComponent("Zone")
				sector_zones[zone_i] = zone_node
			end
			local zone = zone_node:GetComponent("Zone")
			do
				local yst_min = 1000000
				for y0 = 0, M.zone_size.y - 1 do
					for x0 = 0, M.zone_size.x - 1 do
						-- NOTE: yst_volume uses global coordinates
						local x = sector_p.x * M.sector_size.x +
								x_div * M.zone_size.x + x0
						local y = sector_p.y * M.sector_size.y +
								y_div * M.zone_size.y + y0
						local v = yst_volume:get_voxel_at(x, 0, y)
						if v.int32 < yst_min then
							yst_min = v.int32
						end
					end
				end
				local y_min = -10000
				local y_max = yst_min - 1
				local lc = magic.Vector3(
					sector_p.x * M.sector_size.x + x_div * M.zone_size.x,
					y_min,
					sector_p.y * M.sector_size.y + y_div * M.zone_size.y
				)
				local uc = magic.Vector3(
					lc.x + M.zone_size.x,
					y_max,
					lc.z + M.zone_size.y
				)
				log:debug("Dark zone at lc=("..lc.x..", "..lc.y..", "..lc.z..
						"), uc=("..uc.x..", "..uc.y..", "..uc.z..")")
				zone.boundingBox = magic.BoundingBox(lc, uc)
				zone.ambientColor = magic.Color(0, 0, 0)
				zone.fogColor = magic.Color(0.6, 0.7, 0.8)
				zone.fogStart = 10
				zone.fogEnd = voxelworld.camera_far_clip * 1.2
				zone.override = true
			end
			zone_i = zone_i + 1
		end
	end
end

buildat.sub_packet("ground_plane_lighting:update", function(data)
	log:debug("ground_plane_lighting:update: #data="..#data)
	local values = cereal.binary_input(data, {"object",
		{"p", {"object",
			{"x", "int16_t"},
			{"y", "int16_t"},
		}},
		{"data", "string"},
	})
	--log:verbose("ground_plane_lighting:update: values="..dump(values))
	log:verbose("ground_plane_lighting:update: #data="..#values.data..
			", p=("..values.p.x..", "..values.p.y..")")

	--log:verbose("values.data="..dump(buildat.bytes(values.data)))
	local volume = buildat.deserialize_volume_int32(values.data)
	local region = volume:get_enclosing_region()
	log:debug("region: ("..region.x0..", "..region.y0..", "..region.z0..", "..
			region.x1..", "..region.y1..", "..region.z1..")")
	
	local sector_p = buildat.Vector2(values.p)
	set_dark_zones(sector_p, volume)
end)

-- TODO: Remove
voxelworld.sub_geometry_update(function(node)
--[[
	local cs = voxelworld.chunk_size_voxels
	local node_p = node:GetWorldPosition()
	local scene = node.scene

	local has_sunlight = false

	local zone_node = scene:CreateChild("Zone")
	local zone = zone_node:CreateComponent("Zone")
	zone.boundingBox = magic.BoundingBox(
			node_p - magic.Vector3(cs.x, cs.y, cs.z)/2,
			node_p + magic.Vector3(cs.x, cs.y, cs.z)/2
	)
	if has_sunlight then
		zone.ambientColor = magic.Color(0.1, 0.1, 0.1)
		zone.fogColor = magic.Color(0.6, 0.7, 0.8)
	else
		zone.ambientColor = magic.Color(0, 0, 0)
		--zone.fogColor = magic.Color(0, 0, 0)
		zone.fogColor = magic.Color(0.6, 0.7, 0.8)
	end
	--zone.ambientColor = magic.Color(
	--		math.random(), math.random(), math.random())
	--zone.fogEnd = 10 + math.random() * 50
	zone.fogStart = 10
	zone.fogEnd = voxelworld.camera_far_clip * 1.2
	--zone.ambientGradient = true
--]]
--[[
	local yst_data = node:GetVar("gpl_voxel_yst_data"):GetBuffer()
	log:verbose("#yst_data="..yst_data:GetSize())
	local yst_volume = buildat.deserialize_volume_8bit(yst_data)

	local cs = voxelworld.chunk_size_voxels

	local v_min = cs.y + 1

	for x = 0, cs.x - 1 do
		for z = 0, cs.z - 1 do
			local v = yst_volume:get_voxel_at(x, 0, z)
			--log:verbose("voxel at ("..x..",0,"..z.."): "..v.id)
			if v.data < v_min then
				v_min = v.data
			end
		end
	end

	--local v_avg = v_sum / cs.x / cs.z
	--log:verbose("v_avg: "..v_avg)

	--local has_sunlight = buildat.voxel_heuristic_has_sunlight(
	--		data, voxel_reg)
	-- TODO: This is a hack that doesn't even work properly
	--local has_sunlight = (v_min < cs.y + 1)
	local has_sunlight = (v_min < cs.y + 0)

	local node_p = node:GetWorldPosition()
	local scene = node.scene

	local zone_node = scene:CreateChild("Zone")
	local zone = zone_node:CreateComponent("Zone")
	zone.boundingBox = magic.BoundingBox(
			node_p - magic.Vector3(cs.x, cs.y, cs.z)/2,
			node_p + magic.Vector3(cs.x, cs.y, cs.z)/2
	)
	if has_sunlight then
		zone.ambientColor = magic.Color(0.1, 0.1, 0.1)
		zone.fogColor = magic.Color(0.6, 0.7, 0.8)
	else
		zone.ambientColor = magic.Color(0, 0, 0)
		--zone.fogColor = magic.Color(0, 0, 0)
		zone.fogColor = magic.Color(0.6, 0.7, 0.8)
	end
	--zone.ambientColor = magic.Color(
	--		math.random(), math.random(), math.random())
	--zone.fogEnd = 10 + math.random() * 50
	zone.fogStart = 10
	zone.fogEnd = voxelworld.camera_far_clip * 1.2
	--zone.ambientGradient = true
--]]
end)

return M
-- vim: set noet ts=4 sw=4:
