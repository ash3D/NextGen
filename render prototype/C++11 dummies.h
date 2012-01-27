/**
\author		Alexey Shaydurov aka ASH
\date		27.1.2012 (c)Alexey Shaydurov

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef __CPP11_DUMMIES_H__
#define __CPP11_DUMMIES_H__

// define dummies for unsupported C++11 features
#if defined _MSC_VER & _MSC_VER <= 1600
#	define noexcept
#	define final
#endif

#endif//__CPP11_DUMMIES_H__