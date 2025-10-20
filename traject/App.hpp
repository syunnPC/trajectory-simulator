#pragma once

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <format>
#include <random>
#include <cmath>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <DirectXMath.h>

#include "DxRenderer.hpp"
#include "Camera.hpp"
#include "TrajectorySimulator.hpp"
#include "PitchConfig.hpp"
#include "Physics.hpp"

struct AppParam
{
	AppParam(std::wstring key, std::wstring value) : ParamValue{ value }, ParamKey{ key } {}
	std::wstring ParamKey;
	std::wstring ParamValue;
};

std::optional<std::wstring> GetValueForKey(std::wstring key, const std::vector<AppParam>& p);

class App
{
public:
	App() = default;
	App(const App&) = delete;
	~App() = default;

	App& operator=(const App&) = delete;

	bool Initialize(HINSTANCE hInstance, std::vector<AppParam>& param);
	int Run(std::vector<AppParam>& param);

	LRESULT HandleMessage(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);

	static double GenerateRandom(double min, double max);

	inline double GetPlateDistance()
	{
		return m_PlateDistance_m;
	}

private:
	void Recompute();
	void BuildGroundGrid();
	void UpdateAnimation(double dt_s);
	void ReloadConfigAndBuild();
	void RestartAnimationForIndex(std::size_t i) noexcept;
	void RestartAnimationForAll() noexcept;
	void BuildStrikeZone();
	void RecalcTrajectForIndex(std::size_t i);
	bool IsPitchRequireRecalc(std::size_t i);
	void RestartAnimationForIndexWithoutRecompute(std::size_t i) noexcept;
	void RestartAnimationForAllWithoutRecompute() noexcept;

	void RebuildPackedVBs();

	std::wstring m_EnvConfigFilePath{ L"envconfig.txt" };
	std::wstring m_PitchConfigFilePath{ L"pitches.txt" };

	HWND m_HWND{ nullptr };
	DxRenderer m_Renderer;
	OrbitCamera m_Camera;
	PitchSim::TrajectorySimulator m_Simulator;
	std::vector<DxRenderer::Vertex> m_Vertices;
	std::vector<DxRenderer::Vertex> m_GroundVerts;
	std::vector<std::size_t> m_VisibleCounts;
	std::vector<PitchSim::Config::PitchEntry> m_Pitches;
	std::vector<DxRenderer::Vertex> m_StrikeVerts;
	std::vector<std::vector<DxRenderer::Vertex>> m_CircleVertsList;

	bool m_ShowStrikeZone{ true };
	PitchSim::SimParams m_Params;
	bool m_MouseDown{ false };
	bool m_ShowLabels{ true };
	bool m_FilterSingle{ false };
	bool m_ShowBalls{ true };
	bool m_ShowDetail{ false };
	double m_StrikeZoneHeight_m;
	double m_StrikeZoneSizeHeight_m;
	std::vector<std::size_t> m_FilterIndexList;
	int m_Subdivide{ 8 };
	double m_PlateDistance_m{ 18.44 };

	int m_PrevX{ 1280 };
	int m_PrevY{ 720 };

	bool m_Animate{ true };
	std::chrono::steady_clock::time_point m_LastTick;

	std::vector<double> m_TimeElapsed_s;
	std::vector<double> m_TrajDuration_s;

	std::vector<std::vector<DxRenderer::Vertex>> m_TrajectoryVertsList;

	/*
		シュミレーション時間幅を変更したい場合
		m_TimeScaleは実際の速度の何倍かを示すので
		デフォルト1.0/3.0→実際の速度の1/3としてレンダ
	*/
	double m_TimeScale{ 1.0 / 3.0 };

	bool m_PackedDirty{ false };
};