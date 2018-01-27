/**
\author		Alexey Shaydurov aka ASH
\date		27.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "tracked ref.h"

namespace Renderer::Impl::TrackedRef
{
	template<class Class>
	void Ref<Class>::AttachTarget(Base &target) noexcept
	{
		assert(target);

		// detach from old ref if already attached
		if (target->refPtr)
			target->refPtr->reset();

		// attach to this ref
		target->refPtr = this;
	}

	template<class Class>
	inline void Ref<Class>::DetachTarget() noexcept
	{
		if (*this)
			(*this)->refPtr = nullptr;
	}

	template<class Class>
	inline void Ref<Class>::TrackRefMove() noexcept
	{
		if (*this)
			(*this)->refPtr = this;
	}

	template<class Class>
	inline Ref<Class>::Ref(Ref &&src) noexcept : Base(std::move(src))
	{
		TrackRefMove();
	}

	template<class Class>
	inline void Ref<Class>::operator =(Ref &&src) noexcept
	{
		Base::operator =(move(src));
		TrackRefMove();
	}

	template<class Class>
	inline Ref<Class>::~Ref()
	{
		DetachTarget();
	}

	template<class Class>
	template<class Src>
	inline auto &Ref<Class>::operator =(Src &&src) noexcept
	{
		static_assert(noexcept(Base::operator =(std::forward<Src>(src))));	// rejects overload for std::unique_ptr
		DetachTarget();
		AttachTarget(src);
		return Base::operator =(std::forward<Src>(src));
	}

	template<class Class>
	inline void Ref<Class>::swap(Base &target) noexcept
	{
		DetachTarget();
		AttachTarget(target);
		Base::swap(target);
	}
}