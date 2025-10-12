#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "Physics.hpp"

namespace PitchSim::Config
{
	struct PitchEntry
	{
		std::string Label;
		double Speed_kmh = 0.0;
		PitchSim::DVec3 Axis{ 0.0, 1.0, 0.0 };
		double Rpm = 0.0;
	};

	bool LoadPitchConfigFile(const std::string& pathUtf8, std::vector<PitchEntry>& outList, std::size_t maxCount = 8);
}