/**
\author		Alexey Shaydurov aka ASH
\date		05.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "tonemap resource views stage.h"

namespace Renderer::Impl::Descriptors::GPUDescriptorHeap
{
	extern WRL::ComPtr<ID3D12DescriptorHeap> CreateHeap(), heap;

	D3D12_GPU_DESCRIPTOR_HANDLE SetCurFrameTonemapReductionDescs(const TonemapResourceViewsStage &src);
}