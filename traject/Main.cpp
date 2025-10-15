#include "App.hpp"

extern std::unique_ptr<App> gApp;

//次やりたいこと
//設定ファイルで球速、RPM、軸、角度などを範囲指定乱数で生成可能に

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