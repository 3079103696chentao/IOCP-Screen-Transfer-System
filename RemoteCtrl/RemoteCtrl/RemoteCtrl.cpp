// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。


#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "command.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 唯一的应用程序对象

CWinApp theApp;

using namespace std;

//开机启动的时候，程序的权限是根素启动用户的
//如果两个权限不一致，则会导致程序启动失败
//开机启动对环境变量有影响，如果依赖dll，可能会启动失败，需要把这些dll复制到system32或者syswow64（多是32位程序）下面
//system32下面多是64位程序

void WriteRegisterTable() {
    char sSys[MAX_PATH] = "";
    GetSystemDirectoryA(sSys, sizeof(sSys));
    string strExe = "\\RemoteCtrl.exe";

    char sPath[MAX_PATH] = "";
    GetCurrentDirectoryA(MAX_PATH, sPath);
    std::string strcmd = "mklink " + string(sSys) + strExe + " " + string(sPath) + "\\RemoteCtrl.exe";
    system(strcmd.c_str());
    
    HKEY hKey = NULL;

    CString strSubkey = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";//如果不用双斜杠，“\”是转义字符
    int ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSubkey, 0, KEY_WRITE, &hKey);
    if (ret != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        MessageBox(NULL, _T("设置自动开机失败！是否权限不足？ \r\n程序启动失败！"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
        exit(0);
    }
    CString strPath = CString(_T("%windir%\\system32\\RemoteCtrl.exe"));
    ret = RegSetValueEx(hKey, _T("RemoteCtrl"), 0, REG_EXPAND_SZ, (BYTE*)(LPCTSTR)strPath, strPath.GetLength());
    if (ret != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        MessageBox(NULL, _T("设置自动开机失败！是否权限不足？ \r\n程序启动失败！"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
        exit(0);
    }
}

void WriteStartupDir(CString strPath) {
    //CString strPath = _T("C:\\Users\\weima\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl.exe");
    // 获取当前进程的完整路径（不含命令行参数）
    TCHAR szExePath[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, szExePath, MAX_PATH);
    CString strCmd = szExePath; // 这才是正确的源文件路径

    BOOL ret = CopyFile(strCmd, strPath, FALSE);
    if (ret == FALSE) {
        MessageBox(NULL, _T("复制文件失败，是否权限不足？\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
        ::exit(0);
    }
}
void ChooseAutoInvoke() {
    //CString strPath = CString(_T("c:\\windows\\System32\\RemoteCtrl.exe"));
    CString strPath = _T("C:\\Users\\weima\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl.exe");
    if (PathFileExists((LPCTSTR)strPath)) {
        return;
    }
    
    CString strInfo = _T("该程序只允许用于合法的用途！\n");
    strInfo += _T("继续运行该程序，将使得这台机器处于被监控状态!\n");
    strInfo += _T("如果你不想这样，请按“取消”按钮，退出程序。!\n");
    strInfo += _T("按下“是”按钮，该程序将被复制到你的机器上，并随系统启动而自动运行！\n");
    strInfo += _T("按下“否”按钮，程序只运行一次，不会在系统内留下任何东西！\n");
    int ret = MessageBox(NULL, strInfo, _T("警告"),MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);
    if (ret == IDYES) {
        //WriteRegisterTable();
        WriteStartupDir(strPath);
    }
    else if (ret != IDCANCEL) {
        exit(0);
    }
  
    return;
}

int main()
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // 初始化 MFC 并在失败时显示错误
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: 在此处为应用程序的行为编写代码。
            wprintf(L"错误: MFC 初始化失败\n");
            nRetCode = 1;
        }
        else
        {
            ChooseAutoInvoke();

            CCommand command;
            int ret = CServerSocket::getInstance()->Run(&CCommand::RunCommand, &command);
            switch (ret) {
            case -1:
                MessageBox(NULL, _T("网络初始化异常，未能成功初始化，请检查网络状态！"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
                exit(0);
                break;
            case -2:
                MessageBox(NULL, _T("多次无法正常接入用户，结束程序！"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
                exit(0);
                break;

            }
        }
    }
    else
    {
        // TODO: 更改错误代码以符合需要
        wprintf(L"错误: GetModuleHandle 失败\n");
        nRetCode = 1;
    }

    return nRetCode;
}
