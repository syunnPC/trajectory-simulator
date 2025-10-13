#pragma once

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <format>
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

class App
{
public:
	App() = default;
	App(const App&) = delete;
	~App() = default;

	App& operator=(const App&) = delete;

	bool Initialize(HINSTANCE hInstance);
	int Run();

	LRESULT HandleMessage(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);

private:
	void Recompute();
	void BuildGroundGrid();
	void UpdateAnimation(double dt_s);
	void ReloadConfigAndBuild();
	void RestartAnimationForIndex(std::size_t i) noexcept;
	void RestartAnimationForAll() noexcept;

	HWND m_HWND{ nullptr };
	DxRenderer m_Renderer;
	OrbitCamera m_Camera;
	PitchSim::TrajectorySimulator m_Simulator;

	std::vector<DxRenderer::Vertex> m_Vertices;
	std::vector<DxRenderer::Vertex> m_GroundVerts;

	std::vector<std::vector<DxRenderer::Vertex>> m_TrajectoryVertsList;
	std::vector<std::vector<float>> m_ArcLenList_m; //いらない
	std::vector<std::size_t> m_VisibleCounts;

	std::vector<PitchSim::Config::PitchEntry> m_Pitches;

	PitchSim::SimParams m_Params;

	bool m_MouseDown{ false };

	bool m_ShowLabels{ true };

	bool m_FilterSingle{ false };

	std::vector<std::size_t> m_FilterIndexList;

	int m_FilterIndex{ -1 };

	float m_DrawSpeed_mps{ 4.0f }; //いらない
	float m_DrawLength_m{ 0.0f }; //いらない
	std::size_t m_VisibleCount{ 0 };
	bool m_Animate{ true };
	std::chrono::steady_clock::time_point m_LastTick;

	std::vector<double> m_TimeElapsed_s;
	std::vector<double> m_TrajDuration_s;

	/*
		シュミレーション時間幅を変更したい場合
		m_TimeScaleは実際の速度の何倍かを示すので
		デフォルト1.0/3.0→実際の速度の1/3としてレンダ
	*/

	double m_TimeScale{ 1.0 / 3.0 };
};