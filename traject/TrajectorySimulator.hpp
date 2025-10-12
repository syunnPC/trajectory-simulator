#pragma once

#include <vector>
#include <cstddef>

#include "Physics.hpp"

namespace PitchSim
{
	class TrajectorySimulator
	{
	public:
		TrajectorySimulator() = default;
		TrajectorySimulator(const TrajectorySimulator&) = default;
		TrajectorySimulator(TrajectorySimulator&&) noexcept = default;
		~TrajectorySimulator() = default;
		TrajectorySimulator& operator=(const TrajectorySimulator&) = default;
		TrajectorySimulator& operator=(TrajectorySimulator&&) noexcept = default;

		void Simulate(const SimParams& params, std::vector<Float3>& outPoints);

	private:
		struct RK4State
		{
			DVec3 P;
			DVec3 V;
		};

		static DVec3 ComputeAcceleration(const DVec3& position, const DVec3& velocity, double radius_m, double mass_kg, double rho, double spin_rpm, const DVec3& omega, double g) noexcept;
	};
}