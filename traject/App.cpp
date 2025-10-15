#include "App.hpp"

#include <thread>

#include "PitchConfig.hpp"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PitchSim;

std::unique_ptr<App> gApp;

void App::RestartAnimationForIndex(std::size_t i) noexcept
{
	if (i >= m_TrajectoryVertsList.size())
	{
		return;
	}

	if (IsPitchRequireRecalc(i))
	{
		RecalcTrajectForIndex(i);
	}

	const std::size_t n = m_TrajectoryVertsList[i].size();

	if (i < m_TimeElapsed_s.size())
	{
		m_TimeElapsed_s[i] = 0;
	}

	if (i < m_VisibleCounts.size())
	{
		m_VisibleCounts[i] = (n == 0 ? 0u : (n == 1 ? 1u : 2u));
	}

	m_Animate = true;
}

void App::RestartAnimationForAll() noexcept
{
	for (std::size_t i = 0; i < m_TrajectoryVertsList.size(); ++i)
	{
		RestartAnimationForIndex(i);
	}
}

namespace
{
	LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (gApp)
		{
			return gApp->HandleMessage(hwnd, msg, wParam, lParam);
		}

		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	const wchar_t* WINDOW_CLASS_NAME = L"DX11Window";

	inline XMFLOAT4 Lerp(const XMFLOAT4& a, const XMFLOAT4& b, float t) noexcept
	{
		float u = std::clamp(t, 0.0f, 1.0f);
		XMFLOAT4 r =
		{
			a.x + (b.x - a.x) * u,
			a.y + (b.y - a.y) * u,
			a.z + (b.z - a.z) * u,
			a.w + (b.w - a.w) * u
		};

		return r;
	}

	inline XMFLOAT4 Palette(std::size_t idx) noexcept
	{
		static const XMFLOAT4 k[] =
		{
			{0.90f, 0.30f, 0.30f, 1.0f}, //赤
			{0.30f, 0.85f, 0.40f, 1.0f}, //緑
			{0.30f, 0.50f, 0.95f, 1.0f}, //青
			{0.95f, 0.65f, 0.20f, 1.0f}, //オレンジ
			{0.80f, 0.40f, 0.85f, 1.0f}, //マゼンタ
			{0.95f, 0.85f, 0.25f, 1.0f}, //黄色
			{0.35f, 0.85f, 0.85f, 1.0f}, //シアン
			{0.80f, 0.55f, 0.35f, 1.0f}, //ブラウン
		};

		return k[idx % (sizeof(k) / sizeof(k[0]))];
	}

	inline double KmphToMps(double kmh) noexcept
	{
		return kmh / 3.6;
	}

	bool ProjectToScreen(const XMFLOAT3& world, const XMMATRIX& view, const XMMATRIX& proj, std::uint32_t width, std::uint32_t height, XMFLOAT2& out) noexcept
	{
		XMMATRIX vp = XMMatrixMultiply(view, proj);
		XMVECTOR p = XMVectorSet(world.x, world.y, world.z, 1.0f);
		XMVECTOR clip = XMVector4Transform(p, vp);

		float w = XMVectorGetW(clip);
		if (w <= 0.0f)
		{
			return false;
		}

		XMFLOAT4 cf{};
		XMStoreFloat4(&cf, clip);

		float ndcX = cf.x / cf.w;
		float ndcY = cf.y / cf.w;

		out.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(width);
		out.y = (-ndcY * 0.5f + 0.5f) * static_cast<float>(height);

		return true;
	}

	std::wstring Utf8ToWString(const std::string& s)
	{
		if (s.empty())
		{
			return std::wstring();
		}

		int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
		std::wstring out;
		out.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len);
		return out;
	}

	inline XMFLOAT3 ToF3(const Float3& p) noexcept
	{
		return XMFLOAT3{ p.X, p.Y, p.Z };
	}

	inline Float3 LerpF3(const Float3& a, const Float3& b, float t) noexcept
	{
		return Float3{ a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t, a.Z + (b.Z - a.Z) * t };
	}

	inline Float3 CatmullRom(const Float3& p0, const Float3& p1, const Float3& p2, const Float3& p3, float t) noexcept
	{
		const float t2 = t * t;
		const float t3 = t2 * t;

		float cx = 0.5f * (2.0f * p1.X + (-p0.X + p2.X) * t + (2.0f * p0.X - 5.0f * p1.X + 4.0f * p2.X - p3.X) * t2 + (-p0.X + 3.0f * p1.X - 3.0f * p2.X + p3.X) * t3);
		float cy = 0.5f * (2.0f * p1.Y + (-p0.Y + p2.Y) * t + (2.0f * p0.Y - 5.0f * p1.Y + 4.0f * p2.Y - p3.Y) * t2 + (-p0.Y + 3.0f * p1.Y - 3.0f * p2.Y + p3.Y) * t3);
		float cz = 0.5f * (2.0f * p1.Z + (-p0.Z + p2.Z) * t + (2.0f * p0.Z - 5.0f * p1.Z + 4.0f * p2.Z - p3.Z) * t2 + (-p0.Z + 3.0f * p1.Z - 3.0f * p2.Z + p3.Z) * t3);

		return Float3{ cx, cy, cz };
	}

	inline const Float3& ClampIdx(const std::vector<Float3>& v, int i) noexcept
	{
		if (i < 0)
		{
			return v.front();
		}

		if (i >= static_cast<int>(v.size()))
		{
			return v.back();
		}

		return v[static_cast<std::size_t>(i)];
	}
}

