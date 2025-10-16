#include "Camera.hpp"

#include <algorithm>

using namespace DirectX;

namespace
{
	inline float ClampFloat(float v, float lo, float hi) noexcept
	{
		if (v < lo)
		{
			return lo;
		}
		else if (v > hi)
		{
			return hi;
		}
		else
		{
			return v;
		}
	}
}

OrbitCamera::OrbitCamera()
	:m_YawRad(0.0f), m_PitchRad(0.5f), m_Radius(12.0f), m_MinRadius(3.0f), m_MaxRadius(60.0f), m_FovYDeg(60.0f), m_Aspect(16.0f / 9.0f), m_NearZ(0.01f), m_FarZ(500.0f), m_Center(9.22f, 0.0f, 0.0f), m_Dragging(false),
	m_LastMouseX(0), m_LastMouseY(0), m_RotateSpeed(0.005f), m_ZoomScale(1.1f)
{}

void OrbitCamera::SetCenter(const XMFLOAT3& center) noexcept
{
	m_Center = center;
}

void OrbitCamera::SetRadiusLimits(float minRadius, float maxRadius) noexcept
{
	m_MinRadius = std::max(0.01f, minRadius);
	m_MaxRadius = std::max(m_MinRadius, maxRadius);
	m_Radius = ClampFloat(m_Radius, m_MinRadius, m_MaxRadius);
}

void OrbitCamera::SetViewportSize(std::uint32_t width, std::uint32_t height) noexcept
{
	if (height == 0)
	{
		m_Aspect = 1.0f;
	}
	else
	{
		m_Aspect = static_cast<float>(width) / static_cast<float>(height);
	}
}

void OrbitCamera::SetProjection(float fovYDeg, float nearZ, float farZ) noexcept
{
	m_FovYDeg = fovYDeg;
	m_NearZ = nearZ;
	m_FarZ = farZ;
}

void OrbitCamera::BeginDrag(int mouseX, int mouseY) noexcept
{
	m_Dragging = true;
	m_LastMouseX = mouseX;
	m_LastMouseY = mouseY;
}

void OrbitCamera::UpdateDrag(int mouseX, int mouseY) noexcept
{
	if (!m_Dragging)
	{
		return;
	}

	int dx = mouseX - m_LastMouseX;
	int dy = mouseY - m_LastMouseY;

	m_LastMouseX = mouseX;
	m_LastMouseY = mouseY;

	m_YawRad += static_cast<float>(dx) * m_RotateSpeed;
	m_PitchRad += static_cast<float>(-dy) * m_RotateSpeed;

	const float eps = 0.001f;
	const float halfPi = 1.57079632679f;

	m_PitchRad = ClampFloat(m_PitchRad, eps, halfPi - eps);
}

void OrbitCamera::EndDrag() noexcept
{
	m_Dragging = false;
}

void OrbitCamera::OnMouseWheel(int wheelDelta) noexcept
{
	int steps = wheelDelta / 120;

	for (int i = 0; i < std::abs(steps); ++i)
	{
		if (steps > 0)
		{
			m_Radius /= m_ZoomScale;
		}
		else
		{
			m_Radius *= m_ZoomScale;
		}
	}

	m_Radius = ClampFloat(m_Radius, m_MinRadius, m_MaxRadius);
}

XMMATRIX OrbitCamera::GetViewMatrix() const noexcept
{
	XMFLOAT3 eyeF = GetEyePosition();
	XMVECTOR eye = XMLoadFloat3(&eyeF);
	XMVECTOR at = XMLoadFloat3(&m_Center);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
	return view;
}

XMMATRIX OrbitCamera::GetProjMatrix() const noexcept
{
	float fovY = m_FovYDeg * (3.14159265358979323846f / 180.0f);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(fovY, m_Aspect, m_NearZ, m_FarZ);
	return proj;
}

XMMATRIX OrbitCamera::GetViewProjMatrix() const noexcept
{
	XMMATRIX vp = XMMatrixMultiply(GetViewMatrix(), GetProjMatrix());
	return vp;
}

XMFLOAT3 OrbitCamera::GetEyePosition() const noexcept
{
	float cy = std::cos(m_YawRad);
	float sy = std::sin(m_YawRad);
	float ce = std::cos(m_PitchRad);
	float se = std::sin(m_PitchRad);

	XMFLOAT3 eye
	{
		m_Center.x + m_Radius * (ce * cy),
		m_Center.y + m_Radius * (se),
		m_Center.z + m_Radius * (ce * sy)
	};
	
	return eye;
}

XMFLOAT3 OrbitCamera::GetCenter() const noexcept
{
	return m_Center;
}