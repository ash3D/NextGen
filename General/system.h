#pragma once

#include <filesystem>

namespace System
{
	extern bool __fastcall DetectSSD(const std::filesystem::path &location) noexcept;
}