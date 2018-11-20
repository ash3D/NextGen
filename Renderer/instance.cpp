/**
\author		Alexey Shaydurov aka ASH
\date		03.11.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "instance.hh"

using namespace std;
using namespace Renderer;

Impl::Instance::Instance(shared_ptr<class Renderer::World> &&world, Renderer::Object3D &&object, const float (&xform)[4][3], const AABB<3> &worldAABB) :
	world(move(world)), object(move(object)), worldAABB(worldAABB.Size() < 0.f ? this->object.GetXformedAABB(xform) : worldAABB)
{
	memcpy(worldXform, xform, sizeof xform);
}

Impl::Instance::~Instance() = default;

void Impl::Instance::Render(ID3D12GraphicsCommandList2 *cmdList) const
{
	cmdList->SetGraphicsRootConstantBufferView(Renderer::Object3D::ROOT_PARAM_INSTANCE_DATA_CBV/*consider or callback to root param binding abstraction instead*/, CB_GPU_ptr);
	object.Render(cmdList);
}