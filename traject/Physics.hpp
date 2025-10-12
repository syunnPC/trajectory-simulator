#pragma once

#include <algorithm>
#include <vector>
#include <cmath>
#include <cfloat>
#include <cstdint>

namespace PitchSim
{
	struct DVec3
	{
		double X;
		double Y;
		double Z;
	};

	struct Float3
	{
		float X;
		float Y;
		float Z;
	};

	struct SimParams
	{
		double ReleaseHeight_cm = 170.0;
		double InitialSpeed_mps = 41.6667;
		double Elevation_deg = 0.0;
		double Azimuth_deg = 0.0;

		double SpinRPM = 1800.0;
		DVec3 SpinAxis{ 0.0, 1.0, 0.0 };

		double Radius_mm = 73.00;
		double Mass_kg = 0.142;

		double AirTemp_C = 15.0;
		double RelHumidity_pct = 50.0;
		double Pressure_hPa = 1013.25;
		bool UseAltitudePressure = false;
		double Altitude_m = 0.0;

		double Dt_s = 0.0005;
		bool StopOnGroundHit = false;
	};

	inline constexpr double PLATE_DISTANCE_M = 18.44;
	inline constexpr double G_STANDARD = 9.80665;
	inline constexpr double MOUND_OFFSET_M = 0.254;

	inline DVec3 Add(const DVec3& a, const DVec3& b) noexcept
	{
		DVec3 r{ a.X + b.X, a.Y + b.Y, a.Z + b.Z };
		return r;
	}

	inline DVec3 Sub(const DVec3& a, const DVec3& b) noexcept
	{
		DVec3 r{ a.X - b.X, a.Y - b.Y, a.Z - b.Z };
		return r;
	}

	inline DVec3 Mul(const DVec3& a, double k) noexcept
	{
		DVec3 r{ a.X * k, a.Y * k, a.Z * k };
		return r;
	}

	inline double Dot(const DVec3& a, const DVec3& b) noexcept
	{
		double d = a.X * b.X + a.Y * b.Y + a.Z * b.Z;
		return d;
	}

	inline DVec3 Cross(const DVec3& a, const DVec3& b) noexcept
	{
		DVec3 c =
		{
			a.Y * b.Z - a.Z * b.Y,
			a.Z * b.X - a.X * b.Z,
			a.X * b.Y - a.Y * b.X
		};

		return c;
	}

	inline double Norm(const DVec3& a) noexcept
	{
		double n = std::sqrt(Dot(a, a));
		return n;
	}

	inline DVec3 Normalize(const DVec3& a) noexcept
	{
		double n = Norm(a);
		if (n > 1e-15)
		{
			return Mul(a, 1.0 / n);
		}
		else
		{
			DVec3 z{ 0.0, 0.0, 0.0 };
			return z;
		}
	}

	inline double BuckSaturationVaporPressure_hPa(double tempC) noexcept
	{
		double T = tempC;

		if (T >= 0.0)
		{
			double v = (18.678 - T / 234.5) * (T / (257.14 + T));
			double ps = 6.1121 * std::exp(v);
			return ps;
		}
		else
		{
			double v = (23.036 - T / 333.7) * (T / (279.82 + T));
			double ps = 6.1115 * std::exp(v);
			return ps;
		}
	}

	inline double PressureFromAltitude_hPa(double h) noexcept
	{
		constexpr double P0 = 1013.25;
		constexpr double T0 = 288.15;
		constexpr double L = 0.0065;
		constexpr double G0 = 9.80665;
		constexpr double M = 0.0289644;
		constexpr double R = 8.3144598;

		double exponent = (G0 * M) / (R * L);
		double p = P0 * std::pow(1.0 - (L * h) / T0, exponent);
		return p;
	}

	inline double ComputeAirDensity_kg_per_m3(double tempC, double RHpct, double p_hPa)
	{
		double T = tempC + 273.15;
		double RH = std::clamp(RHpct, 0.0, 100.0) * 0.01;
		double ps_hPa = BuckSaturationVaporPressure_hPa(tempC);
		double e_hPa = RH * ps_hPa;

		double Pd_Pa = (p_hPa - e_hPa) * 100.0;
		double Pv_Pa = e_hPa * 100.0;

		constexpr double RD = 287.058;
		constexpr double RV = 461.495;

		double rho = Pd_Pa / (RD * T) + Pv_Pa / (RV * T);
		return rho;
	}

	inline double LiftCoeffFormS(double S) noexcept
	{
		constexpr double CL0 = 0.583;
		constexpr double CL1 = 2.333;
		constexpr double CL2 = 1.120;

		double cl = (CL2 * S) / (CL0 + CL1 * S + 1e-12);
		return cl;
	}

	inline double DragCoeffFromRPM(double rpm) noexcept
	{
		constexpr double CD0 = 0.297;
		constexpr double CD1 = 0.0292;

		double cd = CD0 + CD1 * (rpm / 1000.0);
		return cd;
	}
}