// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。


#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include <direct.h>
#include <atlimage.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 唯一的应用程序对象

CWinApp theApp;

using namespace std;

void Dump(BYTE* pData, size_t nSize) {
    std::string strOut;
    for (size_t i = 0; i < nSize; i++) {
        char buf[8] = "";
        if (i > 0 && (i % 16 == 0)) strOut += "\n";
        snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF); //不足两位补0，和0xFF，确保只取低八位
        strOut += buf;
    }
    strOut += "\n";
    OutputDebugStringA(strOut.c_str());
}
int MakeDriverInfo() { // 1==>A 2==>B 3==>C
    std::string result;
    for (int i = 1; i <= 26; i++)
    {
        if (_chdrive(i) == 0) {
            if (result.size() > 0) {
                result += ',';
            }
            result += 'A' + i - 1;
        }
    }
    CPacket pack(1, (BYTE*)result.c_str(), result.size());//打包
    //Dump(( BYTE*)pack.Data(), pack.Size());
    CServerSocket::getInstance()->Send(pack);
    return 0;
}
#include<io.h>//包含_findfist等函数
#include<list>


int MakeDirectoryInfo() {
    std::string strPath; //文件路径
    //std::list<FILEINFO>lstFileInfos;//存储文件信息

    if (CServerSocket::getInstance()->GetFilePath(strPath) == false) {
        OutputDebugString(_T("当前的命令，不是获取文件列表，命令解析错误！！！"));
        return -1;
    }
    if (_chdir(strPath.c_str()) != 0) {
        FILEINFO finfo;
        finfo.HaNext = false; //没有权限访问目录
        CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
        //lstFileInfos.push_back(finfo);
        CServerSocket::getInstance()->Send(pack);

        OutputDebugString(_T("没有权限，访问目录！！！"));
        return -2;
    }
   /* char szCurrentPath2[MAX_PATH] = { 0 };
    if (_getcwd(szCurrentPath2, MAX_PATH) != NULL) {
        TRACE("切换目录后的工作目录: %s\r\n", szCurrentPath2);
        OutputDebugStringA(("切换后当前目录: " + std::string(szCurrentPath2) + "\n").c_str());
    }*/
    _finddata_t fdata;
    long long hfind = 0; //64位编译器 int长度不够
    if ((hfind = _findfirst("*", &fdata)) == -1) {
        //查找失败
        OutputDebugString(_T("没有找到任何文件！！！"));
        FILEINFO finfo;
        finfo.HaNext = false; //没有权限访问目录
        CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
        CServerSocket::getInstance()->Send(pack);
       
        return -3;
    }
    int Count = 0;
    do {
        FILEINFO finfo;
        finfo.isDirectory = (fdata.attrib & _A_SUBDIR) != 0;
        memcpy(finfo.szFileName, fdata.name, sizeof(fdata.name));
        //TRACE("[%s]\r\n", finfo.szFileName);
        //lstFileInfos.push_back(finfo);
        CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
        
        CServerSocket::getInstance()->Send(pack);
        Count++;

    } while (!_findnext(hfind, &fdata));
    TRACE("Server send Count = %d\r\n", Count);
    //发送信息到控制端
    FILEINFO finfo;
    finfo.HaNext = false;
    CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
    CServerSocket::getInstance()->Send(pack);


    return 0;
}
int RunFile() {
    string strPath;
    if (CServerSocket::getInstance()->GetFilePath(strPath) == false) {
        OutputDebugString(_T("当前的命令，不是获取文件列表，命令解析错误！！！"));
        return -1;
    }
    ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    CPacket pack(3, nullptr, 0);
    CServerSocket::getInstance()->Send(pack);
    return 0;
}
#pragma warning(disable:4966) // fopen sprintd strcpy strstr
int DownloadFile() {
    std::string strPath;
    CServerSocket::getInstance()->GetFilePath(strPath);
    long long data = 0;
    FILE* pFile = nullptr;
    errno_t err = fopen_s(&pFile,strPath.c_str(), "rb"); //服务端 上传 读 按照二进制方式读
    if (err != 0 ) {
        CPacket pack(4,(BYTE*)& data, 8);
        CServerSocket::getInstance()->Send(pack);
        return -1;
    }
    if (pFile != nullptr) {

        fseek(pFile, 0, SEEK_END); //将文件指针移动到文件末尾
        data = _ftelli64(pFile);
        CPacket head(4, (BYTE*)&data, 8);
        CServerSocket::getInstance()->Send(head);
        fseek(pFile, 0, SEEK_SET);//将文件指针移动到文件开头
        char buffer[1024] = "";
        size_t rlen = 0;
        do {
            rlen = fread(buffer, 1, 1024, pFile);
            CPacket pack(4, (BYTE*)buffer, rlen);
            CServerSocket::getInstance()->Send(pack);
        } while (rlen >= 1024); //当读到的字节数小于1024说明读到文件尾
        fclose(pFile);

    }
    CPacket pack(4, NULL, 0);
    CServerSocket::getInstance()->Send(pack);
   
    return 0;
}
int MouseEvent() {
    MOUSEEV mouse;
    if (CServerSocket::getInstance()->GetMouseEvent(mouse)) {
       
        DWORD nFlags = 0;
        switch (mouse.nButton) {
        case 0://左键
            nFlags = 1;
            break;
        case 1://右键
            nFlags = 2;
            break;
        case 2://中键
            nFlags = 4;
            break;
        case 4: //没有按键
            nFlags = 8;
            break;
        }

        if (nFlags != 8) {
            SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
        }

        switch (mouse.nAction) {
        case 0://单击
            nFlags |= 0x10;
            break;
        case 1://双击
            nFlags |= 0x20;
            break;
        case 2: //按下
            nFlags |= 0x40;
            break;
        case 3: //放开
            nFlags |= 0x80;
            break;
        default:break;
        }
        switch (nFlags) {
        case 0x21: //左键双击
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
        case 0x11: //左键单击
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
       
        case 0x41: //左键按下
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x81: //左键放开
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x22: //右键双击
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
        case 0x12: //右键单击
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x42: //右键按下
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x82: //右键放开
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        
        case 0x44: //中键双击
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
        case 0x24: //中键单击
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x14: //中键按下
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x84: //中键放开
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x08: //鼠标移动
            mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
            break;
        }
        CPacket pack(4, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
    }
    else {
        OutputDebugString(_T("获取鼠标操作参数失败！！！"));
        return -1;
    }
}
int SendScreen() {

    CImage screen; //GDI global device interface
    HDC hScreen = ::GetDC(NULL); //获取句柄
    int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);//24 ARGB8888 32 RGB565  RGB444
    int nWidth = GetDeviceCaps(hScreen, HORZRES);
    int nHeight = GetDeviceCaps(hScreen, VERTRES);
    screen.Create(nWidth, nHeight, nBitPerPixel);
    BitBlt(screen.GetDC(), 0, 0, 1920, 1020, hScreen, 0, 0, SRCCOPY);
    ReleaseDC(NULL, hScreen);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);//将文件保存到内存中
    if (hMem == NULL) return -1;
    IStream* pStream = NULL;
    HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
    if (ret == S_OK) {
        screen.Save(pStream, Gdiplus::ImageFormatJPEG);
        LARGE_INTEGER bg = { 0 };
        pStream->Seek(bg, STREAM_SEEK_SET, NULL); //将流指针指向文件开头

        PBYTE pData = (PBYTE)GlobalLock(hMem);
        SIZE_T nSize = GlobalSize(hMem);
        CPacket pack(6, pData, nSize);
        CServerSocket::getInstance()->Send(pack);
        GlobalLock(hMem);
        
    }
    
    /*for (int i = 0; i < 10; i++) {
        DWORD tick = GetTickCount64();
        screen.Save(_T("test2020.png"), Gdiplus::ImageFormatPNG);
        TRACE("PNG %d\r\n", GetTickCount64() - tick);

        tick = GetTickCount64();
        screen.Save(_T("test2020.jpg"), Gdiplus::ImageFormatJPEG);
        TRACE("JPG %d\r\n", GetTickCount64() - tick);
    }*/
    /*screen.Save(_T("test2020.jpg"), Gdiplus::ImageFormatJPEG);*/
    pStream->Release();
    GlobalFree(hMem);
    screen.ReleaseDC();
    
    return 0;
}
#include"LockDialog.h"
CLockDialog dlg;
unsigned threadid = 0;