namespace
{
	inline bool SetRandomValue(PitchSim::Config::PitchEntry& pe)
	{
		if (pe.IsRandomAxisX && pe.XMin.has_value() && pe.XMax.has_value())
		{
			pe.Axis.X = App::GenerateRandom(pe.XMin.value(), pe.XMax.value());
		}
		else if (pe.IsRandomAxisX)
		{
			return false;
		}

		if (pe.IsRandomAxisY && pe.YMin.has_value() && pe.YMax.has_value())
		{
			pe.Axis.Y = App::GenerateRandom(pe.YMin.value(), pe.YMax.value());
		}
		else if (pe.IsRandomAxisY)
		{
			return false;
		}

		if (pe.IsRandomAxisZ && pe.ZMin.has_value() && pe.ZMax.has_value())
		{
			pe.Axis.Z = App::GenerateRandom(pe.ZMin.value(), pe.ZMax.value());
		}
		else if(pe.IsRandomAxisZ)
		{
			return false;
		}

		if (pe.IsRandomAzimuth && pe.AzimuthMin.has_value() && pe.AzimuthMax.has_value())
		{
			pe.Azimuth_deg = App::GenerateRandom(pe.AzimuthMin.value(), pe.AzimuthMax.value());
		}
		else if(pe.IsRandomAzimuth)
		{
			return false;
		}

		if (pe.IsRandomElevation && pe.ElevationMin.has_value() && pe.ElevationMax.has_value())
		{
			pe.Elevation_deg = App::GenerateRandom(pe.ElevationMin.value(), pe.ElevationMax.value());
		}
		else if (pe.IsRandomElevation)
		{
			return false;
		}

		if (pe.IsRandomRelease && pe.ReleaseMin.has_value() && pe.ReleaseMax.has_value())
		{
			pe.Release_cm = App::GenerateRandom(pe.ReleaseMin.value(), pe.ReleaseMax.value());
		}
		else if (pe.IsRandomRelease)
		{
			return false;
		}

		if (pe.IsRandomRpm && pe.RpmMin.has_value() && pe.RpmMax.has_value())
		{
			pe.Rpm = App::GenerateRandom(pe.RpmMin.value(), pe.RpmMax.value());
		}
		else if(pe.IsRandomRpm)
		{
			return false;
		}

		if (pe.IsRandomSpeed && pe.SpeedMin.has_value() && pe.SpeedMax.has_value())
		{
			pe.Speed_kmh = App::GenerateRandom(pe.SpeedMin.value(), pe.SpeedMax.value());
		}
		else if (pe.IsRandomSpeed)
		{
			return false;
		}
	}
}

bool App::IsPitchRequireRecalc(std::size_t i)
{
	if (i >= m_Pitches.size())
	{
		return false;
	}

	const auto& x = m_Pitches[i];

	return (x.IsRandomAxisX || x.IsRandomAxisY || x.IsRandomAxisZ || x.IsRandomAzimuth || x.IsRandomElevation || x.IsRandomRelease || x.IsRandomRpm || x.IsRandomSpeed);
}

