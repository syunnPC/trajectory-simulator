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
		bool IsRandomSpeed = false;
		double Speed_kmh = 0.0;
		std::optional<double> SpeedMin;
		std::optional<double> SpeedMax;

		bool IsRandomAxisX = false;
		bool IsRandomAxisY = false;
		bool IsRandomAxisZ = false;
		PitchSim::DVec3 Axis{ 0.0, 1.0, 0.0 };
		std::optional<double> XMin;
		std::optional<double> XMax;
		std::optional<double> YMin;
		std::optional<double> YMax;
		std::optional<double> ZMin;
		std::optional<double> ZMax;

		bool IsRandomRpm = false;
		double Rpm = 0.0;
		std::optional<double> RpmMin;
		std::optional<double> RpmMax;

		bool IsRandomRelease = false;
		std::optional<double> Release_cm;
		std::optional<double> ReleaseMin;
		std::optional<double> ReleaseMax;

		bool IsRandomElevation = false;
		std::optional<double> Elevation_deg;
		std::optional<double> ElevationMin;
		std::optional<double> ElevationMax;

		bool IsRandomAzimuth = false;
		std::optional<double> Azimuth_deg;
		std::optional<double> AzimuthMin;
		std::optional<double> AzimuthMax;
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
		std::optional<double> ZoneSizeHeight_m;
		std::optional<double> ZoneHeight_m;
		std::optional<int> MsaaCount;
		std::optional<int> GraphicQuality;
	};

	bool LoadPitchConfigFile(const std::string& pathUtf8, std::vector<PitchEntry>& outList, std::size_t maxCount = 8);

	bool LoadEnvConfigFile(const std::string& pathUtf8, EnvironmentSettings& outSettings);

	bool LoadPitchConfigFileEx(const std::string& pathUtf8, std::vector<PitchEntry>& outList, std::size_t maxCount);
}