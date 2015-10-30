/**
\author		Alexey Shaydurov aka ASH
\date		25.10.2015 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "Interface/Renderer.h"

namespace DtorImpl
{
	// dtor impl with ref counting
	class CRef: virtual DGLE2::Renderer::IDtor
	{
		mutable std::shared_ptr<CRef> _externRef;	// reference from lib user

	public:
		virtual void operator ~() const override
		{
			assert(_externRef);
			_externRef.reset();
		}

	protected:
		/*
		default shared_ptr's deleter can not access protected dtor
		dtor called from internal implementation-dependent class => 'friend shared_ptr<CRef>;' does not help
		*/
		CRef(): _externRef(this, [](const CRef *dtor){delete dtor;}) {}
		CRef(CRef &) = delete;
		void operator =(CRef &) = delete;
		virtual ~CRef() = default;

	private:
		// use SFINAE to redirect to appropriate pointer cast (dynamic for virtual inheritence; static otherwise)
		template<class Class>
		struct TIsStaticCastPossible
		{
		private:
			template<class Test>
			static std::true_type test(decltype(static_cast<Test *>(std::declval<CRef *>())));

			template<class>
			static std::false_type test(...);

		private:
			typedef decltype(test<Class>(nullptr)) type;

		public:
			static constexpr bool value = type::value;
		};

	protected:
		template<class Class>
		typename std::enable_if<TIsStaticCastPossible<Class>::value, std::shared_ptr<Class>>::type GetRef()
		{
			static_assert(std::is_base_of<CRef, Class>::value, "GetRef() with invalid target class");
			return std::static_pointer_cast<Class>(_externRef);
		}

		template<class Class>
		typename std::enable_if<!TIsStaticCastPossible<Class>::value, std::shared_ptr<Class>>::type GetRef()
		{
			static_assert(std::is_base_of<CRef, Class>::value, "GetRef() with invalid target class");
			return std::dynamic_pointer_cast<Class>(_externRef);
		}

		template<class Class>
		typename std::enable_if<TIsStaticCastPossible<Class>::value, std::shared_ptr<const Class>>::type GetRef() const
		{
			static_assert(std::is_base_of<CRef, Class>::value, "GetRef() with invalid target class");
			return std::static_pointer_cast<const Class>(_externRef);
		}

		template<class Class>
		typename std::enable_if<!TIsStaticCastPossible<Class>::value, std::shared_ptr<const Class>>::type GetRef() const
		{
			static_assert(std::is_base_of<CRef, Class>::value, "GetRef() with invalid target class");
			return std::dynamic_pointer_cast<const Class>(_externRef);
		}
	};
}