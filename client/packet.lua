-- Buildat: client/packet.lua
-- http://www.apache.org/licenses/LICENSE-2.0
-- Copyright 2014 Perttu Ahola <celeron55@gmail.com>
local log = buildat.Logger("__client/packet")

local packet_subs = {}

function __buildat_handle_packet(name, data)
	local cb = packet_subs[name]
	if cb then
		cb(data)
	end
end

function buildat.sub_packet(name, cb)
	packet_subs[name] = cb
end
buildat.safe.sub_packet = buildat.sub_packet

function buildat.unsub_packet(cb)
	for name, cb1 in pairs(buildat.packet_subs) do
		if cb1 == cb then
			packet_subs[cb] = nil
		end
	end
end
buildat.safe.unsub_packet = buildat.unsub_packet

function buildat.send_packet(name, data)
	__buildat_send_packet(name, data)
end
buildat.safe.send_packet = buildat.send_packet

-- vim: set noet ts=4 sw=4:
