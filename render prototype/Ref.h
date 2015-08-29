/**
\author		Alexey Shaydurov aka ASH
\date		26.4.2013 (c)Korotkov Andrey

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

	private:
		// use SFINAE to redirect to appropriate pointer cast (dynamic for virtual inheritence; static otherwise)
		template<class Class>
		struct TIsStaticCastPossible
		{
		private:
			template<class Test>
			static std::true_type test(decltype(static_cast<Test *>(std::declval<CRef *>())) *);

			template<class>
			static std::false_type test(...);

		private:
#ifdef MSVC_LIMITATIONS
			typedef std::false_type type;
#else
			typedef decltype(test<Class>(nullptr)) type;
#endif

		public:
			static constexpr bool value = type::value;
		};

	protected:
		template<class Class>
		typename std::enable_if<TIsStaticCastPossible<Class>::value, std::shared_ptr<Class>>::type GetRef()
		{
			return std::static_pointer_cast<Class>(_externRef);
		}

		template<class Class>
		typename std::enable_if<!TIsStaticCastPossible<Class>::value, std::shared_ptr<Class>>::type GetRef()
		{
			return std::dynamic_pointer_cast<Class>(_externRef);
		}

		template<class Class>
		typename std::enable_if<TIsStaticCastPossible<Class>::value, std::shared_ptr<const Class>>::type GetRef() const
		{
			return std::static_pointer_cast<const Class>(_externRef);
		}

		template<class Class>
		typename std::enable_if<!TIsStaticCastPossible<Class>::value, std::shared_ptr<const Class>>::type GetRef() const
		{
			return std::dynamic_pointer_cast<const Class>(_externRef);
		}
	};
}