void App::RecalcTrajectForIndex(std::size_t i)
{
	using namespace PitchSim;
	using namespace PitchSim::Config;

	if (i >= m_Pitches.size())
	{
		return;
	}

	PitchEntry& pe = m_Pitches[i];

	SetRandomValue(pe);

	SimParams p = m_Params;
	p.InitialSpeed_mps = KmphToMps(pe.Speed_kmh);
	p.SpinAxis = pe.Axis;
	p.SpinRPM = pe.Rpm;

	if (pe.Release_cm.has_value())
	{
		p.ReleaseHeight_cm = pe.Release_cm.value();
	}

	if (pe.Elevation_deg.has_value())
	{
		p.Elevation_deg = pe.Elevation_deg.value();
	}

	if (pe.Azimuth_deg.has_value())
	{
		p.Azimuth_deg = pe.Azimuth_deg.value();
	}

	std::vector<Float3> pts;
	m_Simulator.Simulate(p, pts);

	XMFLOAT4 base = Palette(static_cast<std::size_t>(i));
	XMFLOAT4 white{ 1.0f, 1.0f, 1.0f, 1.0f };

	std::vector<DxRenderer::Vertex> verts;
	verts.reserve(pts.size());
	const std::size_t n = pts.size();

	for (std::size_t k = 0; k < n; ++k)
	{
		float t = (n > 1) ? static_cast<float>(k) / static_cast<float>(n - 1) : 0.0f;
		XMFLOAT4 col = Lerp(base, white, t);
		verts.emplace_back(DxRenderer::Vertex{ XMFLOAT3{pts[k].X, pts[k].Y, pts[k].Z }, col });
	}

	const float plateX = PLATE_DISTANCE_M;
	const std::size_t vn = verts.size();
	std::optional<XMFLOAT3> hit;

	if (vn >= 2)
	{
		for (std::size_t k = 1; k < vn; ++k)
		{
			const auto& a = verts[k - 1].Pos;
			const auto& b = verts[k].Pos;

			if (a.x <= plateX && b.x >= plateX)
			{
				float t = (std::abs(b.x - a.x) > 1e-6f) ? ((plateX - a.x) / (b.x - a.x)) : 0.0f;
				t = std::clamp(t, 0.0f, 1.0f);
				hit = XMFLOAT3
				{
					plateX,
					a.y + (b.y - a.y) * t,
					a.z + (b.z - a.z) * t
				};

				break;
			}
		}
	}

	std::vector<DxRenderer::Vertex> circle;

	if (hit.has_value())
	{
		const float r = static_cast<float>(m_Params.Radius_mm * 1e-3);
		const int segs = 48;
		const XMFLOAT4 fillCol{ base.x, base.y, base.z, 0.35f };
		circle.reserve(segs * 3);

		auto addTri = [&](const XMFLOAT3& p0, const XMFLOAT3& p1, const XMFLOAT3& p2)
			{
				circle.emplace_back(DxRenderer::Vertex{ p0, fillCol });
				circle.emplace_back(DxRenderer::Vertex{ p1, fillCol });
				circle.emplace_back(DxRenderer::Vertex{ p2, fillCol });
			};

		const float cx = hit->x;
		const float cy = hit->y;
		const float cz = hit->z;

		constexpr float PI = 3.14159265358979323846f;

		for (int s = 0; s < segs; ++s)
		{
			float a0 = 2.0f * PI * (static_cast<float>(s) / segs);
			float a1 = 2.0f * PI * (static_cast<float>(s + 1) / segs);
			XMFLOAT3 p0{ cx, cy, cz };
			XMFLOAT3 p1{ cx, cy + r * std::cos(a0), cz + r * std::sin(a0) };
			XMFLOAT3 p2{ cx, cy + r * std::cos(a1), cz + r * std::sin(a1) };
			addTri(p0, p1, p2);
		}
	}

	const std::size_t ns = verts.size();
	double trajSec = (ns >= 2) ? (static_cast<double>(ns - 1) * p.Dt_s) : 0.0;

	m_TrajectoryVertsList[i] = std::move(verts);
	m_CircleVertsList[i] = std::move(circle);
	m_TimeElapsed_s[i] = 0.0;
	m_VisibleCounts[i] = (ns > 0 ? 1u : 0u);
	m_TrajDuration_s[i] = trajSec;
}

