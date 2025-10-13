#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

#include "Physics.hpp"

namespace PitchSim::Config
{
	struct PitchEntry
	{
		std::string Label;
		double Speed_kmh = 0.0;
		PitchSim::DVec3 Axis{ 0.0, 1.0, 0.0 };
		double Rpm = 0.0;

		std::optional<double> Release_cm;
		std::optional<double> Elevation_deg;
		std::optional<double> Azimuth_deg;
	};

	struct EnvironmentSettings
	{
		std::optional<double> Pressure_hPa;
		std::optional<bool> UseHeightPressure;
		std::optional<double> Height_m;
		std::optional<double> PitchSpeedScale;
		std::optional<double> AirTemp_C;
		std::optional<double> Dt_s;
		std::optional<double> RelHumid_pct;
		std::optional<double> Radius_mm;
		std::optional<double> Mass_kg;
	};

	bool LoadPitchConfigFile(const std::string& pathUtf8, std::vector<PitchEntry>& outList, std::size_t maxCount = 8);

	bool LoadEnvConfigFile(const std::string& pathUtf8, EnvironmentSettings& outSettings);
}