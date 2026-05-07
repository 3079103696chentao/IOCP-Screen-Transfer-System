#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "EdoyunServer.h"
#include "EdoyunTool.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CWinApp theApp;

int main(int argc, char* argv[]) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    if (!CEdoyunTool::Init()) {
        return 1;
    }

    EdoyunServer server;
    if (!server.StartService()) {
        MessageBox(NULL, _T("IOCP 服务端启动失败"), _T("错误"), MB_OK | MB_ICONERROR);
        return 1;
    }

    getchar();
    server.StopService();
    return 0;
}