unsigned _stdcall threadLockDlg(void* arg) {
    TRACE("%s(%d):%d\r\n",__FUNCTION__, __LINE__, GetCurrentThreadId());

    dlg.Create(IDD_DIALOG_INFO, NULL);
    dlg.ShowWindow(SW_SHOW);
    //遮蔽后台窗口
    CRect rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = GetSystemMetrics(SM_CXFULLSCREEN);//获取横向分辨率
    rect.bottom = GetSystemMetrics(SM_CYFULLSCREEN);//获取纵向分辨率
    rect.bottom = (LONG)rect.bottom*1.1;
    TRACE("right = %d bottom = %d\r\n", rect.right, rect.bottom);
    dlg.MoveWindow(rect);//将对话框作用于全屏
    //窗口置顶
    //dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE); //不改变大小和移动 置顶
    //限制鼠标功能
    ShowCursor(false);
    //隐藏任务栏
    ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE);
    //限制鼠标活动范围
    //dlg.GetWindowRect(rect);
    rect.left = 0;
    rect.top = 0;
    rect.right = 1;
    rect.bottom = 1;
    ClipCursor(rect);//限制鼠标活动范围 鼠标的范围在左上角
    MSG msg;
    //消息循环
    while (GetMessage(&msg, NULL, 0, 0)) { //依赖于线程，只能接收该进程的消息，消息泵
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_KEYDOWN) {
            TRACE("msg: %08X wparam:%08x Iparam:%08X\r\n", msg.message, msg.wParam, msg.lParam);
            if (msg.wParam == 0x1B) { //按下ESC退出
                break;
            }
        }
    }
   
    ShowCursor(true);
    //显示任务栏
    ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_SHOW);
    dlg.DestroyWindow();
    _endthreadex(0);

    return 0;
}

