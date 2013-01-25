/**
\author		Alexey Shaydurov aka ASH
\date		25.1.2013 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "Interface\Renderer.h"

namespace DtorImpl
{
	// dtor impl with ref counting
	class CRef: virtual DGLE2::Renderer::IDtor
	{
		mutable std::shared_ptr<CRef> _externRef;	// reference from lib user

	public:
		virtual void operator ~() const override
		{
			_ASSERTE(_externRef);
			_externRef.reset();
		}

	protected:
		/*
		default shared_ptr's deleter can not access protected dtor
		dtor called from internal implementation-dependent class => 'friend shared_ptr<CRef>;' does not help
		*/
		CRef(): _externRef(this, [](const CRef *dtor){delete dtor;}) {}
	#ifdef MSVC_LIMITATIONS
		CRef(CRef &);
		void operator =(CRef &);
		virtual ~CRef() {}
	#else
		CRef(CRef &) = delete;
		void operator =(CRef &) = delete;
		virtual ~CRef() = default;
	#endif

	protected:
		//template<class Owned, bool virtualBase>
		//std::shared_ptr<Owned> GetRef();

		//template<class Owned, bool virtualBase>
		//std::shared_ptr<const Owned> GetRef() const;

		//template<class Owned>
		//std::shared_ptr<Owned> GetRef<Owned, false>()
		//{
		//	return static_pointer_cast<Owned>(_externRef);
		//}

		//template<class Owned>
		//std::shared_ptr<const Owned> GetRef<Owned, false>() const
		//{
		//	return static_pointer_cast<const Owned>(_externRef);
		//}

		template<class Owned>
		std::shared_ptr<Owned> GetRef/*<Owned, true>*/()
		{
			return dynamic_pointer_cast<Owned>(_externRef);
		}

		template<class Owned>
		std::shared_ptr<const Owned> GetRef/*<Owned, true>*/() const
		{
			return dynamic_pointer_cast<const Owned>(_externRef);
		}
	};
}