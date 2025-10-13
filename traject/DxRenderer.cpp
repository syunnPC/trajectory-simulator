#include "DxRenderer.hpp"

#include <cassert>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
	const wchar_t* SHADER_FILE = L"shaders\\Line.hlsl";
}

DxRenderer::DxRenderer() : m_Viewport{}, m_Width(0), m_Height(0)
{}

bool DxRenderer::Initialize(HWND hwnd, std::uint32_t width, std::uint32_t height)
{
	if (!CreateDeviceAndSwap(hwnd, width, height))
	{
		return false;
	}

	if (!CreateRenderTargets(width, height))
	{
		return false;
	}

	if (!CreateShadersAndInputLayout())
	{
		return false;
	}

	if (!CreateConstantBuffers())
	{
		return false;
	}

	if (!CreateFixedStates())
	{
		return false;
	}

	if (!CreateTextResources())
	{
		return false;
	}

	if (!CreateTextTargetBitmap())
	{
		return false;
	}

	m_Width = width;
	m_Height = height;

	m_Viewport.TopLeftX = 0.0f;
	m_Viewport.TopLeftY = 0.0f;
	m_Viewport.Width = static_cast<float>(width);
	m_Viewport.Height = static_cast<float>(height);
	m_Viewport.MinDepth = 0.0f;
	m_Viewport.MaxDepth = 1.0f;

	return true;
}

bool DxRenderer::Resize(std::uint32_t width, std::uint32_t height)
{
	if (!m_Device || !m_SwapChain)
	{
		return false;
	}

	if (width == 0 || height == 0)
	{
		return true;
	}

	ID3D11RenderTargetView* nullRtv[1]{ nullptr };
	m_Context->OMSetRenderTargets(1, nullRtv, nullptr);

	if (m_D2DContext)
	{
		m_D2DContext->SetTarget(nullptr);
	}
	m_D2DTargetBitmap.Reset();

	ReleaseSizeDependentResources();

	HRESULT hr = m_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr))
	{
		return false;
	}

	if (!CreateRenderTargets(width, height))
	{
		return false;
	}

	if (m_D2DContext)
	{
		m_D2DTargetBitmap.Reset();
		if (!CreateTextTargetBitmap())
		{
			return false;
		}
	}

	m_Width = width;
	m_Height = height;

	m_Viewport.TopLeftX = 0.0f;
	m_Viewport.TopLeftY = 0.0f;
	m_Viewport.Width = static_cast<float>(width);
	m_Viewport.Height = static_cast<float>(height);
	m_Viewport.MinDepth = 0.0f;
	m_Viewport.MaxDepth = 1.0f;

	return true;
}