void App::ReloadConfigAndBuild()
{
	using namespace PitchSim;
	using namespace PitchSim::Config;

	m_Pitches.clear();

	/*
	if (!LoadPitchConfigFile("pitches.txt", m_Pitches, 8))
	{
		MessageBox(m_HWND, L"Failed to load pitches.txt.", L"Error", MB_OK | MB_ICONERROR);
		throw std::exception();
	}
	*/

	if (!LoadPitchConfigFileEx("pitches.txt", m_Pitches, 8))
	{
		MessageBox(m_HWND, L"LoadPitchConfigFileEx() Failed to load pitches.txt.", L"Error", MB_OK | MB_ICONERROR);
		throw std::exception();
	}

	std::size_t N = m_Pitches.size();

	m_TrajectoryVertsList.clear();
	m_VisibleCounts.clear();
	m_TimeElapsed_s.clear();
	m_TrajDuration_s.clear();
	m_CircleVertsList.clear();

	m_TrajectoryVertsList.resize(N);
	m_VisibleCounts.resize(N);
	m_TimeElapsed_s.resize(N);
	m_TrajDuration_s.resize(N);
	m_CircleVertsList.resize(N);

	//ループ並列化を有効に
	auto& x = *this;
#ifndef _DEBUG
#pragma	omp parallel for schedule(static) default(none) shared(x)
#endif
	for (int i = 0; i < static_cast<int>(N); ++i)
	{
		PitchEntry& pe = x.m_Pitches[i];

		SetRandomValue(pe);

		SimParams p = x.m_Params;
		p.InitialSpeed_mps = KmphToMps(pe.Speed_kmh);
		p.SpinAxis = pe.Axis;
		p.SpinRPM = pe.Rpm;

		if (pe.Release_cm.has_value())
		{
			p.ReleaseHeight_cm = pe.Release_cm.value();
		}

		if (pe.Elevation_deg.has_value())
		{
			p.Elevation_deg = pe.Elevation_deg.value();
		}

		if (pe.Azimuth_deg.has_value())
		{
			p.Azimuth_deg = pe.Azimuth_deg.value();
		}

		std::vector<Float3> pts;
		m_Simulator.Simulate(p, pts);

		XMFLOAT4 base = Palette(static_cast<std::size_t>(i));
		XMFLOAT4 white{ 1.0f, 1.0f, 1.0f, 1.0f };

		std::vector<DxRenderer::Vertex> verts;
		verts.reserve(pts.size());
		const std::size_t n = pts.size();

		for (std::size_t k = 0; k < n; ++k)
		{
			float t = (n > 1) ? static_cast<float>(k) / static_cast<float>(n - 1) : 0.0f;
			XMFLOAT4 col = Lerp(base, white, t);
			verts.emplace_back(DxRenderer::Vertex{ XMFLOAT3{pts[k].X, pts[k].Y, pts[k].Z }, col });
		}

		const float plateX = PLATE_DISTANCE_M;
		const std::size_t vn = verts.size();
		std::optional<XMFLOAT3> hit;

		if (vn >= 2)
		{
			for (std::size_t k = 1; k < vn; ++k)
			{
				const auto& a = verts[k - 1].Pos;
				const auto& b = verts[k].Pos;

				if (a.x <= plateX && b.x >= plateX)
				{
					float t = (std::abs(b.x - a.x) > 1e-6f) ? ((plateX - a.x) / (b.x - a.x)) : 0.0f;
					t = std::clamp(t, 0.0f, 1.0f);
					hit = XMFLOAT3
					{
						plateX,
						a.y + (b.y - a.y) * t,
						a.z + (b.z - a.z) * t
					};

					break;
				}
			}
		}

		std::vector<DxRenderer::Vertex> circle;

		if (hit.has_value())
		{
			const float r = static_cast<float>(x.m_Params.Radius_mm * 1e-3);
			const int segs = 48;
			const XMFLOAT4 fillCol{ base.x, base.y, base.z, 0.35f };
			circle.reserve(segs * 3);

			auto addTri = [&](const XMFLOAT3& p0, const XMFLOAT3& p1, const XMFLOAT3& p2)
			{
				circle.emplace_back(DxRenderer::Vertex{ p0, fillCol });
				circle.emplace_back(DxRenderer::Vertex{ p1, fillCol });
				circle.emplace_back(DxRenderer::Vertex{ p2, fillCol });
			};

			const float cx = hit->x;
			const float cy = hit->y;
			const float cz = hit->z;

			constexpr float PI = 3.14159265358979323846f;

			for (int s = 0; s < segs; ++s)
			{
				float a0 = 2.0f * PI * (static_cast<float>(s) / segs);
				float a1 = 2.0f * PI * (static_cast<float>(s + 1) / segs);
				XMFLOAT3 p0{ cx, cy, cz };
				XMFLOAT3 p1{ cx, cy + r * std::cos(a0), cz + r * std::sin(a0) };
				XMFLOAT3 p2{ cx, cy + r * std::cos(a1), cz + r * std::sin(a1) };
				addTri(p0, p1, p2);
			}
		}

		const std::size_t ns = verts.size();
		double trajSec = (ns >= 2) ? (static_cast<double>(ns - 1) * p.Dt_s) : 0.0;

		x.m_TrajectoryVertsList[i] = std::move(verts);
		x.m_CircleVertsList[i] = std::move(circle);
		x.m_TimeElapsed_s[i] = 0.0;
		x.m_VisibleCounts[i] = (ns > 0 ? 1u : 0u);
		x.m_TrajDuration_s[i] = trajSec;
	}

	m_Animate = true;
}

