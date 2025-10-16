#include "TrajectorySimulator.hpp"

#include <algorithm>
#include <memory>
#include <limits>
#include <cmath>

#include "App.hpp"

namespace PitchSim
{
	constexpr double PI = 3.14159265358979323846;

	namespace
	{
		inline DVec3 ForwardAxis() noexcept
		{
			DVec3 a{ 1.0, 0.0, 0.0 };
			return a;
		}

		inline DVec3 RightAxis() noexcept
		{
			DVec3 a{ 0.0, 0.0, 1.0 };
			return a;
		}

		inline DVec3 UpAxis() noexcept
		{
			DVec3 a{ 0.0, 1.0, 0.0 };
			return a;
		}

		inline DVec3 DirectionFromAngles(double elevation_deg, double azimuch_deg) noexcept
		{
			double el = elevation_deg * (PI / 180.0);
			double az = azimuch_deg * (PI / 180.0);

			DVec3 dir = Add(Add(Mul(ForwardAxis(), std::cos(el) * std::cos(az)), Mul(RightAxis(), std::cos(el) * std::sin(az))), Mul(UpAxis(), std::sin(el)));
			dir = Normalize(dir);
			return dir;
		}
	}

	DVec3 TrajectorySimulator::ComputeAcceleration(const DVec3& position, const DVec3& velocity, double radius_m, double mass_kg, double rho, double spin_rpm, const DVec3& omega, double g) noexcept
	{
		static_cast<void>(position);

		double speed = Norm(velocity);
		DVec3 a{ 0.0, -g, 0.0 };

		if (speed < 1e-12)
		{
			return a;
		}

		DVec3 vhat = Mul(velocity, 1.0 / speed);

		DVec3 omegaPerp = Sub(omega, Mul(vhat, Dot(omega, vhat)));
		double omegaPerpMag = Norm(omegaPerp);

		double S = (radius_m * omegaPerpMag) / std::max(1e-12, speed);
		double cl = LiftCoeffFormS(S);
		double cd = DragCoeffFromRPM(spin_rpm);

		double area = PI * radius_m * radius_m;
		double K = 0.5 * rho * area / std::max(1e-12, mass_kg);

		DVec3 dragDir = Mul(vhat, -1.0);

		DVec3 magnusDir{ 0.0, 0.0, 0.0 };
		DVec3 omegaHat = Normalize(omega);
		DVec3 c = Cross(omegaHat, vhat);
		double cLen = Norm(c);
		if (cLen > 1e-12)
		{
			magnusDir = Mul(c, 1.0 / cLen);
		}

		DVec3 drag = Mul(dragDir, K * cd * speed * speed);
		DVec3 magnus = Mul(magnusDir, K * cl * speed * speed);
		
		DVec3 sum = Add(Add(a, drag), magnus);

		return sum;
	}

	void TrajectorySimulator::Simulate(const SimParams& params, std::vector<Float3>& outPoints)
	{
		outPoints.clear();

		double g = G_STANDARD;
		double r_m = params.Radius_mm * 1e-3;
		double m_kg = std::max(1e-9, params.Mass_kg);

		double p_hPa = params.UseAltitudePressure ? PressureFromAltitude_hPa(params.Altitude_m) : params.Pressure_hPa;
		double rho = ComputeAirDensity_kg_per_m3(params.AirTemp_C, params.RelHumidity_pct, p_hPa);

		DVec3 dir = DirectionFromAngles(params.Elevation_deg, params.Azimuth_deg);

		DVec3 omegaAxis = Normalize(params.SpinAxis);
		double omegaMag = params.SpinRPM * 2.0 * PI / 60.0;
		DVec3 omega = Mul(omegaAxis, omegaMag);

		double releaseY_m = (params.ReleaseHeight_cm + 25.4) * 0.01;
		DVec3 p{ 0.0, releaseY_m, 0.0 };
		DVec3 v = Mul(dir, params.InitialSpeed_mps);

		outPoints.reserve(200000);
		outPoints.emplace_back(Float3{ static_cast<float>(p.X), static_cast<float>(p.Y), static_cast<float>(p.Z) });
		
		int maxSteps = 5000000;
		int steps = 0;
		double traveled_m = 0.0;
		double dt_s = params.Dt_s;

		auto StepRK4 = [&](DVec3& Pp, DVec3& Pv)
		{
			DVec3 k1v = ComputeAcceleration(Pp, Pv, r_m, m_kg, rho, params.SpinRPM, omega, g);
			DVec3 k1p = Pv;

			DVec3 v2 = Add(Pv, Mul(k1v, 0.5 * dt_s));
			DVec3 k2v = ComputeAcceleration(Add(Pp, Mul(k1p, 0.5 * dt_s)), v2, r_m, m_kg, rho, params.SpinRPM, omega, g);
			DVec3 k2p = v2;

			DVec3 v3 = Add(Pv, Mul(k2v, 0.5 * dt_s));
			DVec3 k3v = ComputeAcceleration(Add(Pp, Mul(k2p, 0.5 * dt_s)), v3, r_m, m_kg, rho, params.SpinRPM, omega, g);
			DVec3 k3p = v3;

			DVec3 v4 = Add(Pv, Mul(k3v, dt_s));
			DVec3 k4v = ComputeAcceleration(Add(Pp, Mul(k3p, dt_s)), v4, r_m, m_kg, rho, params.SpinRPM, omega, g);
			DVec3 k4p = v4;

			Pp = Add(Pp, Mul(Add(Add(k1p, Mul(k2p, 2.0)), Add(Mul(k3p, 2.0), k4p)), dt_s / 6.0));
			Pv = Add(Pv, Mul(Add(Add(k1v, Mul(k2v, 2.0)), Add(Mul(k3v, 2.0), k4v)), dt_s / 6.0));
		};

		while (steps < maxSteps)
		{
			extern std::unique_ptr<App> gApp;

			traveled_m = p.X;
			if (traveled_m >= gApp->GetPlateDistance())
			{
				break;
			}

			if (params.StopOnGroundHit && p.Y <= 0.0)
			{
				break;
			}

			StepRK4(p, v);

			outPoints.emplace_back(Float3{ static_cast<float>(p.X), static_cast<float>(p.Y), static_cast<float>(p.Z) });
			++steps;
		}
	}
}