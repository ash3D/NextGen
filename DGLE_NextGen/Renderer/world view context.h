/**
\author		Alexey Shaydurov aka ASH
\date		17.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "../tracked resource.h"

struct ID3D12Resource;

namespace Renderer::Impl
{
	struct WorldViewContext
	{
		friend class World;

	private:
		/*
			reference tracking here is somewhat conservative in that it can keep reference a little longer than necessary
			but tracking it on usage would require retire (and associated mutex lock) every frame
		*/
		TrackedResource<ID3D12Resource> ZBufferHistory;	// from previous frame
		unsigned long ZBufferHistoryVersion = 0;
	};
}