bool App::Initialize(HINSTANCE hInstance)
{
	WNDCLASSEX wc{ sizeof(WNDCLASSEX) };
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = WINDOW_CLASS_NAME;

	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	if (!RegisterClassEx(&wc))
	{
		return false;
	}

	UINT w = 1280;
	UINT h = 720;
	RECT rc{ 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	m_HWND = CreateWindow(WINDOW_CLASS_NAME, L"Pitch Trajectory - DirectX 11", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);

	if (!m_HWND)
	{
		return false;
	}

	ShowWindow(m_HWND, SW_SHOW);

	m_Params.ReleaseHeight_cm = 180.0;
	m_Params.InitialSpeed_mps = 0;
	m_Params.Elevation_deg = 0;
	m_Params.Azimuth_deg = 0.0;
	m_Params.SpinRPM = 0;
	m_Params.SpinAxis = DVec3{ 1.0, 0.0, 0.0 };
	m_Params.Radius_mm = 37.0;
	m_Params.Mass_kg = 0.145;
	m_Params.AirTemp_C = 25.0;
	m_Params.RelHumidity_pct = 60.0;
	m_Params.Pressure_hPa = 1013.25;
	m_Params.UseAltitudePressure = false;
	m_Params.Altitude_m = 0.0;
	m_Params.Dt_s = 0.0001;
	m_Params.StopOnGroundHit = true;

	m_StrikeZoneHeight_m = 0.45;
	m_StrikeZoneSizeHeight_m = 0.72;

	PitchSim::Config::EnvironmentSettings es{};

	if (PitchSim::Config::LoadEnvConfigFile("envconfig.txt", es))
	{
		if (es.Pressure_hPa.has_value())
		{
			m_Params.Pressure_hPa = es.Pressure_hPa.value();
		}

		if (es.UseHeightPressure.has_value())
		{
			m_Params.UseAltitudePressure = es.UseHeightPressure.value();
		}

		if (es.Height_m.has_value())
		{
			m_Params.Altitude_m = es.Height_m.value();
		}

		if (es.PitchSpeedScale.has_value())
		{
			m_TimeScale = es.PitchSpeedScale.value();
		}

		if (es.AirTemp_C.has_value())
		{
			m_Params.AirTemp_C = es.AirTemp_C.value();
		}

		if (es.Dt_s.has_value())
		{
			m_Params.Dt_s = es.Dt_s.value();
		}

		if (es.RelHumid_pct.has_value())
		{
			m_Params.RelHumidity_pct = es.RelHumid_pct.value();
		}

		if (es.Radius_mm.has_value())
		{
			m_Params.Radius_mm = es.Radius_mm.value();
		}

		if (es.Mass_kg.has_value())
		{
			m_Params.Mass_kg = es.Mass_kg.value();
		}
		
		if (es.MsaaCount.has_value())
		{
			m_Renderer.SetMSAACount(es.MsaaCount.value());
		}

		if (es.GraphicQuality.has_value())
		{
			m_Subdivide = es.GraphicQuality.value();
		}
	}

	if (!m_Renderer.Initialize(m_HWND, w, h))
	{
		return false;
	}

	m_Camera.SetViewportSize(w, h);
	m_Camera.SetProjection(60.0f, 0.01f, 500.0f);
	m_Camera.SetCenter(XMFLOAT3(9.22f, 0.0f, 0.0f));
	m_Camera.SetRadiusLimits(3.0f, 60.0f);
	
	BuildGroundGrid();
	try
	{
		ReloadConfigAndBuild();
	}
	catch (...)
	{
		return false;
	}
	BuildStrikeZone();

	Recompute();

	m_LastTick = std::chrono::steady_clock::now();

	return true;
}

void App::Recompute()
{
	std::vector<Float3> pts;
	m_Simulator.Simulate(m_Params, pts);

	std::vector<Float3> drawPts;
	if (pts.size() < 4 || m_Subdivide <= 1)
	{
		drawPts = pts;
	}
	else
	{
		drawPts.reserve(pts.size() * static_cast<std::size_t>(m_Subdivide));
		const int n = static_cast<int>(pts.size());
		for (int i = 0; i < n - 1; ++i)
		{
			const Float3& p0 = ClampIdx(pts, i - 1);
			const Float3& p1 = ClampIdx(pts, i);
			const Float3& p2 = ClampIdx(pts, i + 1);
			const Float3& p3 = ClampIdx(pts, i + 2);

			for (int s = 0; s < m_Subdivide; ++s)
			{
				float t = static_cast<float>(s) / static_cast<float>(m_Subdivide);
				drawPts.emplace_back(CatmullRom(p0, p1, p2, p3, t));
			}
		}

		drawPts.emplace_back(pts.back());
	}

	m_Vertices.clear();
	m_Vertices.reserve(drawPts.size());

	const std::size_t n = drawPts.size();
	for (std::size_t i = 0; i < n; ++i)
	{
		float t = (n > 1) ? static_cast<float>(i) / static_cast<float>(n - 1) : 0.0f;
		XMFLOAT4 col
		{
			0.1f * (1.0f - t) + 0.8f * t,
			0.9f * (1.0f - t) + 1.0f * t,
			1.0f,
			1.0f
		};

		m_Vertices.emplace_back(DxRenderer::Vertex{ XMFLOAT3{drawPts[i].X, drawPts[i].Y, drawPts[i].Z}, col });
	}

	m_Renderer.UploadLineVertices(m_Vertices);
}

void App::UpdateAnimation(double dt_s)
{
	if (!m_Animate)
	{
		return;
	}

	const double dtSim = dt_s * m_TimeScale;

	bool allDone = true;

	for (std::size_t i = 0; i < m_TrajectoryVertsList.size(); ++i)
	{
		const auto& verts = m_TrajectoryVertsList[i];
		const std::size_t n = verts.size();
		if (n <= 1)
		{
			m_VisibleCounts[i] = static_cast<std::size_t>(n);
			continue;
		}

		if (m_VisibleCounts[i] < n)
		{
			m_TimeElapsed_s[i] += dtSim;

			std::size_t count = static_cast<size_t>(m_TimeElapsed_s[i] / m_Params.Dt_s) + 1;

			if (count > n)
			{
				count = n;
			}

			if (count < 2)
			{
				count = 2;
			}

			m_VisibleCounts[i] = count;
		}

		if (m_VisibleCounts[i] < n)
		{
			allDone = false;
		}
	}

	if (allDone)
	{
		m_Animate = false;
	}
}

int App::Run()
{
	MSG msg{};

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			auto now = std::chrono::steady_clock::now();
			double dt_s = std::chrono::duration<double>(now - m_LastTick).count();
			m_LastTick = now;
			UpdateAnimation(dt_s);

			m_Renderer.BeginFrame();

			DxRenderer::CbScene cb{ XMMatrixTranspose(XMMatrixMultiply(m_Camera.GetViewMatrix(), m_Camera.GetProjMatrix())) };
			m_Renderer.UpdateSceneCB(cb);

			m_Renderer.DrawGroundLineList(m_GroundVerts.size());

			if (m_ShowStrikeZone)
			{
				m_Renderer.DrawStrikeZoneLineList(m_StrikeVerts.size());
			}

			for (std::size_t i = 0; i < m_TrajectoryVertsList.size(); ++i)
			{
				if (m_FilterSingle && !std::ranges::contains(m_FilterIndexList, i))
				{
					continue;
				}

				m_Renderer.UploadLineVertices(m_TrajectoryVertsList[i]);
				m_Renderer.DrawLineStrip(m_VisibleCounts[i]);

				const auto& verts = m_TrajectoryVertsList[i];
				bool finished = false;

				if (!m_Animate)
				{
					finished = true;
				}
				else if (i < m_VisibleCounts.size())
				{
					finished = (m_VisibleCounts[i] >= verts.size());
				}
				else
				{
					finished = true;
				}

				if (m_ShowBalls && finished && i < m_CircleVertsList.size() && !m_CircleVertsList[i].empty())
				{
					m_Renderer.UploadCircleVertices(m_CircleVertsList[i]);
					m_Renderer.DrawCircleTriangles(m_CircleVertsList[i].size());
				}
			}

			if (m_ShowLabels)
			{
				const auto view = m_Camera.GetViewMatrix();
				const auto proj = m_Camera.GetProjMatrix();

				m_Renderer.BeginText();

				for (std::size_t i = 0; i < m_TrajectoryVertsList.size(); ++i)
				{
					if (m_FilterSingle && !std::ranges::contains(m_FilterIndexList, i))
					{
						continue;
					}

					const auto& verts = m_TrajectoryVertsList[i];
					if (verts.empty())
					{
						continue;
					}

					bool finished = (!m_Animate);
					if (!finished)
					{
						if (i < m_VisibleCounts.size())
						{
							finished = (m_VisibleCounts[i] >= verts.size());
						}
						else
						{
							finished = true;
						}
					}
					if (!finished)
					{
						continue;
					}

					XMFLOAT3 endPos = verts.back().Pos;
					XMFLOAT2 sp{};

					if (!ProjectToScreen(endPos, view, proj, m_Renderer.GetWidth(), m_Renderer.GetHeight(), sp))
					{
						continue;
					}

					std::wstring label = (i < m_Pitches.size()) ? Utf8ToWString(m_Pitches[i].Label) : std::format(L"Pitch {}", i + 1);

					double speedKmh = (i < m_Pitches.size()) ? m_Pitches[i].Speed_kmh : m_Params.InitialSpeed_mps * 3.6;
					double rpm = (i < m_Pitches.size()) ? m_Pitches[i].Rpm : m_Params.SpinRPM;

					std::wstring text = std::format(L"{}: {} {:.0f} km/h {:.0f} RPM",i+1, label, speedKmh, rpm);

					D2D1_COLOR_F col = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.98f);

					sp.y -= i * 16.0f;
					m_Renderer.DrawTextLabel(text, sp.x + 8.0f, sp.y - 18.0f, 18.0f, col);
				}

				m_Renderer.EndText();
			}

			m_Renderer.EndFrame();
		}
	}

	return static_cast<int>(msg.wParam);
}

