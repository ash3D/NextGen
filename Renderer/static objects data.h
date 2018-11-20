#pragma once

#include "stdafx.h"
#include "world.hh"
#include "CB register.h"

struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) alignas(D3D12_COMMONSHADER_CONSTANT_BUFFER_PARTIAL_UPDATE_EXTENTS_BYTE_ALIGNMENT) Renderer::Impl::World::StaticObjectData
{
	CBRegister::AlignedRow<3> worldXform[4];
};