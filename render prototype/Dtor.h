/**
\author		Alexey Shaydurov aka ASH
\date		10.01.2016 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "Interface/Renderer.h"

namespace DtorImpl
{
	// simple dtor forwarding
	class CDtor: virtual DGLE2::Renderer::IDtor
	{
	public:
		virtual void operator ~() const override final
		{
			delete this;
		}
	protected:
		CDtor() = default;
		CDtor(CDtor &) = delete;
		void operator =(CDtor &) = delete;
		virtual ~CDtor() = default;
	};
}