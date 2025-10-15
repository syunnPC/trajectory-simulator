#include "PitchConfig.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <limits>
#include <map>

namespace PitchSim::Config
{
	namespace
	{
		inline void TrimInPlace(std::string& s)
		{
			auto issp = [](unsigned char c) {return std::isspace(c) != 0; };
			s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](unsigned char c) {return !issp(c); }));
			s.erase(std::find_if(s.rbegin(), s.rend(), [&](unsigned char c) {return !issp(c); }).base(), s.end());
		}

		inline void StripUtf8Bom(std::string& s) noexcept
		{
			if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF)
			{
				s.erase(0, 3);
			}
		}

		inline bool StartsWith(const std::string s, const char* prefix)
		{
			return s.rfind(prefix, 0) == 0;
		}

		inline bool StartsWithCI(const std::string& s, const char* prefix)
		{
			const std::size_t n = std::strlen(prefix);
			if (s.size() < n)
			{
				return false;
			}

			for (std::size_t i = 0; i < n; ++i)
			{
				if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(prefix[i])))
				{
					return false;
				}
			}

			return true;
		}

		inline std::vector<std::string> SplitTopLevelCsv(const std::string& s)
		{
			std::vector<std::string> out;
			std::string cur;
			int depth = 0;
			for (char ch : s)
			{
				if (ch == '(')
				{
					++depth;
					cur.push_back(ch);
				}
				else if (ch == ')')
				{
					depth = std::max(0, depth - 1);
					cur.push_back(ch);
				}
				else if (ch == ',' && depth == 0)
				{
					TrimInPlace(cur);
					if (!cur.empty())
					{
						out.emplace_back(cur);
					}

					cur.clear();
				}
				else
				{
					cur.push_back(ch);
				}
			}

			TrimInPlace(cur);
			if (!cur.empty())
			{
				out.emplace_back(cur);
			}

			return out;
		}

		inline bool ParseAxis(const std::string& token, PitchSim::DVec3& out)
		{
			auto lp = token.find('(');
			auto rp = token.find(')');
			if (lp == std::string::npos || rp == std::string::npos || rp < lp)
			{
				return false;
			}

			std::string inside = token.substr(lp + 1, rp - lp - 1);
			std::replace(inside.begin(), inside.end(), ',', ' ');
			std::istringstream iss(inside);
			double x = 0.0;
			double y = 0.0;
			double z = 0.0;
			if (!(iss >> x >> y >> z))
			{
				return false;
			}
			out = PitchSim::DVec3{ x, y, z };
			return true;
		}

		inline bool ParseLineKV(const std::string& line, double& speed_kmh, PitchSim::DVec3& axis, double& rpm)
		{
			std::string s = line;
			TrimInPlace(s);
			if (s.empty())
			{
				return false;
			}

			std::vector<std::string> toks = SplitTopLevelCsv(s);

			if (toks.size() < 3)
			{
				return false;
			}

			bool okSpeed = false;
			bool okAxis = false;
			bool okRpm = false;

			for (const std::string& tRaw : toks)
			{
				std::string t = tRaw;
				TrimInPlace(t);

				if (StartsWithCI(t, "Speed="))
				{
					std::string v = t.substr(6);
					TrimInPlace(v);
					try
					{
						speed_kmh = std::stod(v);
						okSpeed = true;
					}
					catch (...)
					{
						return false;
					}
				}
				else if (StartsWithCI(t, "Axis="))
				{
					okAxis = ParseAxis(t, axis);
					if (!okAxis)
					{
						return false;
					}
				}
				else if (StartsWithCI(t, "RPM="))
				{
					std::string v = t.substr(t.find('=') + 1);
					TrimInPlace(v);
					try
					{
						rpm = std::stod(v);
						okRpm = true;
					}
					catch (...)
					{
						return false;
					}
				}
				else
				{
					//–³Ž‹
				}
			}

			return okSpeed && okAxis && okRpm;
		}

		inline bool ParseAxisEx(const std::string& token, PitchEntry& entry)
		{
			auto lp = token.find('(');
			auto rp = token.find(')');
			if (lp == std::string::npos || rp == std::string::npos || rp < lp)
			{
				return false;
			}

			std::string inside = token.substr(lp + 1, rp - lp - 1);
			std::replace(inside.begin(), inside.end(), ',', ' ');
			std::istringstream iss(inside);
			std::string exprX;
			std::string exprY;
			std::string exprZ;
			double x = 0.0;
			double y = 0.0;
			double z = 0.0;

			if (!(iss >> exprX >> exprY >> exprZ))
			{
				return false;
			}
			else
			{
				std::string& v = exprX;

				if (StartsWithCI(v, "RAND[") && v[v.length() - 1] == ']')
				{
					std::string minStr;
					std::string maxStr;
					std::size_t posBegin = v.find_first_of('[');
					std::size_t posEnd = v.find_first_of(']');
					std::size_t posSeparete = v.find_first_of(':');
					if (posSeparete == v.npos || v.find_last_of(':') != posSeparete || posEnd == v.npos || posBegin == v.npos)
					{
						return false;
					}
					minStr = v.substr(posBegin + 1, posSeparete - posBegin - 1);
					maxStr = v.substr(posSeparete + 1, v.length() - posSeparete - 1);

					try
					{
						double min = std::stod(minStr);
						double max = std::stod(maxStr);
						entry.IsRandomAxisX = true;
						entry.XMax = max;
						entry.XMin = min;
					}
					catch (...)
					{
						return false;
					}
				}
				else
				{
					try
					{
						x = std::stod(exprX);
					}
					catch (...)
					{
						return false;
					}
				}

				v = exprY;
				if (StartsWithCI(v, "RAND[") && v[v.length() - 1] == ']')
				{
					std::string minStr;
					std::string maxStr;
					std::size_t posBegin = v.find_first_of('[');
					std::size_t posEnd = v.find_first_of(']');
					std::size_t posSeparete = v.find_first_of(':');
					if (posSeparete == v.npos || v.find_last_of(':') != posSeparete || posEnd == v.npos || posBegin == v.npos)
					{
						return false;
					}
					minStr = v.substr(posBegin + 1, posSeparete - posBegin - 1);
					maxStr = v.substr(posSeparete + 1, v.length() - posSeparete - 1);

					try
					{
						double min = std::stod(minStr);
						double max = std::stod(maxStr);
						entry.IsRandomAxisY = true;
						entry.YMax = max;
						entry.YMin = min;
					}
					catch (...)
					{
						return false;
					}
				}
				else
				{
					try
					{
						y = std::stod(exprY);
					}
					catch (...)
					{
						return false;
					}
				}

				v = exprZ;
				if (StartsWithCI(v, "RAND[") && v[v.length() - 1] == ']')
				{
					std::string minStr;
					std::string maxStr;
					std::size_t posBegin = v.find_first_of('[');
					std::size_t posEnd = v.find_first_of(']');
					std::size_t posSeparete = v.find_first_of(':');
					if (posSeparete == v.npos || v.find_last_of(':') != posSeparete || posEnd == v.npos || posBegin == v.npos)
					{
						return false;
					}
					minStr = v.substr(posBegin + 1, posSeparete - posBegin - 1);
					maxStr = v.substr(posSeparete + 1, v.length() - posSeparete - 1);

					try
					{
						double min = std::stod(minStr);
						double max = std::stod(maxStr);
						entry.IsRandomAxisZ = true;
						entry.ZMax = max;
						entry.ZMin = min;
					}
					catch (...)
					{
						return false;
					}
				}
				else
				{
					try
					{
						z = std::stod(exprZ);
					}
					catch (...)
					{
						return false;
					}
				}
			}

			entry.Axis = PitchSim::DVec3{ x, y, z };
			return true;
		}

		inline bool ParseLineKV(const std::string& line, PitchEntry& entry)
		{
			std::string s = line;
			TrimInPlace(s);
			if (s.empty())
			{
				return false;
			}

			std::vector<std::string> toks = SplitTopLevelCsv(s);
			if (toks.empty())
			{
				return false;
			}

			bool okSpeed = false;
			bool okAxis = false;
			bool okRpm = false;

			for (const std::string& tRaw : toks)
			{
				std::string t = tRaw;
				TrimInPlace(t);

				if (StartsWithCI(t, "Speed="))
				{
					std::string v = t.substr(t.find('=') + 1);
					TrimInPlace(v);
					if (StartsWithCI(v, "RAND[") && v[v.length() - 1] == ']')
					{
						std::string minStr;
						std::string maxStr;
						std::size_t posBegin = v.find_first_of('[');
						std::size_t posEnd = v.find_first_of(']');
						std::size_t posSeparete = v.find_first_of(':');
						if (posSeparete == v.npos || v.find_last_of(':') != posSeparete || posEnd == v.npos || posBegin == v.npos)
						{
							return false;
						}
						minStr = v.substr(posBegin + 1, posSeparete - posBegin - 1);
						maxStr = v.substr(posSeparete + 1, v.length() - posSeparete - 1);

						try
						{
							double min = std::stod(minStr);
							double max = std::stod(maxStr);
							entry.IsRandomSpeed = true;
							entry.SpeedMax = max;
							entry.SpeedMin = min;
							okSpeed = true;

						}
						catch (...)
						{
							return false;
						}
					}
					else
					{
						try
						{
							entry.Speed_kmh = std::stod(v);
							okSpeed = true;
						}
						catch (...)
						{
							return false;
						}
					}
				}
				else if (StartsWithCI(t, "Axis="))
				{
					if (!ParseAxisEx(t, entry))
					{
						return false;
					}

					okAxis = true;
				}
				else if (StartsWithCI(t, "RPM="))
				{
					std::string v = t.substr(t.find('=') + 1);
					TrimInPlace(v);
					if (StartsWithCI(v, "RAND[") && v[v.length() - 1] == ']')
					{
						std::string minStr;
						std::string maxStr;
						std::size_t posBegin = v.find_first_of('[');
						std::size_t posEnd = v.find_first_of(']');
						std::size_t posSeparete = v.find_first_of(':');
						if (posSeparete == v.npos || v.find_last_of(':') != posSeparete || posEnd == v.npos || posBegin == v.npos)
						{
							return false;
						}
						minStr = v.substr(posBegin + 1, posSeparete - posBegin - 1);
						maxStr = v.substr(posSeparete + 1, v.length() - posSeparete - 1);

						try
						{
							double min = std::stod(minStr);
							double max = std::stod(maxStr);
							entry.IsRandomRpm = true;
							entry.RpmMax = max;
							entry.RpmMin = min;
							okRpm = true;

						}
						catch (...)
						{
							return false;
						}
					}
					else
					{
						try
						{
							entry.Rpm = std::stod(v);
							okRpm = true;
						}
						catch (...)
						{
							return false;
						}
					}
				}
				else if (StartsWithCI(t, "Release=") || StartsWithCI(t, "ReleaseHeight="))
				{
					std::string v = t.substr(t.find('=') + 1);
					TrimInPlace(v);
					if (StartsWithCI(v, "RAND[") && v[v.length() - 1] == ']')
					{
						std::string minStr;
						std::string maxStr;
						std::size_t posBegin = v.find_first_of('[');
						std::size_t posEnd = v.find_first_of(']');
						std::size_t posSeparete = v.find_first_of(':');
						if (posSeparete == v.npos || v.find_last_of(':') != posSeparete || posEnd == v.npos || posBegin == v.npos)
						{
							return false;
						}
						minStr = v.substr(posBegin + 1, posSeparete - posBegin - 1);
						maxStr = v.substr(posSeparete + 1, v.length() - posSeparete - 1);

						try
						{
							double min = std::stod(minStr);
							double max = std::stod(maxStr);
							entry.IsRandomRelease = true;
							entry.ReleaseMax = max;
							entry.ReleaseMin = min;
						}
						catch (...)
						{
							return false;
						}
					}
					else
					{
						try
						{
							double val = std::stod(v);
							entry.Release_cm = val;
						}
						catch (...)
						{
							return false;
						}
					}
				}
				else if (StartsWithCI(t, "Elevation="))
				{
					std::string v = t.substr(t.find('=') + 1);
					TrimInPlace(v);
					if (StartsWithCI(v, "RAND[") && v[v.length() - 1] == ']')
					{
						std::string minStr;
						std::string maxStr;
						std::size_t posBegin = v.find_first_of('[');
						std::size_t posEnd = v.find_first_of(']');
						std::size_t posSeparete = v.find_first_of(':');
						if (posSeparete == v.npos || v.find_last_of(':') != posSeparete || posEnd == v.npos || posBegin == v.npos)
						{
							return false;
						}
						minStr = v.substr(posBegin + 1, posSeparete - posBegin - 1);
						maxStr = v.substr(posSeparete + 1, v.length() - posSeparete - 1);

						try
						{
							double min = std::stod(minStr);
							double max = std::stod(maxStr);
							entry.IsRandomElevation = true;
							entry.ElevationMax = max;
							entry.ElevationMin = min;
						}
						catch (...)
						{
							return false;
						}
					}
					else
					{
						try
						{
							double val = std::stod(v);
							entry.Elevation_deg = val;
						}
						catch (...)
						{
							return false;
						}
					}
				}
				else if (StartsWithCI(t, "Azimuth="))
				{
					std::string v = t.substr(t.find('=') + 1);
					TrimInPlace(v);
					if (StartsWithCI(v, "RAND[") && v[v.length() - 1] == ']')
					{
						std::string minStr;
						std::string maxStr;
						std::size_t posBegin = v.find_first_of('[');
						std::size_t posEnd = v.find_first_of(']');
						std::size_t posSeparete = v.find_first_of(':');
						if (posSeparete == v.npos || v.find_last_of(':') != posSeparete || posEnd == v.npos || posBegin == v.npos)
						{
							return false;
						}
						minStr = v.substr(posBegin + 1, posSeparete - posBegin - 1);
						maxStr = v.substr(posSeparete + 1, v.length() - posSeparete - 1);

						try
						{
							double min = std::stod(minStr);
							double max = std::stod(maxStr);
							entry.IsRandomAzimuth = true;
							entry.AzimuthMax = max;
							entry.AzimuthMin = min;
						}
						catch (...)
						{
							return false;
						}
					}
					else
					{
						try
						{
							double val = std::stod(v);
							entry.Azimuth_deg = val;
						}
						catch (...)
						{
							return false;
						}
					}	
				}
				else
				{
					//–³Ž‹
				}
			}

			return okSpeed && okAxis && okRpm;
		}

		inline bool ParseLineKV(const std::string& line, double& speed_kmh, PitchSim::DVec3& axis, double& rpm, std::optional<double>& release_cm, std::optional<double>& elevation_deg, std::optional<double>& azimuth_deg)
		{
			std::string s = line;
			TrimInPlace(s);
			if (s.empty())
			{
				return false;
			}

			std::vector<std::string> toks = SplitTopLevelCsv(s);
			if (toks.empty())
			{
				return false;
			}

			bool okSpeed = false;
			bool okAxis = false;
			bool okRpm = false;

			for (const std::string& tRaw : toks)
			{
				std::string t = tRaw;
				TrimInPlace(t);

				if (StartsWithCI(t, "Speed="))
				{
					std::string v = t.substr(t.find('=') + 1);
					TrimInPlace(v);
					try
					{
						speed_kmh = std::stod(v);
						okSpeed = true;
					}
					catch (...)
					{
						return false;
					}
				}
				else if (StartsWithCI(t, "Axis="))
				{
					if (!ParseAxis(t, axis))
					{
						return false;
					}

					okAxis = true;
				}
				else if (StartsWithCI(t, "RPM="))
				{
					std::string v = t.substr(t.find('=') + 1);
					TrimInPlace(v);
					try
					{
						rpm = std::stod(v);
						okRpm = true;
					}
					catch (...)
					{
						return false;
					}
				}
				else if (StartsWithCI(t, "Release=") || StartsWithCI(t, "ReleaseHeight="))
				{
					std::string v = t.substr(t.find('=') + 1);
					TrimInPlace(v);
					try
					{
						double val = std::stod(v);
						release_cm = val;
					}
					catch (...)
					{
						return false;
					}
				}
				else if (StartsWithCI(t, "Elevation="))
				{
					std::string v = t.substr(t.find('=') + 1);
					TrimInPlace(v);
					try
					{
						double val = std::stod(v);
						elevation_deg = val;
					}
					catch (...)
					{
						return false;
					}
				}
				else if (StartsWithCI(t, "Azimuth="))
				{
					std::string v = t.substr(t.find('=') + 1);
					TrimInPlace(v);
					try
					{
						double val = std::stod(v);
						azimuth_deg = val;
					}
					catch (...)
					{
						return false;
					}
				}
				else
				{
					//–³Ž‹
				}
			}

			return okSpeed && okAxis && okRpm;
		}
	}

	bool LoadPitchConfigFile(const std::string& pathUtf8, std::vector<PitchEntry>& outList, std::size_t maxCount)
	{
		outList.clear();

		std::ifstream ifs(pathUtf8, std::ios::in);
		if (!ifs)
		{
			return false;
		}

		std::string line;
		std::string currentLabel;
		bool firstLine = true;

		while (std::getline(ifs, line))
		{
			if (firstLine)
			{
				StripUtf8Bom(line);
				firstLine = false;
			}

			TrimInPlace(line);
			if (line.empty())
			{
				continue;
			}

			if (line[0] == '/')
			{
				continue;
			}

			if (line[0] == '#')
			{
				currentLabel = line.substr(1);
				TrimInPlace(currentLabel);
				continue;
			}

			if (!currentLabel.empty())
			{
				double spd = 0.0;
				PitchSim::DVec3 axis{ 0.0, 0.0, 0.0 };
				double rpm = 0.0;

				std::optional<double> rel;
				std::optional<double> elv;
				std::optional<double> azm;

				if (ParseLineKV(line, spd, axis, rpm, rel, elv, azm))
				{
					PitchEntry e;
					e.Label = currentLabel;
					e.Speed_kmh = spd;
					e.Axis = axis;
					e.Rpm = rpm;
					e.Release_cm = rel;
					e.Elevation_deg = elv;
					e.Azimuth_deg = azm;

					outList.emplace_back(e);
					currentLabel.clear();

					if (outList.size() >= maxCount)
					{
						break;
					}
				}
				else
				{
					currentLabel.clear();
				}
			}
			else
			{
				//–³Ž‹
			}
		}

		return !outList.empty();
	}

	bool LoadPitchConfigFileEx(const std::string& pathUtf8, std::vector<PitchEntry>& outList, std::size_t maxCount)
	{
		outList.clear();

		std::ifstream ifs(pathUtf8, std::ios::in);
		if (!ifs)
		{
			return false;
		}

		std::string line;
		std::string currentLabel;
		bool firstLine = true;

		while (std::getline(ifs, line))
		{
			if (firstLine)
			{
				StripUtf8Bom(line);
				firstLine = false;
			}

			TrimInPlace(line);
			if (line.empty())
			{
				continue;
			}

			if (line[0] == '/')
			{
				continue;
			}

			if (line[0] == '#')
			{
				currentLabel = line.substr(1);
				TrimInPlace(currentLabel);
				continue;
			}

			if (!currentLabel.empty())
			{
				double spd = 0.0;
				PitchSim::DVec3 axis{ 0.0, 0.0, 0.0 };
				double rpm = 0.0;

				std::optional<double> rel;
				std::optional<double> elv;
				std::optional<double> azm;

				PitchEntry pe{};

				if (ParseLineKV(line, pe))
				{
					pe.Label = currentLabel;
					outList.emplace_back(pe);
					currentLabel.clear();

					if (outList.size() >= maxCount)
					{
						break;
					}
				}
				else
				{
					currentLabel.clear();
				}
			}
			else
			{
				//–³Ž‹
			}
		}

		return !outList.empty();
	}

	bool LoadEnvConfigFile(const std::string& pathUtf8, EnvironmentSettings& outSettings)
	{
		std::ifstream ifs(pathUtf8);
		if (!ifs)
		{
			return false;
		}

		std::string line;
		std::map<std::string, std::string> set;
		while (std::getline(ifs, line))
		{
			TrimInPlace(line);
			if (line[0] == '#')
			{
				continue;
			}

			if (line.empty())
			{
				continue;
			}

			std::size_t n = line.find('=');
			if (n == std::string::npos)
			{
				continue;
			}

			std::string key{};
			std::string value{};
			std::string keyUpper{};
			std::string valueUpper{};

			key = line.substr(0, n);
			value = line.substr(n + 1, line.length() - key.length() - 1);

			keyUpper.resize(key.size());
			valueUpper.resize(value.size());

			std::transform(key.begin(), key.end(), keyUpper.begin(), [](unsigned char c) {return std::toupper(c); });
			std::transform(value.begin(), value.end(), valueUpper.begin(), [](unsigned char c) {return std::toupper(c); });

			set[key] = value;
		}

		EnvironmentSettings& s = outSettings;

		constexpr auto PRESSURE_SETTING_KEY = "PRESSURE";
		constexpr auto PRESSURE_USEHEIGHT_KEY = "USEHEIGHT";
		constexpr auto HEIGHT_KEY = "HEIGHT";
		constexpr auto SPEED_SCALE_KEY = "SPEED";
		constexpr auto AIR_TEMP_KEY = "TEMP";
		constexpr auto DT_KEY = "DT";
		constexpr auto RELHUMID_KEY = "HUMID";
		constexpr auto RADIUS_KEY = "RADIUS";
		constexpr auto MASS_KEY = "MASS";
		constexpr auto ZONE_HEIGHT_KEY = "ZONEHEIGHT";
		constexpr auto ZONE_SIZE_HEIGHT_KEY = "ZONESIZEHEIGHT";
		constexpr auto MSAA_COUNT_KEY = "MSAA";
		constexpr auto QUALITY_KEY = "QUALITY";

		if (set[PRESSURE_SETTING_KEY] != "")
		{
			try
			{
				s.Pressure_hPa = std::stod(set[PRESSURE_SETTING_KEY]);
			}
			catch (...)
			{
				return false;
			}
		}

		if (set[PRESSURE_USEHEIGHT_KEY] != "")
		{
			if (set[PRESSURE_USEHEIGHT_KEY] == "TRUE")
			{
				s.UseHeightPressure = true;
			}
			else if (set[PRESSURE_USEHEIGHT_KEY] == "FALSE")
			{
				s.UseHeightPressure = false;
			}
			else
			{
				return false;
			}
		}

		if (set[HEIGHT_KEY] != "")
		{
			try
			{
				s.Height_m = std::stod(set[HEIGHT_KEY]);
			}
			catch (...)
			{
				return false;
			}
		}

		if (set[SPEED_SCALE_KEY] != "")
		{
			try
			{
				s.PitchSpeedScale = (1 / std::stod(set[SPEED_SCALE_KEY]));
			}
			catch (...)
			{
				return false;
			}
		}

		if (set[AIR_TEMP_KEY] != "")
		{
			try
			{
				s.AirTemp_C = std::stod(set[AIR_TEMP_KEY]);
			}
			catch (...)
			{
				return false;
			}
		}

		if (set[DT_KEY] != "")
		{
			try
			{
				s.Dt_s = std::stod(set[DT_KEY]);
			}
			catch (...)
			{
				return false;
			}
		}

		if (set[RELHUMID_KEY] != "")
		{
			try
			{
				s.RelHumid_pct = std::stod(set[RELHUMID_KEY]);
			}
			catch (...)
			{
				return false;
			}
		}

		if (set[RADIUS_KEY] != "")
		{
			try
			{
				s.Radius_mm = std::stod(set[RADIUS_KEY]);
			}
			catch (...)
			{
				return false;
			}
		}

		if (set[MASS_KEY] != "")
		{
			try
			{
				s.Mass_kg = std::stod(set[MASS_KEY]);
			}
			catch (...)
			{
				return false;
			}
		}

		if (set[ZONE_HEIGHT_KEY] != "")
		{
			try
			{
				s.ZoneHeight_m = std::stod(set[ZONE_HEIGHT_KEY]);
			}
			catch (...)
			{
				return false;
			}
		}

		if (set[ZONE_SIZE_HEIGHT_KEY] != "")
		{
			try
			{
				s.ZoneSizeHeight_m = std::stod(set[ZONE_SIZE_HEIGHT_KEY]);
			}
			catch (...)
			{
				return false;
			}
		}

		if (set[MSAA_COUNT_KEY] != "")
		{
			try
			{
				s.MsaaCount = std::stoi(set[MSAA_COUNT_KEY]);
			}
			catch (...)
			{
				return false;
			}
		}

		if (set[QUALITY_KEY] != "")
		{
			try
			{
				s.GraphicQuality = std::stoi(set[QUALITY_KEY]);
			}
			catch (...)
			{
				return false;
			}
		}

		return true;
	}
}