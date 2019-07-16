#pragma once

#include "system.h"

namespace Renderer::Impl
{
	constexpr const char eventHandleName[] = "fence event";
	struct EventHandle : System::Handle<eventHandleName>
	{
		EventHandle();
	};
}