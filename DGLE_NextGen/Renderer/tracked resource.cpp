/**
\author		Alexey Shaydurov aka ASH
\date		15.11.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "tracked resource.h"

using Microsoft::WRL::ComPtr;

void RetireTrackedResource(ComPtr<IUnknown> resource)
{
	extern void RetireResource(ComPtr<IUnknown> resource);
	if (resource)
		RetireResource(std::move(resource));
}