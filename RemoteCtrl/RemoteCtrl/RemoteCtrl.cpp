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
void ShowError() {
    LPVOID lpMessageBuf = NULL;
    //strerror(errno);//标准c语言库
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
     NULL, GetLastError(),
     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPSTR)&lpMessageBuf,0,NULL);
    OutputDebugString((LPCSTR)lpMessageBuf);
    LocalFree(lpMessageBuf);
}

bool IsAdmain() {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        ShowError();
        return false;
    }
    TOKEN_ELEVATION eve;
    DWORD len = 0;
    if (GetTokenInformation(hToken, TokenElevation, &eve, sizeof(eve), &len) == FALSE) {
        ShowError();
        return false;
    }
    CloseHandle(hToken);
    if (len == sizeof(eve)) {
        return eve.TokenIsElevated;
    }
    printf("length of tokeninformation is d%\r\n", len);
    return false;
}

void RunAsAdmin() {
    TCHAR szExePath[MAX_PATH] = { 0 };
    if (!GetModuleFileName(NULL, szExePath, MAX_PATH))
    {
        OutputDebugString(_T("获取程序路径失败！\r\n"));
        ::exit(0);
    }

    // 第三步：初始化提权启动参数（触发UAC）
    SHELLEXECUTEINFO sei = { 0 };
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS; // 保留进程句柄
    sei.lpVerb = _T("runas");            // 关键：指定「以管理员身份运行」
    sei.lpFile = szExePath;              // 要启动的程序（当前程序自身）
    sei.lpParameters = NULL;             // 传递给新进程的参数（无则传NULL）
    sei.nShow = SW_HIDE;                 // 后台运行（无窗口）

    // 第四步：启动提权后的进程
    if (!ShellExecuteEx(&sei))
    {
        DWORD dwErr = GetLastError();
        // 常见错误处理：用户取消UAC弹窗、权限不足等
        switch (dwErr)
        {
        case ERROR_CANCELLED:
            OutputDebugString(_T("用户取消了管理员权限请求！\r\n"));
            break;
        case ERROR_ACCESS_DENIED:
            OutputDebugString(_T("系统拒绝提权请求（账户无管理员权限）！\r\n"));
            break;
        default:
            ATLTRACE(_T("提权失败，错误码：%d\r\n"), dwErr);
            break;
        }
        ::exit(0);
    }
    /*HANDLE hToken = NULL;
    BOOL ret = LogonUser("Administrator", NULL, NULL, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken);
    if (!ret) {
        ShowError();
        MessageBox(NULL, "登录错误", "程序错误", 0);
        ::exit(0);
    }
    OutputDebugString("Logon administrator success\r\n");
    STARTUPINFOW si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    TCHAR sPath[MAX_PATH] = "";
    GetCurrentDirectory(MAX_PATH, sPath);
    CString strCmd = sPath;
    strCmd += _T("\\RemoteCtrl.exe");
    ret = CreateProcessWithTokenW(hToken, LOGON_WITH_PROFILE, NULL,(LPWSTR)(LPCWSTR)strCmd.GetBuffer(),
        CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
    CloseHandle(hToken);
    if (!ret) {
        ShowError();
        MessageBox(NULL, _T("创建进程失败"), _T("程序错误"), 0);
        ::exit(0);
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);*/
}

int main()
{
    int nRetCode = 0;
    if (IsAdmain()) {
        OutputDebugString("current is run as admainistrator!\r\n");
    }
    else {
        OutputDebugString("current is run as normal user!\r\n");
        RunAsAdmin();
        return nRetCode;
    }
    

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