void DxRenderer::BeginFrame() noexcept
{
	if (!m_Rtv || !m_Dsv )
	{
		return;
	}

	float clear[4]{ 0.05f, 0.07f, 0.12f, 1.0f };

	m_Context->OMSetRenderTargets(1, m_Rtv.GetAddressOf(), m_Dsv.Get());
	m_Context->RSSetViewports(1, &m_Viewport);

	m_Context->ClearRenderTargetView(m_Rtv.Get(), clear);
	m_Context->ClearDepthStencilView(m_Dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void DxRenderer::EndFrame() noexcept
{
	m_SwapChain->Present(0, 0);
}

void DxRenderer::UpdateSceneCB(const CbScene& cb)
{
	D3D11_MAPPED_SUBRESOURCE ms{};
	HRESULT hr = m_Context->Map(m_CbScene.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
	if (SUCCEEDED(hr))
	{
		std::memcpy(ms.pData, &cb, sizeof(cb));
		m_Context->Unmap(m_CbScene.Get(), 0);
	}
}

void DxRenderer::UploadLineVertices(const std::vector<Vertex>& vertices)
{
	if (!m_Vb)
	{
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * std::max<std::size_t>(1, vertices.size()));
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT hr = m_Device->CreateBuffer(&bd, nullptr, m_Vb.GetAddressOf());
		if (FAILED(hr))
		{
			return;
		}
	}
	else
	{
		m_Vb.Reset();
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * std::max<std::size_t>(1, vertices.size()));
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT hr = m_Device->CreateBuffer(&bd, nullptr, m_Vb.GetAddressOf());
		if (FAILED(hr))
		{
			return;
		}
	}

	D3D11_MAPPED_SUBRESOURCE ms{};
	HRESULT hr = m_Context->Map(m_Vb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
	if (SUCCEEDED(hr))
	{
		std::memcpy(ms.pData, vertices.data(), sizeof(Vertex) * vertices.size());
		m_Context->Unmap(m_Vb.Get(), 0);
	}
}

void DxRenderer::DrawLineStrip(std::size_t vertexCount) noexcept
{
	if (!m_Vb)
	{
		return;
	}

	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	m_Context->IASetInputLayout(m_InputLayout.Get());
	m_Context->IASetVertexBuffers(0, 1, m_Vb.GetAddressOf(), &stride, &offset);
	m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

	m_Context->VSSetShader(m_Vs.Get(), nullptr, 0);
	m_Context->PSSetShader(m_Ps.Get(), nullptr, 0);
	m_Context->VSSetConstantBuffers(0, 1, m_CbScene.GetAddressOf());

	m_Context->Draw(static_cast<UINT>(vertexCount), 0);
}

void DxRenderer::UploadGroundVertices(const std::vector<Vertex>& vertices)
{
	if (!m_VbGround)
	{
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * std::max<std::size_t>(1, vertices.size()));
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT hr = m_Device->CreateBuffer(&bd, nullptr, m_VbGround.GetAddressOf());
		if (FAILED(hr))
		{
			return;
		}
	}
	else
	{
		m_VbGround.Reset();
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * std::max<std::size_t>(1, vertices.size()));
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT hr = m_Device->CreateBuffer(&bd, nullptr, m_VbGround.GetAddressOf());
		if (FAILED(hr))
		{
			return;
		}
	}

	D3D11_MAPPED_SUBRESOURCE ms{};
	HRESULT hr = m_Context->Map(m_VbGround.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
	if (SUCCEEDED(hr))
	{
		std::memcpy(ms.pData, vertices.data(), sizeof(Vertex) * vertices.size());
		m_Context->Unmap(m_VbGround.Get(), 0);
	}
}

void DxRenderer::DrawGroundLineList(std::size_t vertexCount) noexcept
{
	if (!m_VbGround)
	{
		return;
	}

	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	m_Context->IASetInputLayout(m_InputLayout.Get());
	m_Context->IASetVertexBuffers(0, 1, m_VbGround.GetAddressOf(), &stride, &offset);
	m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	m_Context->VSSetShader(m_Vs.Get(), nullptr, 0);
	m_Context->PSSetShader(m_Ps.Get(), nullptr, 0);
	m_Context->VSSetConstantBuffers(0, 1, m_CbScene.GetAddressOf());

	m_Context->Draw(static_cast<UINT>(vertexCount), 0);
}

std::uint32_t DxRenderer::GetWidth() const noexcept
{
	return m_Width;
}

std::uint32_t DxRenderer::GetHeight() const noexcept
{
	return m_Height;
}

bool DxRenderer::CreateDeviceAndSwap(HWND hwnd, std::uint32_t width, std::uint32_t height)
{
	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.SampleDesc.Count = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2;
	sd.OutputWindow = hwnd;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif //_DEBUG

	D3D_FEATURE_LEVEL fl;
	HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &sd, m_SwapChain.GetAddressOf(), m_Device.GetAddressOf(), &fl, m_Context.GetAddressOf());

	if (FAILED(hr))
	{
		hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &sd, m_SwapChain.GetAddressOf(), m_Device.GetAddressOf(), &fl, m_Context.GetAddressOf());
	}
	
	return SUCCEEDED(hr);
}

