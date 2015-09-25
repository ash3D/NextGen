/**
\author		Alexey Shaydurov aka ASH
\date		26.9.2015 (c)Alexey Shaydurov

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

// define dummies for unsupported C++11 features
#if defined _MSC_VER && _MSC_VER <= 1800
#	define MSVC_LIMITATIONS
#endif
#ifdef MSVC_LIMITATIONS
#	define _ALLOW_KEYWORD_MACROS
#	define noexcept
#	define constexpr const
#endif