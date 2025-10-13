#include "App.hpp"

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
}

void App::ReloadConfigAndBuild()
{
	using namespace PitchSim;
	using namespace PitchSim::Config;

	m_Pitches.clear();

	if (!LoadPitchConfigFile("pitches.txt", m_Pitches, 8))
	{
		MessageBox(m_HWND, L"Failed to load pitches.txt.", L"Error", MB_OK | MB_ICONERROR);
		throw std::exception();
	}

	m_TrajectoryVertsList.clear();
	m_ArcLenList_m.clear();
	m_VisibleCounts.clear();
	m_TimeElapsed_s.clear();
	m_TrajDuration_s.clear();

	for (std::size_t i = 0; i < m_Pitches.size(); ++i)
	{
		const PitchEntry& pe = m_Pitches[i];

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

		XMFLOAT4 base = Palette(i);
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

		std::vector<float> arc;
		arc.reserve(verts.size());
		arc.emplace_back(0.0f);
		for (std::size_t k = 1; k < verts.size(); ++k)
		{
			const auto& a = verts[k - 1].Pos;
			const auto& b = verts[k].Pos;

			float dx = b.x - a.x;
			float dy = b.y - a.y;
			float dz = b.z - a.z;

			float seg = std::sqrt(dx * dx + dy * dy + dz * dz);
			arc.emplace_back(arc.back() + seg);
		}

		m_TrajectoryVertsList.emplace_back(std::move(verts));
		const std::size_t ns = m_TrajectoryVertsList.back().size();

		double trajSec = 0.0;
		if (ns >= 2)
		{
			trajSec = static_cast<double>(ns - 1) * m_Params.Dt_s;
		}

		m_TrajDuration_s.emplace_back(trajSec);

		m_TimeElapsed_s.emplace_back(0.0);

		m_VisibleCounts.emplace_back(ns > 0 ? 1u : 0u);
	}

	m_DrawLength_m = 0.0f;
	m_Animate = true;
}

bool App::Initialize(HINSTANCE hInstance)
{
	WNDCLASSEX wc{ sizeof(WNDCLASSEX) };
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = WINDOW_CLASS_NAME;

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

	if (!m_Renderer.Initialize(m_HWND, w, h))
	{
		return false;
	}

	m_Camera.SetViewportSize(w, h);
	m_Camera.SetProjection(60.0f, 0.01f, 500.0f);
	m_Camera.SetCenter(XMFLOAT3(9.22f, 0.0f, 0.0f));
	m_Camera.SetRadiusLimits(3.0f, 60.0f);

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
	}
	
	BuildGroundGrid();
	try
	{
		ReloadConfigAndBuild();
	}
	catch (...)
	{
		return false;
	}

	Recompute();

	m_LastTick = std::chrono::steady_clock::now();

	return true;
}

void App::Recompute()
{
	ReloadConfigAndBuild();
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

			for (std::size_t i = 0; i < m_TrajectoryVertsList.size(); ++i)
			{
				if (m_FilterSingle && static_cast<int>(i) != m_FilterIndex)
				{
					//continue;
				}

				if (m_FilterSingle && !std::ranges::contains(m_FilterIndexList, i))
				{
					continue;
				}

				m_Renderer.UploadLineVertices(m_TrajectoryVertsList[i]);
				m_Renderer.DrawLineStrip(m_VisibleCounts[i]);
			}

			if (m_ShowLabels)
			{
				const auto view = m_Camera.GetViewMatrix();
				const auto proj = m_Camera.GetProjMatrix();

				m_Renderer.BeginText();

				for (std::size_t i = 0; i < m_TrajectoryVertsList.size(); ++i)
				{
					if (m_FilterSingle && static_cast<int>(i) != m_FilterIndex)
					{
						//continue;
					}

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