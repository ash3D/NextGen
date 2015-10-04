/**
\author		Alexey Shaydurov aka ASH
\date		4.10.2015 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "Interface/Renderer.h"
#include "Dtor.h"

namespace DisplayModesImpl
{
	extern const class CDisplayModes displayModes;
	namespace Interface = DGLE2::Renderer::HighLevel::DisplayModes;
}

class DisplayModesImpl::CDisplayModes: public Interface::IDisplayModes
{
	class CDesc: DtorImpl::CDtor, public Interface::IDesc
	{
	public:
		CDesc(const std::string &&desc): _desc(desc) {}
		virtual operator const char *() const override
		{
			return _desc.c_str();
		}
	private:
		const std::string _desc;
		//virtual ~CDesc() = default;
	};
public:
	CDisplayModes();
	virtual unsigned Count() const override
	{
		return _modes.size();
	}
	virtual TDispModeDesc Get(unsigned idx) const override;
public:
	const DXGI_MODE_DESC &GetDX11Mode(unsigned idx) const
	{
		return _modes[idx];
	}
private:
	std::vector<DXGI_MODE_DESC> _modes;
};