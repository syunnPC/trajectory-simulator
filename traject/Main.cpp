#include "App.hpp"

extern std::unique_ptr<App> gApp;

//����肽������
//�ݒ�t�@�C���ŋ����ARPM�A���A�p�x�Ȃǂ�͈͎w�藐���Ő����\��

//�G���g���|�C���g
//�R���p�C�����ɂ��邳���̂ňꉞSAL�����Ă���
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int)
{
	gApp = std::make_unique<App>();
	if (!gApp->Initialize(hInstance))
	{
		return 0;
	}

	return gApp->Run();
}