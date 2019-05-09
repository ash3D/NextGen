#pragma once

namespace Renderer::PIXEvents
{
	enum
	{
		ViewportPre,
		ViewportPost,
		TerrainLayer,
		TerrainOcclusionQueryPass,
		TerrainMainPass,
		TerrainBuildRenderStage,
		TerrainSchedule,
		TerrainIssue,
	};
}