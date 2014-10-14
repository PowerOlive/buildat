// http://www.apache.org/licenses/LICENSE-2.0
// Copyright 2014 Perttu Ahola <celeron55@gmail.com>
#pragma once
#include "core/types.h"
#include <Vector2.h>
#include <Ptr.h>

namespace Urho3D
{
	class Context;
	class Texture2D;
	class Image;
}

namespace interface
{
	namespace magic = Urho3D;

	static constexpr uint ATLAS_UNDEFINED = 0;

	struct AtlasSegmentReference
	{
		uint atlas_id = ATLAS_UNDEFINED;// 0 = undefined atlas
		uint segment_id = 0;
	};

	const uint8_t ATLAS_LOD_TOP_FACE = 0x10;
	const uint8_t ATLAS_LOD_HALFBRIGHT_FACE = 0x20;

	struct AtlasSegmentDefinition
	{
		ss_ resource_name;	// If "", segment won't be added
		magic::IntVector2 total_segments;
		magic::IntVector2 select_segment;
		// Mask 0x0f: LOD level, mask 0xf0: flags
		uint8_t lod_simulation = 0;
		// TODO: Rotation

		bool operator==(const AtlasSegmentDefinition &other) const;
	};

	struct AtlasSegmentCache
	{
		magic::Texture2D *texture = nullptr;
		magic::Vector2 coord0;
		magic::Vector2 coord1;
	};

	struct TextureAtlasDefinition
	{
		uint id = ATLAS_UNDEFINED;
		magic::IntVector2 segment_resolution;
		magic::IntVector2 total_segments;
		sv_<AtlasSegmentDefinition> segments;
	};

	struct TextureAtlasCache
	{
		magic::SharedPtr<magic::Image> image;
		magic::SharedPtr<magic::Texture2D> texture;
		magic::IntVector2 segment_resolution;
		magic::IntVector2 total_segments;
		sv_<AtlasSegmentCache> segments;
	};

	struct TextureAtlasRegistry
	{
		virtual ~TextureAtlasRegistry(){}

		virtual const AtlasSegmentReference add_segment(
				const AtlasSegmentDefinition &segment_def) = 0;
		virtual const AtlasSegmentReference find_or_add_segment(
				const AtlasSegmentDefinition &segment_def) = 0;

		virtual const TextureAtlasDefinition* get_atlas_definition(
				uint atlas_id) = 0;
		virtual const AtlasSegmentDefinition* get_segment_definition(
				const AtlasSegmentReference &ref) = 0;

		virtual const TextureAtlasCache* get_atlas_cache(uint atlas_id) = 0;

		virtual const AtlasSegmentCache* get_texture(
				const AtlasSegmentReference &ref) = 0;

		virtual void update() = 0;
	};

	TextureAtlasRegistry* createTextureAtlasRegistry(magic::Context *context);
}
// vim: set noet ts=4 sw=4:
