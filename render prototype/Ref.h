/**
\author		Alexey Shaydurov aka ASH
\date		30.1.2013 (c)Korotkov Andrey

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

		public:
#ifdef MSVC_LIMITATIONS
			typedef std::false_type type;
#else
			typedef decltype(test<Class>(nullptr)) type;
#endif
		};

		template<class Class>
		inline std::shared_ptr<Class> GetRefImpl(std::true_type)
		{
			return static_pointer_cast<Class>(_externRef);
		}

		template<class Class>
		inline std::shared_ptr<const Class> GetRefImpl(std::true_type) const
		{
			return static_pointer_cast<const Class>(_externRef);
		}

		template<class Class>
		inline std::shared_ptr<Class> GetRefImpl(std::false_type)
		{
			return dynamic_pointer_cast<Class>(_externRef);
		}

		template<class Class>
		inline std::shared_ptr<const Class> GetRefImpl(std::false_type) const
		{
			return dynamic_pointer_cast<const Class>(_externRef);
		}

	protected:
		template<class Class>
		std::shared_ptr<Class> GetRef()
		{
			return GetRefImpl<Class>(typename TIsStaticCastPossible<Class>::type());
		}

		template<class Class>
		std::shared_ptr<const Class> GetRef() const
		{
			return GetRefImpl<Class>(typename TIsStaticCastPossible<Class>::type());
		}
	};
}