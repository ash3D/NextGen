#include "stdafx.h"
#include "tracked resource.h"

using Microsoft::WRL::ComPtr;

void RetireTrackedResource(ComPtr<IUnknown> resource)
{
	extern void RetireResource(ComPtr<IUnknown> resource);
	if (resource)
		RetireResource(std::move(resource));
}