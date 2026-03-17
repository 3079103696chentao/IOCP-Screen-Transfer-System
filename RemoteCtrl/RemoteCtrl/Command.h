#pragma once
#include<map>
#include<atlimage.h>
#include"ServerSocket.h"
#include<direct.h>
#include<io.h>//包含_findfist等函数
#include<list>
#include<string>
#include"EdoyunTool.h"
#include"LockDialog.h"
#include"resource.h"
#include"Packet.h"
#pragma warning (disable 4996)
class CCommand
{
public:
	CCommand();
	~CCommand() {}
	int ExcuteCommand(int nCmd, std::list<CPacket>& lstPacket, CPacket& inPacket);
	static void RunCommand(void* arg, int status, std::list<CPacket>& lstPacket, CPacket& inPacket) {
		CCommand* thiz = (CCommand*)arg;
		if (status > 0) {
			int ret = thiz->ExcuteCommand(status, lstPacket, inPacket);
			if (ret != 0) {
				TRACE("执行命令失败：%d ret = %d\r\n", status, ret);
			}
		}
		else {
			MessageBox(NULL, _T("无法正常接入用户，自动重试"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
		}

	}
protected:
	typedef int(CCommand::* CMDFUNC)(std::list<CPacket>&, CPacket&); //成员函数指针
	std::map<int, CMDFUNC>m_mapFunction;//从命令号到功能的映射
	CLockDialog dlg;
	unsigned threadid;
protected:
	int MakeDriverInfo(std::list<CPacket>& lstPacket, CPacket& inPacket) {
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
		lstPacket.push_back(CPacket(1, (BYTE*)result.c_str(), result.size()));
		return 0;
	}
	int MakeDirectoryInfo(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		std::string strPath = inPacket.strData; //文件路径
		//std::list<FILEINFO>lstFileInfos;//存储文件信息
		if (_chdir(strPath.c_str()) != 0) {
			FILEINFO finfo;
			finfo.HaNext = false; //没有权限访问目录
			lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
			OutputDebugString(_T("没有权限，访问目录！！！"));
			return -2;
		}

		_finddata_t fdata;
		long long hfind = 0; //64位编译器 int长度不够
		if ((hfind = _findfirst("*", &fdata)) == -1) {
			//查找失败
			OutputDebugString(_T("没有找到任何文件！！！"));
			FILEINFO finfo;
			finfo.HaNext = false; //没有权限访问目录
			lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
			return -3;
		}
		int Count = 0;
		do {
			FILEINFO finfo;
			finfo.isDirectory = (fdata.attrib & _A_SUBDIR) != 0;
			memcpy(finfo.szFileName, fdata.name, sizeof(fdata.name));
			lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
			Count++;

		} while (!_findnext(hfind, &fdata));
		TRACE("Server send Count = %d\r\n", Count);
		//发送信息到控制端
		FILEINFO finfo;
		finfo.HaNext = false;
		lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
		return 0;
	}
	int RunFile(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		std::string strPath = inPacket.strData;
		ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
		lstPacket.push_back(CPacket(3, nullptr, 0));
		return 0;
	}
	int DownloadFile(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		std::string strPath = inPacket.strData;
		long long data = 0;
		FILE* pFile = nullptr;
		errno_t err = fopen_s(&pFile, strPath.c_str(), "rb"); //服务端 上传 读 按照二进制方式读
		if (err != 0) {
			lstPacket.push_back(CPacket(4, nullptr, 0));
			return -1;
		}
		if (pFile != nullptr) {

			fseek(pFile, 0, SEEK_END); //将文件指针移动到文件末尾
			data = _ftelli64(pFile);
			lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));

			fseek(pFile, 0, SEEK_SET);//将文件指针移动到文件开头
			char buffer[1024] = "";
			size_t rlen = 0;
			do {
				rlen = fread(buffer, 1, 1024, pFile);
				lstPacket.push_back(CPacket(4, (BYTE*)buffer, rlen));

			} while (rlen >= 1024); //当读到的字节数小于1024说明读到文件尾
			fclose(pFile);

		}
		else {
			lstPacket.push_back(CPacket(4, nullptr, 0));
		}

		return 0;
	}
	int MouseEvent(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		MOUSEEV mouse;
		memcpy(&mouse, inPacket.strData.c_str(), sizeof(MOUSEEV));
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
		lstPacket.push_back(CPacket(5, nullptr, 0));
		return 0;
	}
	int SendScreen(std::list<CPacket>& lstPacket, CPacket& inPacket) {
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
			lstPacket.push_back(CPacket(6, pData, nSize));

			GlobalLock(hMem);

		}
		pStream->Release();
		GlobalFree(hMem);
		screen.ReleaseDC();
		return 0;
	}
	static unsigned _stdcall threadLockDlg(void* arg) {//_beginthreadex需要
		CCommand* thiz = (CCommand*)arg;
		thiz->threadLockDlgMain();
		_endthreadex(0);
		return 0;
	}
	void threadLockDlgMain() {
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

	int LockMachine(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		if (dlg.m_hWnd == NULL || dlg.m_hWnd == INVALID_HANDLE_VALUE) {
			_beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid); //另开一个线程的原因，避免主线程卡在对话框函数，而不能接收其他消息，而这个线程进行消息循环
			TRACE("threadid = %d\r\n", threadid);
		}
		lstPacket.push_back(CPacket(7, nullptr, 0));
		return 0;
	}
	int UnlockMachine(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		TRACE("now threadid = %d\r\n", threadid);
		PostThreadMessage(threadid, WM_KEYDOWN, 0x1b, 00010001);
		lstPacket.push_back(CPacket(8, nullptr, 0));
		return 0;
	}
	int DeleteLocalFile(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		std::string strPath = inPacket.strData;
		DeleteFile((LPCWSTR)strPath.c_str());
		lstPacket.push_back(CPacket(9, nullptr, 0));
		return 0;
	}
	int TestConnect(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		lstPacket.push_back(CPacket(1981, nullptr, 0));
		return 0;
	}
};