int LockMachine() { 
    if (dlg.m_hWnd == NULL || dlg.m_hWnd == INVALID_HANDLE_VALUE) {
        _beginthreadex(NULL, 0, threadLockDlg, NULL, 0, &threadid); //另开一个线程的原因，避免主线程卡在对话框函数，而不能接收其他消息，而这个线程进行消息循环
        TRACE("threadid = %d\r\n", threadid);
    }
    CPacket pack(7, NULL, 0);
    CServerSocket::getInstance()->Send(pack); 
    
    return 0;
}

int UnlockMachine() {
    
   // dlg.SendMessage(WM_KEYDOWN, 0x1b, 00010001);
   // ::SendMessage(dlg.m_hWnd, WM_KEYDOWN, 0x1b, 00010001);
    TRACE("now threadid = %d\r\n", threadid);
    PostThreadMessage(threadid, WM_KEYDOWN, 0x1b, 00010001);
    CPacket pack(7, NULL, 0);
    CServerSocket::getInstance()->Send(pack);
    return 0;
}
int TestConnect() {
    CPacket pack(1981, NULL, 0);
    CServerSocket::getInstance()->Send(pack);
    return 0;
}

int DeleteLocalFile() {

    std::string strPath;
    CServerSocket::getInstance()->GetFilePath(strPath);
   // TCHAR sPath[MAX_PATH] = _T("");
    //mbstowcs(sPath, strPath.c_str(), strPath.size()); //由多字节字符集转为宽字节字符集
    DeleteFile(strPath.c_str());

    CPacket pack(9, NULL, 0);
    CServerSocket::getInstance()->Send(pack);
    return 0;
}

int ExcuteCommand(int nCmd) {
    int ret = 0;
    switch (nCmd) {
    case 1: //查看磁盘分区
        ret = MakeDriverInfo();
        break;
    case 2://查看指定目录下的文件
        ret = MakeDirectoryInfo();
        break;
    case 3: //打开文件
        ret = RunFile();
        break;
    case 4://下载文件
        ret = DownloadFile();
        break;
    case 5://鼠标操作
        ret = MouseEvent();
        break;
    case 6://发送屏幕内容==>发送屏幕的截图
        ret = SendScreen();
        break;
    case 7://锁机
        ret = LockMachine();
        break;
    case 8:
        ret = UnlockMachine();
        break;
    case 9:
        ret = DeleteLocalFile();
    case 1981:
        ret = TestConnect();
        break;
    }
    return ret;
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
            //1.进度的可控性 2.对接的方便性 3.可行性评估，提早暴露风险
            // TODO: socket bind listen accept read write close
            
            CServerSocket* pserver = CServerSocket::getInstance();
            int count = 0;
            if (pserver->InitSocket() == false) {
                MessageBox(NULL, _T("网络初始化异常，未能成功初始化，请检查网络状态！"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
                exit(0);
            }
            while(CServerSocket::getInstance()!=NULL) {  
                if (pserver->AcceptClient() == false) {
                    if(count >=3){
                        MessageBox(NULL, _T("多次无法正常接入用户，结束程序！"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
                        exit(0);
                    }
                    MessageBox(NULL, _T("无法正常接入用户，自动重试"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
                    count++;
                }
                int ret = pserver->DealCommand();
                if (ret != -1) {
                    ret = ExcuteCommand(ret);
                    if (ret != 0) {
                        TRACE("执行命令失败：%d ret = %d\r\n", pserver->GetPacket().sCmd, ret);
                    }
                    pserver->CloseClient();
                    TRACE("Command has done\r\n");
            }
            
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
