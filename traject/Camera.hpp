#pragma once

#include <cmath>
#include <cstdint>

#include <DirectXMath.h>

class OrbitCamera
{
public:
	OrbitCamera();
	OrbitCamera(const OrbitCamera&) = default;
	OrbitCamera(OrbitCamera&&) noexcept = default;
	~OrbitCamera() = default;
	OrbitCamera& operator=(const OrbitCamera&) = default;
	OrbitCamera& operator=(OrbitCamera&&) noexcept = default;

	void SetCenter(const DirectX::XMFLOAT3& center) noexcept;
	void SetRadiusLimits(float minRadius, float maxRadius) noexcept;
	void SetViewportSize(std::uint32_t width, std::uint32_t height) noexcept;
	void SetProjection(float fovYDeg, float nearZ, float farZ) noexcept;

	void BeginDrag(int mouseX, int mouseY) noexcept;
	void UpdateDrag(int mouseX, int mouseY) noexcept;
	void EndDrag() noexcept;
	void OnMouseWheel(int wheelDelta) noexcept;

	DirectX::XMMATRIX GetViewMatrix() const noexcept;
	DirectX::XMMATRIX GetProjMatrix() const noexcept;
	DirectX::XMMATRIX GetViewProjMatrix() const noexcept;

	DirectX::XMFLOAT3 GetEyePosition() const noexcept;

private:
	float m_YawRad;
	float m_PitchRad;
	float m_Radius;

	float m_MinRadius;
	float m_MaxRadius;

	float m_FovYDeg;
	float m_Aspect;
	float m_NearZ;
	float m_FarZ;

	DirectX::XMFLOAT3 m_Center;

	bool m_Dragging;
	int m_LastMouseX;
	int m_LastMouseY;

	float m_RotateSpeed;
	float m_ZoomScale;
};