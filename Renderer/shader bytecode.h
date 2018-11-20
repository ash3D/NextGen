#pragma once

#include "d3dx12.h"

namespace Renderer
{
	// extension to CD3DX12_SHADER_BYTECODE
	struct ShaderBytecode : CD3DX12_SHADER_BYTECODE
	{
		// allow all CD3DX12_SHADER_BYTECODE ctors
		using CD3DX12_SHADER_BYTECODE::CD3DX12_SHADER_BYTECODE;

		// array based ctor
		template<typename T, size_t N>
		ShaderBytecode(const T (&buffer)[N]) : CD3DX12_SHADER_BYTECODE(buffer, sizeof buffer) {}
	};
}