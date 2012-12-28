/**
\author		Alexey Shaydurov aka ASH
\date		28.12.2012 (c)Alexey Shaydurov

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

// define dummies for unsupported C++11 features
#if defined _MSC_VER & _MSC_VER <= 1700
#	define MSVC_LIMITATIONS
#endif
#ifdef MSVC_LIMITATIONS
#	define noexcept
#	define constexpr const
#endif
#if defined _MSC_VER & _MSC_VER <= 1600
#	define final
#endif
#if defined _MSC_VER & _MSC_VER == 1700
#	define NO_INIT_LIST
#endif