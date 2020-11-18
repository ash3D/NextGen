#pragma once

namespace Renderer::PIXEvents
{
	enum
	{
		ViewportPre,
		ViewportPost,
		Skybox,
		TerrainLayer,
		TerrainOcclusionQueryPass,
		TerrainMainPass,
		TerrainBuildRenderStage,
		TerrainSchedule,
		TerrainIssue,
	};
}