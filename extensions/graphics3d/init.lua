-- Buildat: extension/graphics3d/init.lua
local polybox = require("buildat/extension/polycode_sandbox")
local log = buildat:Logger("extension/graphics3d")
local dump = buildat.dump
local M = {safe = {}}

M.safe.Scene = polybox.wrap_class("Scene", {
	constructor = function(sceneType, virtualScene)
		polybox.check_enum(sceneType, {Scene.SCENE_3D, Scene.SCENE_2D})
		polybox.check_enum(virtualScene, {true, false, "__nil"})
		return Scene(sceneType, virtualScene)
	end,
	class = {
		SCENE_3D = Scene.SCENE_3D,
		SCENE_2D = Scene.SCENE_2D,
	},
	instance = {
		addEntity = function(safe, entity_safe)
			unsafe = polybox.check_type(safe, "Scene")
			entity_unsafe = polybox.check_type(entity_safe, "ScenePrimitive")
			unsafe:addEntity(entity_unsafe)
		end,
		getDefaultCamera = function(safe)
			unsafe = polybox.check_type(safe, "Scene")
			return getmetatable(M.safe.Camera).wrap(unsafe:getDefaultCamera())
		end,
	},
})

M.safe.ScenePrimitive = polybox.wrap_class("ScenePrimitive", {
	constructor = function(type, v1, v2, v3, v4, v5)
		polybox.check_type(v1, {"number", "nil"})
		polybox.check_type(v2, {"number", "nil"})
		polybox.check_type(v3, {"number", "nil"})
		polybox.check_type(v4, {"number", "nil"})
		polybox.check_type(v5, {"number", "nil"})
		return ScenePrimitive(type, v1, v2, v3, v4, v5)
	end,
	class = {
		TYPE_BOX = ScenePrimitive.TYPE_BOX,
		TYPE_PLANE = ScenePrimitive.TYPE_PLANE,
	},
	instance = {
		loadTexture = function(safe, texture_name)
			unsafe = polybox.check_type(safe, "ScenePrimitive")
			         polybox.check_type(texture_name, "string")
			unsafe:loadTexture("foo")
		end,
		setPosition = function(safe, x, y, z)
			unsafe = polybox.check_type(safe, "ScenePrimitive")
			         polybox.check_type(x, "number")
			         polybox.check_type(y, "number")
			         polybox.check_type(z, "number")
			unsafe:setPosition(x, y, z)
		end,
	},
})

M.safe.Camera = polybox.wrap_class("Camera", {
	constructor = function()
		return Camera()
	end,
	class = {
	},
	instance = {
		setPosition = function(safe, x, y, z)
			unsafe = polybox.check_type(safe, "Camera")
			         polybox.check_type(x, "number")
			         polybox.check_type(y, "number")
			         polybox.check_type(z, "number")
			unsafe:setPosition(x, y, z)
		end,
		lookAt = function(safe, v1, v2)
			unsafe = polybox.check_type(safe, "Camera")
			unsafe_v1 = polybox.check_type(v1, "Vector3")
			unsafe_v2 = polybox.check_type(v2, "Vector3")
			unsafe:lookAt(unsafe_v1, unsafe_v2)
		end,
	},
})

M.safe.Vector3 = polybox.wrap_class("Vector3", {
	constructor = function(x, y, z)
		polybox.check_type(x, "number")
		polybox.check_type(y, "number")
		polybox.check_type(z, "number")
		return Vector3(x, y, z)
	end,
	class = {
	},
	instance = {
	},
})

return M
