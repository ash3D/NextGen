/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <wrl/client.h>

struct IDXGISwapChain4;
struct ID3D12DescriptorHeap;

namespace Renderer
{
	namespace WRL = Microsoft::WRL;

	class RenderOutput
	{
		WRL::ComPtr<IDXGISwapChain4> swapChain;
		WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
		std::shared_ptr<class Viewport> viewport;

	public:
		RenderOutput(HWND wnd, bool allowModeSwitch, unsigned int bufferCount = 2);
		RenderOutput(const RenderOutput &);
		RenderOutput(RenderOutput &&);
		RenderOutput &operator =(const RenderOutput &);
		RenderOutput &operator =(RenderOutput &&);
		~RenderOutput();

	public:
		void Monitor_ALT_ENTER(bool enable);
		void SetViewport(std::shared_ptr<Viewport> viewport);
		void Resize(unsigned int width, unsigned int height), OnResize();
		void NextFrame(bool vsync = true);

	private:
		void Fill_RTV_Heap(unsigned int bufferCount);
	};
}