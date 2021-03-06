-- Buildat: extension/urho3d
-- http://www.apache.org/licenses/LICENSE-2.0
-- Copyright 2014 Perttu Ahola <celeron55@gmail.com>
local log = buildat.Logger("extension/urho3d")
local dump = buildat.dump
local magic_sandbox = require("buildat/extension/magic_sandbox")
local safe_globals = dofile(buildat.extension_path("urho3d").."/safe_globals.lua")
local safe_events = dofile(buildat.extension_path("urho3d").."/safe_events.lua")
local safe_classes = dofile(buildat.extension_path("urho3d").."/safe_classes.lua")

local Safe = {}
local Unsafe = {}

--
-- Safe interface
--

local function wc(name, def)
	Safe[name] = magic_sandbox.wrap_class(name, def)
end

local function wrap_instance(name, instance)
	local class = Safe[name]
	local class_meta = getmetatable(class)
	if not class_meta then error(dump(name).." is not a whitelisted class") end
	return class_meta.wrap(instance)
end

-- (return_types, param_types, f) or (param_types, f)
local function wrap_function(return_types, param_types, f)
	if type(param_types) == 'function' and f == nil then
		f = param_types
		param_types = return_types
		return_types = {"__safe"}
	end
	return function(...)
		local arg = {...}
		local checked_arg = {}
		for i = 1, #param_types do
			checked_arg[i] = magic_sandbox.safe_to_unsafe(arg[i], param_types[i])
		end
		local wrapped_ret = {}
		local ret = {f(unpack(checked_arg, 1, table.maxn(checked_arg)))}
		for i = 1, #return_types do
			wrapped_ret[i] = magic_sandbox.unsafe_to_safe(ret[i], return_types[i])
		end
		return unpack(wrapped_ret, 1, #return_types)
	end
end

local function self_function(function_name, return_types, param_types)
	return function(...)
		if #param_types < 1 then
			error("At least one argument required (self)")
		end
		local arg = {...}
		local checked_arg = {}
		for i = 1, #param_types do
			checked_arg[i] = magic_sandbox.safe_to_unsafe(arg[i], param_types[i])
		end
		local wrapped_ret = {}
		local self = checked_arg[1]
		local f = self[function_name]
		if type(f) ~= 'function' then
			error(dump(function_name).." not found in instance")
		end
		local ret = {f(unpack(checked_arg, 1, table.maxn(checked_arg)))}
		for i = 1, #return_types do
			wrapped_ret[i] = magic_sandbox.unsafe_to_safe(ret[i], return_types[i])
		end
		return unpack(wrapped_ret, 1, #return_types)
	end
end

local function simple_property(valid_types)
	return {
		get = function(current_value)
			return magic_sandbox.unsafe_to_safe(current_value, valid_types)
		end,
		set = function(new_value)
			return magic_sandbox.safe_to_unsafe(new_value, valid_types)
		end,
	}
end

for _, name in ipairs(safe_globals) do
	local v = _G[name]
	if type(v) ~= 'number' and type(v) ~= 'string' then
		error("Invalid safe global "..dump(name).." type: "..dump(type(v)))
	end
	Safe[name] = v
end

safe_classes.define(Safe, {
	wc = wc,
	wrap_instance = wrap_instance,
	wrap_function = wrap_function,
	self_function = self_function,
	simple_property = simple_property,
	check_safe_resource_name = Unsafe.check_safe_resource_name,
	--resave_file = Unsafe.resave_file,
})

setmetatable(Safe, {
	__index = function(t, k)
		local v = rawget(t, k)
		if v ~= nil then return v end
		error("extension/urho3d: Class "..dump(k).." is not whitelisted")
	end,
})

-- SubscribeToEvent

local sandbox_callback_to_global_function_name = {}
local next_sandbox_global_function_i = 1

function Safe.SubscribeToEvent(x, y, z)
	log:debug("Safe.SubscribeToEvent("..dump(x)..", "..dump(y)..", "..dump(z)..")")
	local object = x
	local sub_event_type = y
	local callback = z
	if z == nil then
		object = nil
		sub_event_type = x
		callback = y
	end
	if object then
		if not getmetatable(object) or not getmetatable(object).unsafe then
			error("SubscribeToEvent(): Object must be sandboxed")
		end
	end
	if not safe_events[sub_event_type] then
		error("Event type is not whitelisted: "..dump(sub_event_type))
	end
	if type(callback) == 'string' then
		-- Allow supplying callback function name like Urho3D does by default
		local caller_environment = getfenv(2)
		callback = caller_environment[callback]
		if type(callback) ~= 'function' then
			error("SubscribeToEvent(): '"..callback..
					"' is not a global function in current sandbox environment")
		end
	else
		-- Allow directly supplying callback function
	end
	local global_function_i = next_sandbox_global_function_i
	next_sandbox_global_function_i = next_sandbox_global_function_i + 1
	local global_callback_name = "__buildat_sandbox_callback_"..global_function_i
	sandbox_callback_to_global_function_name[callback] = global_callback_name
	_G[global_callback_name] = function(event_type_thing, unsafe_event_data)
		local error = error
		local f = function()
			-- How the hell does one get a string out of event_type_thing?
			-- It is not a Variant, and none of the Lua examples try to do anything
			-- with it.
			-- Let's just assume it's the correct one...
			local got_event_type = sub_event_type
			-- Filter event_data (Urho3D::VariantMap)
			local safe_fields = safe_events[got_event_type]
			if not safe_fields then
				log:warning("Received unsafe event: "..dump(got_event_type))
			end
			local safe_event_data = Safe.VariantMap()
			for field_name, field_def in pairs(safe_fields) do
				local variant_type = field_def.variant
				local safe_type = field_def.safe
				local safe_value = nil
				if variant_type == "Ptr" then
					local get_type = field_def.get_type or safe_type
					local unsafe_value = unsafe_event_data:GetPtr(
							get_type, field_name)
					if unsafe_value == nil then
						error("Value for field "..dump(field_name).." as "..
								dump(safe_type).." in "..dump(got_event_type)..
								" gotten as "..dump(get_type).." is nil")
					end
					safe_value = wrap_instance(safe_type, unsafe_value)
					safe_event_data["SetPtr"](
							safe_event_data, field_name, safe_value)
				else
					local get_type = field_def.get_type or variant_type
					local unsafe_value = unsafe_event_data["Get"..get_type](
							unsafe_event_data, field_name)
					if safe_type == 'number' or safe_type == 'string' or
							safe_type == 'boolean' then
						-- Regular type
						safe_value = magic_sandbox.unsafe_to_safe(unsafe_value, safe_type)
					else
						-- Object wrapper
						safe_value = wrap_instance(safe_type, unsafe_value)
					end
					safe_event_data["Set"..get_type](
							safe_event_data, field_name, safe_value)
				end
			end
			-- Call callback
			if object then
				callback(object, got_event_type, safe_event_data)
			else
				callback(got_event_type, safe_event_data)
			end
		end
		__buildat_run_function_in_sandbox(f)
	end
	if object then
		local unsafe_object = getmetatable(object).unsafe
		SubscribeToEvent(unsafe_object, sub_event_type, global_callback_name)
	else
		SubscribeToEvent(sub_event_type, global_callback_name)
	end
	log:debug("-> global_callback_name="..dump(global_callback_name))
	return global_callback_name
end

function Safe.UnsubscribeFromEvent(sub_event_type, cb_name)
	log:debug("Safe.UnsubscribeFromEvent("..dump(sub_event_type)..", "..dump(cb_name)..")")
	UnsubscribeFromEvent(sub_event_type, cb_name)
	-- TODO: Delete the generated global callback
end

--
-- Unsafe interface
--

-- Just wrap everything to the global environment as we don't have a full list
-- of Urho3D's API available.

setmetatable(Unsafe, {
	__index = function(t, k)
		local v = rawget(t, k)
		if v ~= nil then return v end
		return _G[k]
	end,
})

-- Unsafe SubscribeToEvent with function support

local unsafe_callback_to_global_function_name = {}
local next_unsafe_global_function_i = 1

function Unsafe.SubscribeToEvent(x, y, z)
	local object = x
	local event_name = y
	local callback = z
	if callback == nil then
		object = nil
		event_name = x
		callback = y
	end
	if type(callback) == 'string' then
		-- Allow supplying callback function name like Urho3D does by default
		local caller_environment = getfenv(2)
		callback = caller_environment[callback]
		if type(callback) ~= 'function' then
			error("SubscribeToEvent(): '"..callback..
					"' is not a global function in current unsafe environment")
		end
	else
		-- Allow directly supplying callback function
	end
	local global_function_i = next_unsafe_global_function_i
	next_unsafe_global_function_i = next_unsafe_global_function_i + 1
	local global_callback_name = "__buildat_unsafe_callback_"..global_function_i
	unsafe_callback_to_global_function_name[callback] = global_callback_name
	_G[global_callback_name] = function(event_type, event_data)
		local f = function()
			if object then
				callback(object, event_type, event_data)
			else
				callback(event_type, event_data)
			end
		end
		local ok, err = __buildat_pcall(f)
		if not ok then
			__buildat_fatal_error("Error calling callback: "..err)
		end
	end
	if object then
		SubscribeToEvent(object, event_name, global_callback_name)
	else
		SubscribeToEvent(event_name, global_callback_name)
	end
	return global_callback_name
end

--
-- Create the final interface
--

local M = {}
M.safe = Safe
M.unsafe = Unsafe

return M
-- vim: set noet ts=4 sw=4:
