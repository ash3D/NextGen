/**
\author		Alexey Shaydurov aka ASH
\date		21.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

namespace Renderer::Impl::OcclusionCulling::DebugColors
{
	namespace Terrain
	{
		const float
			visible		[3] = {0.f, 0.f, 1.f},
			culled		[3] = {.5f, .5f, .5f};
	}

	namespace Object3D
	{
		const float
			culled		[3] = {.3f, .3f, .3f},
			hidden[2]	[3] = {1.f, 0.f, 0.f, 1.f, 1.f, 0.f},
			visible[2]	[3] = {0.f, 1.f, 0.f, 0.f, 1.f, 1.f};
	}
}