LRESULT App::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_SIZE:
		{
			if (wParam == SIZE_MINIMIZED)
			{
				return 0;
			}

			std::uint32_t w = static_cast<std::uint32_t>(LOWORD(lParam));
			std::uint32_t h = static_cast<std::uint32_t>(HIWORD(lParam));
			if (w == 0 || h == 0)
			{
				return 0;
			}

			m_Renderer.Resize(w, h);
			m_Camera.SetViewportSize(w, h);
			return 0;
		}

		case WM_LBUTTONDOWN:
		{
			int x = static_cast<int>(LOWORD(lParam));
			int y = static_cast<int>(HIWORD(lParam));
			m_MouseDown = true;
			SetCapture(hwnd);
			m_Camera.BeginDrag(x, y);
			return 0;
		}

		case WM_MOUSEMOVE:
		{
			if (m_MouseDown)
			{
				int x = static_cast<int>(LOWORD(lParam));
				int y = static_cast<int>(HIWORD(lParam));
				m_Camera.UpdateDrag(x, y);
			}

			return 0;
		}

		case WM_LBUTTONUP:
		{
			m_MouseDown = false;
			ReleaseCapture();
			m_Camera.EndDrag();
			return 0;
		}

		case WM_MOUSEWHEEL:
		{
			int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			m_Camera.OnMouseWheel(delta);
			return 0;
		}

		case WM_KEYDOWN:
		{
			if (wParam == VK_ESCAPE)
			{
				DestroyWindow(hwnd);
				return 0;
			}
			else if (wParam == VK_SPACE)
			{
				if (m_FilterSingle)
				{
					for (std::size_t i : m_FilterIndexList)
					{
						RestartAnimationForIndex(i);
					}
				}
				else
				{
					RestartAnimationForAll();
				}
				return 0;
			}
			else if (wParam == 'H')
			{
				m_ShowLabels = !m_ShowLabels;
				return 0;
			}
			else if (wParam == 'C')
			{
				m_ShowBalls = !m_ShowBalls;
				return 0;
			}
			else if (wParam >= '1' && wParam <= '8')
			{
				int idx = static_cast<int>(wParam - '1');
				if (idx >= 0 && idx < static_cast<int>(m_TrajectoryVertsList.size()))
				{
					m_FilterSingle = true;
					m_FilterIndex = idx;

					if (!std::ranges::contains(m_FilterIndexList, idx))
					{
						m_FilterIndexList.emplace_back(idx);
					}
					else
					{
						m_FilterIndexList.erase(std::remove(m_FilterIndexList.begin(), m_FilterIndexList.end(), idx));
					}

					for (auto i : m_FilterIndexList)
					{
						RestartAnimationForIndex(i);
					}
				}

				return 0;
			}
			else if (wParam == '0')
			{
				m_FilterSingle = false;
				m_FilterIndex = -1;

				m_FilterIndexList.clear();

				RestartAnimationForAll();

				return 0;
			}
			else if (wParam == '9')
			{
				m_FilterSingle = true;
				m_FilterIndexList.clear();
				RestartAnimationForAll();
				return 0;
			}
			else if (wParam == 'Z')
			{
				m_ShowStrikeZone = !m_ShowStrikeZone;
				return 0;
			}
			else
			{
				return 0;
			}
		}

		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
		default:
			break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void App::BuildGroundGrid()
{
	m_GroundVerts.clear();

	float xMin = -1.0f;
	float xMax = 20.0f;
	float zMin = -6.0f;
	float zMax = 6.0f;
	float step = 1.0f;

	XMFLOAT4 baseCol{ 0.22f, 0.25f, 0.28f, 1.0f };
	XMFLOAT4 axisCol{ 0.35f, 0.38f, 0.42f, 1.0f };

	for (float x = xMin; x <= xMax + 1e-4f; x += step)
	{
		bool axis = std::abs(x) < 1e-4f;
		XMFLOAT4 col = axis ? axisCol : baseCol;
		m_GroundVerts.emplace_back(DxRenderer::Vertex{ XMFLOAT3(x, 0.0f, zMin), col });
		m_GroundVerts.emplace_back(DxRenderer::Vertex{ XMFLOAT3(x, 0.0f, zMax), col });
	}

	for (float z = zMin; z <= zMax + 1e-4f; z += step)
	{
		bool axis = std::abs(z) < 1e-4f;
		XMFLOAT4 col = axis ? axisCol : baseCol;
		m_GroundVerts.emplace_back(DxRenderer::Vertex{ XMFLOAT3(xMin, 0.0f, z), col });
		m_GroundVerts.emplace_back(DxRenderer::Vertex{ XMFLOAT3(xMax, 0.0f, z), col });
	}

	m_Renderer.UploadGroundVertices(m_GroundVerts);
}

