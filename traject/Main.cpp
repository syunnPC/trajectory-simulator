#include "App.hpp"

#include <fstream>

#include <shellapi.h>

extern std::unique_ptr<App> gApp;

namespace
{
	std::vector<AppParam> ParseCommandLineArgs(LPWSTR cmdLine)
	{
		int argc;
		LPWSTR* argv = CommandLineToArgvW(cmdLine, &argc);
		std::vector<AppParam> r{};

		for (std::size_t i = 0; i < argc; ++i)
		{
			assert(argv[i] != nullptr);

			LPWSTR key = argv[i];

			if (std::wcslen(key) > 0 && key[0] == '-' || key[0] == '/')
			{
				key += 1;
			}
			if (i + 1 == argc)
			{
				r.emplace_back(AppParam{ L"", key});
				break;
			}
			LPWSTR value = argv[++i];
			r.emplace_back(AppParam{ key, value });
		}

		LocalFree(argv);
		return r;
	}
}

//エントリポイント
//コンパイル時にうるさいので一応SALをつけておく
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int)
{
	gApp = std::make_unique<App>();

	auto param = ParseCommandLineArgs(lpCmdLine);

	if (!gApp->Initialize(hInstance, param))
	{
		return 0;
	}

	return gApp->Run(param);
}