/**
\author		Alexey Shaydurov aka ASH
\date		17.03.2018 (c)Korotkov Andrey

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
			visible	[3] = {0.f, 1.f, 0.f},
			hidden	[3] = {1.f, 0.f, 0.f},
			culled	[3] = {.5f, .5f, .5f};
	}
}