void App::BuildStrikeZone()
{
	m_StrikeVerts.clear();

	const float x = 18.44f;
	const float halfW = 0.216f;
	const float y0 = static_cast<const float>(m_StrikeZoneHeight_m);
	const float y1 = y0 + static_cast<const float>(m_StrikeZoneSizeHeight_m);

	const float zL = -halfW;
	const float zR = +halfW;

	const XMFLOAT4 frameCol{ 0.95f, 0.35f, 0.10f, 1.0f };
	const XMFLOAT4 meshCol{ 0.95f, 0.60f, 0.30f, 1.0f };

	auto addLine = [&](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT4& c)
	{
		m_StrikeVerts.emplace_back(DxRenderer::Vertex{ a,c });
		m_StrikeVerts.emplace_back(DxRenderer::Vertex{ b,c });
	};

	addLine(XMFLOAT3{ x, y0, zL }, XMFLOAT3{ x, y0, zR }, frameCol);
	addLine(XMFLOAT3{ x, y1, zL }, XMFLOAT3{ x, y1, zR }, frameCol);
	addLine(XMFLOAT3{ x, y0, zL }, XMFLOAT3{ x, y1, zL }, frameCol);
	addLine(XMFLOAT3{ x, y0, zR }, XMFLOAT3{ x, y1, zR }, frameCol);

	const float zV1 = -halfW / 3.0f;
	const float zV2 = +halfW / 3.0f;
	addLine(XMFLOAT3{ x, y0, zV1 }, XMFLOAT3{ x, y1, zV1 }, meshCol);
	addLine(XMFLOAT3{ x, y0, zV2 }, XMFLOAT3{ x, y1, zV2 }, meshCol);

	const float yH1 = y0 + static_cast<const float>(m_StrikeZoneSizeHeight_m) / 3.0f;
	const float yH2 = y0 + 2.0f * static_cast<const float>(m_StrikeZoneSizeHeight_m) / 3.0f;
	addLine(XMFLOAT3{ x, yH1, zL }, XMFLOAT3{ x, yH1, zR }, meshCol);
	addLine(XMFLOAT3{ x, yH2, zL }, XMFLOAT3{ x, yH2, zR }, meshCol);

	m_Renderer.UploadStrikeZoneVertices(m_StrikeVerts);
}

double App::GenerateRandom(double min, double max)
{
	thread_local std::mt19937_64 gen{ static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())) ^ static_cast<uint64_t>(__rdtsc())};
	std::uniform_real_distribution<double> dist{ min, max };
	return dist(gen);
}