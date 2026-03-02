#include "pch.h"
#include "Command.h"

CCommand::CCommand():threadid(0)
{
	struct {
		int nCmd;
		CMDFUNC func;
	}data[] = {
		{1,&CCommand::MakeDriverInfo},
		{2,&CCommand::MakeDirectoryInfo},
		{3,&CCommand::RunFile},
		{4,&CCommand::DownloadFile},
        {5,&CCommand::MouseEvent},
		{6,&CCommand::SendScreen},
		{7,&CCommand::LockMachine},
		{8,&CCommand::UnlockMachine},
		{9,&CCommand::DeleteLocalFile},
		{1981,&CCommand::TestConnect},
		{-1,nullptr}
	};
	for (int i = 0; data[i].nCmd != -1; i++) {
		m_mapFunction.insert({ data[i].nCmd, data[i].func });
	}
}

int CCommand::ExcuteCommand(int nCmd)
{
	auto it = m_mapFunction.find(nCmd);
	if (it == m_mapFunction.end()) {
		return -1;
	}
	int ret = (this->*(it->second))();
	return 0;
}
int CCommand::MakeDriverInfo() {
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
	//CEdoyunTool::Dump(( BYTE*)pack.Data(), pack.Size());
	CServerSocket::getInstance()->Send(pack);
	return 0;
}
int CCommand::MakeDirectoryInfo() {
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
int CCommand::RunFile() {
    std::string strPath;
    if (CServerSocket::getInstance()->GetFilePath(strPath) == false) {
        OutputDebugString(_T("当前的命令，不是获取文件列表，命令解析错误！！！"));
        return -1;
    }
    ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    CPacket pack(3, nullptr, 0);
    CServerSocket::getInstance()->Send(pack);
    return 0;
}
int CCommand::DownloadFile() {
    std::string strPath;
    CServerSocket::getInstance()->GetFilePath(strPath);
    long long data = 0;
    FILE* pFile = nullptr;
    errno_t err = fopen_s(&pFile, strPath.c_str(), "rb"); //服务端 上传 读 按照二进制方式读
    if (err != 0) {
        CPacket pack(4, (BYTE*)&data, 8);
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
int CCommand::MouseEvent() {
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

        TRACE("mouse event:%08x x: %d y:%d\r\n", nFlags, mouse.ptXY.x, mouse.ptXY.y);

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
            mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
            TRACE("鼠标移动 mouse.ptXY.x：%d,mouse.ptXY.y:%d\r\n", mouse.ptXY.x, mouse.ptXY.y);
            break;
        }
        CPacket pack(5, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }
    else {
        OutputDebugString(_T("获取鼠标操作参数失败！！！"));
        return -1;
    }
}
int CCommand::SendScreen() {
    CImage screen; //GDI global device interface
    HDC hScreen = ::GetDC(NULL); //获取句柄
    int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);//24 ARGB8888 32 RGB565  RGB444
    int nWidth = GetDeviceCaps(hScreen, HORZRES);
    int nHeight = GetDeviceCaps(hScreen, VERTRES);
    screen.Create(nWidth, nHeight, nBitPerPixel);
    BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);
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
void CCommand::threadLockDlgMain() {
    TRACE("%s(%d):%d\r\n", __FUNCTION__, __LINE__, GetCurrentThreadId());
    dlg.Create(IDD_DIALOG_INFO, NULL);
    dlg.ShowWindow(SW_SHOW);
    //遮蔽后台窗口
    CRect rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = GetSystemMetrics(SM_CXFULLSCREEN);//获取横向分辨率
    rect.bottom = GetSystemMetrics(SM_CYFULLSCREEN);//获取纵向分辨率
    rect.bottom = (LONG)rect.bottom * 1.1;
    TRACE("right = %d bottom = %d\r\n", rect.right, rect.bottom);
    dlg.MoveWindow(rect);//将对话框作用于全屏
    CWnd* pText = dlg.GetDlgItem(IDC_STATIC);
    if (pText) {
        CRect rtText;
        pText->GetWindowRect(rtText);
        int nWidth = rtText.Width(); //获取文本框的宽度
        int x = (rect.right - nWidth) / 2;
        int nHeight = rtText.Height();
        int y = (rect.bottom - nHeight) / 2;
        pText->MoveWindow(x, y, rtText.Width(), rtText.Height());
    }
    //窗口置顶
    dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE); //不改变大小和移动 置顶
    //限制鼠标功能
    ShowCursor(false);
    //隐藏任务栏
    ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE);
    //限制鼠标活动范围
    dlg.GetWindowRect(rect);
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
    //恢复鼠标范围限制
    ClipCursor(NULL);
    //恢复鼠标
    ShowCursor(true);
    //恢复任务栏
    ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_SHOW);
    dlg.DestroyWindow();
}
unsigned _stdcall CCommand::threadLockDlg(void* arg) {
    CCommand* thiz = (CCommand*)arg;
    thiz->threadLockDlgMain();
    _endthreadex(0);
    return 0;
}
int CCommand::LockMachine() {
    if (dlg.m_hWnd == NULL || dlg.m_hWnd == INVALID_HANDLE_VALUE) {
        _beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid); //另开一个线程的原因，避免主线程卡在对话框函数，而不能接收其他消息，而这个线程进行消息循环
        TRACE("threadid = %d\r\n", threadid);
    }
    CPacket pack(7, NULL, 0);
    CServerSocket::getInstance()->Send(pack);

    return 0;
}
int CCommand::UnlockMachine() {
    TRACE("now threadid = %d\r\n", threadid);
    PostThreadMessage(threadid, WM_KEYDOWN, 0x1b, 00010001);
    CPacket pack(8, NULL, 0);
    CServerSocket::getInstance()->Send(pack);
    return 0;
}
int CCommand::DeleteLocalFile() {
    std::string strPath;
    CServerSocket::getInstance()->GetFilePath(strPath);
    // TCHAR sPath[MAX_PATH] = _T("");
     //mbstowcs(sPath, strPath.c_str(), strPath.size()); //由多字节字符集转为宽字节字符集
    DeleteFile(strPath.c_str());

    CPacket pack(9, NULL, 0);
    CServerSocket::getInstance()->Send(pack);
    return 0;
}
int CCommand::TestConnect() {
    CPacket pack(1981, NULL, 0);
    CServerSocket::getInstance()->Send(pack);
    return 0;
}