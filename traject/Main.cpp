#include "App.hpp"

extern std::unique_ptr<App> gApp;

//エントリポイント
//コンパイル時にうるさいので一応SALをつけておく
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int)
{
	gApp = std::make_unique<App>();
	if (!gApp->Initialize(hInstance))
	{
		return 0;
	}

	return gApp->Run();
}