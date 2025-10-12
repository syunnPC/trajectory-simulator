#pragma once

#include <string>
#include <vector>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXMath.h>

class DxRenderer
{
public:
	struct Vertex
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT4 Col;
	};

	struct CbScene
	{
		DirectX::XMMATRIX Mvp;
	};

	DxRenderer();
	DxRenderer(const DxRenderer&) = delete;
	DxRenderer(DxRenderer&&) noexcept = default;
	~DxRenderer() = default;
	DxRenderer& operator=(const DxRenderer&) = delete;
	DxRenderer& operator=(DxRenderer&&) noexcept = default;

	bool Initialize(HWND hwnd, std::uint32_t width, std::uint32_t height);
	bool Resize(std::uint32_t width, std::uint32_t height);
	void BeginFrame() noexcept;
	void EndFrame() noexcept;
	void UpdateSceneCB(const CbScene& cb);
	void UploadLineVertices(const std::vector<Vertex>& vertices);
	void DrawLineStrip(std::size_t vertexCount) noexcept;
	void UploadGroundVertices(const std::vector<Vertex>& vertices);
	void DrawGroundLineList(std::size_t vertexCount) noexcept;

	void BeginText() noexcept;
	void DrawTextLabel(const std::wstring& text, float x_px, float y_px, float size_px, const D2D1_COLOR_F& color) noexcept;
	void EndText() noexcept;

	std::uint32_t GetWidth() const noexcept;
	std::uint32_t GetHeight() const noexcept;

private:
	bool CreateDeviceAndSwap(HWND hwnd, std::uint32_t width, std::uint32_t height);
	bool CreateRenderTargets(std::uint32_t width, std::uint32_t height);
	bool CreateShadersAndInputLayout();
	bool CreateConstantBuffers();

	void ReleaseSizeDependentResources() noexcept;

	bool CreateTextResources();
	bool CreateTextTargetBitmap();

	Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_Context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_Rtv;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_DsTex;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_Dsv;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Vs;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Ps;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_CbScene;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_Vb;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_VbGround;

	Microsoft::WRL::ComPtr<ID2D1Factory1> m_D2DFactory;
	Microsoft::WRL::ComPtr<ID2D1Device> m_D2DDevice;
	Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_D2DContext;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_D2DBrush;
	Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_D2DTargetBitmap;
	Microsoft::WRL::ComPtr<IDWriteFactory> m_DWriteFactory;

	std::wstring m_DefaultFont{ L"Segoe UI" };

	D3D11_VIEWPORT m_Viewport;

	std::uint32_t m_Width;
	std::uint32_t m_Height;
};