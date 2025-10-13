#include "PitchConfig.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <limits>

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
}