bool DxRenderer::CreateRenderTargets(std::uint32_t width, std::uint32_t height)
{
	ComPtr<ID3D11Texture2D> back;
	HRESULT hr = m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
	if (FAILED(hr))
	{
		return false;
	}

	hr = m_Device->CreateRenderTargetView(back.Get(), nullptr, m_Rtv.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC td{};
	td.Width = width;
	td.Height = height;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	td.SampleDesc.Count = 1;
	td.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	hr = m_Device->CreateTexture2D(&td, nullptr, m_DsTex.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	hr = m_Device->CreateDepthStencilView(m_DsTex.Get(), nullptr, m_Dsv.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

bool DxRenderer::CreateShadersAndInputLayout()
{
	ComPtr<ID3DBlob> bvs;
	ComPtr<ID3DBlob> bps;
	ComPtr<ID3DBlob> err;

	HRESULT hr = D3DCompileFromFile(SHADER_FILE, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, bvs.GetAddressOf(), err.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	hr = D3DCompileFromFile(SHADER_FILE, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, bps.GetAddressOf(), err.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	hr = m_Device->CreateVertexShader(bvs->GetBufferPointer(), bvs->GetBufferSize(), nullptr, m_Vs.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	hr = m_Device->CreatePixelShader(bps->GetBufferPointer(), bps->GetBufferSize(), nullptr, m_Ps.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC idec[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	hr = m_Device->CreateInputLayout(idec, 2, bvs->GetBufferPointer(), bvs->GetBufferSize(), m_InputLayout.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

bool DxRenderer::CreateConstantBuffers()
{
	D3D11_BUFFER_DESC cbd{};
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(CbScene);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = m_Device->CreateBuffer(&cbd, nullptr, m_CbScene.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

void DxRenderer::ReleaseSizeDependentResources() noexcept
{
	m_Rtv.Reset();
	m_DsTex.Reset();
	m_Dsv.Reset();
	m_D2DTargetBitmap.Reset();
}

bool DxRenderer::CreateTextResources()
{
	D2D1_FACTORY_OPTIONS fo{};
#ifdef _DEBUG
	fo.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(m_D2DFactory.GetAddressOf()));

	if (FAILED(hr))
	{
		return false;
	}

	ComPtr<IDXGIDevice> dxgiDevice;
	hr = m_Device.As(&dxgiDevice);
	if (FAILED(hr))
	{
		return false;
	}

	hr = m_D2DFactory->CreateDevice(dxgiDevice.Get(), m_D2DDevice.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	hr = m_D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, m_D2DContext.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	hr = m_D2DContext->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), m_D2DBrush.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(m_DWriteFactory.GetAddressOf()));
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

bool DxRenderer::CreateTextTargetBitmap()
{
	if (!m_SwapChain || !m_D2DContext)
	{
		return false;
	}

	ComPtr<IDXGISurface> surface;
	HRESULT hr = m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&surface));
	if (FAILED(hr))
	{
		return false;
	}

	const float dpi = 96.0f;
	D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);

	hr = m_D2DContext->CreateBitmapFromDxgiSurface(surface.Get(), &props, m_D2DTargetBitmap.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	m_D2DContext->SetTarget(m_D2DTargetBitmap.Get());
	return true;
}

void DxRenderer::BeginText() noexcept
{
	if (!m_D2DContext || !m_D2DTargetBitmap)
	{
		return;
	}

	m_D2DContext->BeginDraw();
	m_D2DContext->SetTransform(D2D1::Matrix3x2F::Identity());
}

void DxRenderer::DrawTextLabel(const std::wstring& text, float x_px, float y_px, float size_px, const D2D1_COLOR_F& color) noexcept
{
	if (!m_D2DContext || !m_DWriteFactory || !m_D2DBrush)
	{
		return;
	}

	ComPtr<IDWriteTextFormat> fmt;
	HRESULT hr = m_DWriteFactory->CreateTextFormat(m_DefaultFont.c_str(), nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size_px, L"ja-JP", fmt.GetAddressOf());
	if (FAILED(hr))
	{
		return;
	}

	fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

	m_D2DBrush->SetColor(color);

	D2D1_RECT_F rc = D2D1::RectF(x_px, y_px, x_px + 800.0f, y_px + size_px * 1.4f);

	D2D1_COLOR_F shadow = D2D1::ColorF{ 0, 0, 0, 0.65f };
	m_D2DBrush->SetColor(shadow);
	m_D2DContext->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), fmt.Get(), D2D1::RectF(rc.left + 1.0f, rc.top + 1.0f, rc.right + 1.0f, rc.bottom + 1.0f), m_D2DBrush.Get());

	m_D2DBrush->SetColor(color);
	m_D2DContext->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), fmt.Get(), rc, m_D2DBrush.Get());
}

void DxRenderer::EndText() noexcept
{
	if (!m_D2DContext)
	{
		return;
	}

	m_D2DContext->EndDraw();
}

void DxRenderer::UploadStrikeZoneVertices(const std::vector<Vertex>& vertices)
{
	if (!m_VbStrikeZone)
	{
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * std::max<std::size_t>(1, vertices.size()));
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT hr = m_Device->CreateBuffer(&bd, nullptr, m_VbStrikeZone.GetAddressOf());
		if (FAILED(hr))
		{
			return;
		}
	}
	else
	{
		m_VbStrikeZone.Reset();
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * std::max<std::size_t>(1, vertices.size()));
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT hr = m_Device->CreateBuffer(&bd, nullptr, m_VbStrikeZone.GetAddressOf());
		if (FAILED(hr))
		{
			return;
		}
	}

	D3D11_MAPPED_SUBRESOURCE ms{};
	HRESULT hr = m_Context->Map(m_VbStrikeZone.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
	if (SUCCEEDED(hr))
	{
		std::memcpy(ms.pData, vertices.data(), sizeof(Vertex) * vertices.size());
		m_Context->Unmap(m_VbStrikeZone.Get(), 0);
	}
}

void DxRenderer::DrawStrikeZoneLineList(std::size_t vertexCount) noexcept
{
	if (!m_VbStrikeZone)
	{
		return;
	}

	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	m_Context->IASetInputLayout(m_InputLayout.Get());
	m_Context->IASetVertexBuffers(0, 1, m_VbStrikeZone.GetAddressOf(), &stride, &offset);
	m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	m_Context->VSSetShader(m_Vs.Get(), nullptr, 0);
	m_Context->PSSetShader(m_Ps.Get(), nullptr, 0);
	m_Context->VSSetConstantBuffers(0, 1, m_CbScene.GetAddressOf());

	m_Context->Draw(static_cast<UINT>(vertexCount), 0);
}

bool DxRenderer::CreateFixedStates()
{
	D3D11_BLEND_DESC bd{};
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	HRESULT hr = m_Device->CreateBlendState(&bd, m_BlendAlpha.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	D3D11_RASTERIZER_DESC rs{};
	rs.FillMode = D3D11_FILL_SOLID;
	rs.CullMode = D3D11_CULL_NONE;
	rs.DepthClipEnable = TRUE;

	hr = m_Device->CreateRasterizerState(&rs, m_RsNoCull.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

void DxRenderer::UploadCircleVertices(const std::vector<Vertex>& vertices)
{
	if (!m_VbCircle)
	{
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * std::max<std::size_t>(1, vertices.size()));
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT hr = m_Device->CreateBuffer(&bd, nullptr, m_VbCircle.GetAddressOf());
		if (FAILED(hr))
		{
			return;
		}
	}
	else
	{
		m_VbCircle.Reset();

		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * std::max<std::size_t>(1, vertices.size()));
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT hr = m_Device->CreateBuffer(&bd, nullptr, m_VbCircle.GetAddressOf());
		if (FAILED(hr))
		{
			return;
		}
	}

	D3D11_MAPPED_SUBRESOURCE ms{};
	HRESULT hr = m_Context->Map(m_VbCircle.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
	if (SUCCEEDED(hr))
	{
		std::memcpy(ms.pData, vertices.data(), sizeof(Vertex) * vertices.size());
		m_Context->Unmap(m_VbCircle.Get(), 0);
	}
}

void DxRenderer::DrawCircleTriangles(std::size_t vertexCount) noexcept
{
	if (!m_VbCircle || vertexCount == 0)
	{
		return;
	}

	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	m_Context->OMSetBlendState(m_BlendAlpha.Get(), nullptr, 0xFFFFFFFF);
	m_Context->RSSetState(m_RsNoCull.Get());

	m_Context->IASetInputLayout(m_InputLayout.Get());
	m_Context->IASetVertexBuffers(0, 1, m_VbCircle.GetAddressOf(), &stride, &offset);
	m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_Context->VSSetShader(m_Vs.Get(), nullptr, 0);
	m_Context->PSSetShader(m_Ps.Get(), nullptr, 0);
	m_Context->VSSetConstantBuffers(0, 1, m_CbScene.GetAddressOf());

	m_Context->Draw(static_cast<UINT>(vertexCount), 0);

	m_Context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	m_Context->RSSetState(nullptr);
}