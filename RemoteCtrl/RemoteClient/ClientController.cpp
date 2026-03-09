#include "pch.h"
#include "ClientController.h"
CClientController* CClientController::m_instance = nullptr;

CClientController::CHelper CClientController::m_helper;

//std::map<UINT, CClientController::MSGFUNC>CClientController::m_mapFunc;

CClientController* CClientController::getInstance() {
	if (m_instance == nullptr) {
		m_instance = new CClientController();
	}
	/*struct { UINT nMsg; MSGFUNC func; }MsgFuncs[] =
	{
		{WM_SHOW_STATUS, &CClientController::OnShowStatus},
		{WM_SHOW_WATCH, &CClientController::OnShowWatcher},
		{(UINT)-1, nullptr}
	};

	for (int i = 0; MsgFuncs[i].nMsg != -1; i++) {
		m_mapFunc.insert({ MsgFuncs[i].nMsg, MsgFuncs[i].func });
	}*/

	return m_instance;
}

void CClientController::releaseInstance() {
	if (m_instance != nullptr) {
		delete m_instance;
		m_instance = nullptr;
	}
}

//int CClientController::InitController()
//{
//	/*m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientController::threadEntry,
//		this, 0, &m_nThreadID);*/
//	//m_statusDlg.Create(IDD_DLG_STATUS, &m_remoteDlg);
//	///*m_statusDlg.ShowWindow(SW_HIDE);*/
//	return 0;
//}

int CClientController::Invoke(CWnd* pMainWnd) {

	pMainWnd = &m_remoteDlg;

	return m_remoteDlg.DoModal();
}

//LRESULT CClientController::SendMessage(MSG msg)
//{
//	//线程间通信 事件 同步对象 可以阻塞线程，让出CPU，知道条件满足，不存储事件，仅用于同步
//	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); //创建事件，初始无信号得到事件句柄
//	if (hEvent == NULL) return -2;
//	MSGINFO info(msg);
//
//	PostThreadMessage(m_nThreadID, WM_SEND_MESSAGE,
//		(WPARAM)&info, (LPARAM)&hEvent);//传递给工作线程
//
//	WaitForSingleObject(hEvent, -1);//等待该事件
//	CloseHandle(hEvent);
//	return info.result;
//}

void CClientController::UpdateAddress(int nIP, int nPort) {
	CClientSocket::getInstance()->UpdateAddress(nIP, nPort);
}

bool CClientController::SendCommandPacket(HWND hWnd, int nCmd, bool bAutoClose, BYTE* pData,
	size_t nLength, WPARAM wParam)
{
	TRACE(" cmd: %d ,%s start %lld \r\n", nCmd, __FUNCTION__, GetTickCount64());

	CClientSocket* pClient = CClientSocket::getInstance();

	bool ret = pClient->SendPacket(hWnd, CPacket(nCmd, pData, nLength), bAutoClose, wParam);
	if (!ret) {
		Sleep(30);
		ret = pClient->SendPacket(hWnd, CPacket(nCmd, pData, nLength), bAutoClose, wParam);
	}
	return ret;
}

//int CClientController::GetImage(CImage& image)
//{
//	//更新数据到缓存
//	CClientSocket* pClient = CClientSocket::getInstance();
//
//	return CEdoyunTool::Bytes2Image(image, pClient->GetPacket().strData);
//}

void CClientController::DwonloadEnd() {
	m_statusDlg.ShowWindow(SW_HIDE);
	m_remoteDlg.EndWaitCursor();
	m_remoteDlg.MessageBox(_T("下载完成！！"), _T("完成"));
}


int CClientController::DownFile(CString strPath)
{
	CFileDialog dlg(FALSE, NULL,
		strPath,
		OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
		NULL, &m_remoteDlg);

	if (dlg.DoModal() == IDOK) {
		m_strRemote = strPath;
		m_strLocal = dlg.GetPathName();
		
		
		SendCommandPacket(m_remoteDlg.GetSafeHwnd(), 4, false,
			(BYTE*)(LPCTSTR)m_strRemote, m_strRemote.GetLength(), (WPARAM)(LPCTSTR)m_strLocal);
		
		m_remoteDlg.BeginWaitCursor();

		m_statusDlg.Create(IDD_DLG_STATUS, &m_remoteDlg);
		m_statusDlg.m_info.SetWindowText(_T("命令正在执行中！"));
		m_statusDlg.ShowWindow(SW_SHOW);

		m_statusDlg.CenterWindow(&m_remoteDlg);
		m_statusDlg.SetActiveWindow();//激活与 m_statusDlg 对象关联的对话框窗口
		//调用后，该窗口将被激活并成为当前活动窗口，能够接收用户的键盘输入

		
	}

	return 0;
}

void CClientController::StartWatchScreen() {
	m_bIsClosed = false;
	m_hThreadWatch = (HANDLE)_beginthread(
		&CClientController::threadWatchScreenEntry, 0, this);
	m_watchDlg.DoModal();
	m_bIsClosed = true;
	WaitForSingleObject(m_hThreadWatch, 500);
}


void CClientController::threadWatchScreen() {
	//每当检测到m_image更新了，就发送命令到服务端，将得到的图片数据放到lstPacks，之后拿出数据，将其加载入图片中
	Sleep(50);
	ULONGLONG nTick = GetTickCount64();
	while (!m_bIsClosed) {
		if (GetTickCount64() - nTick < 200) {
			Sleep(200 + nTick - DWORD(GetTickCount64()));
		}
		nTick = GetTickCount64();
		int ret = SendCommandPacket(m_watchDlg.GetSafeHwnd(), 6, true);
		//TODO:添加消息相应函数 SEND_PACK_ACK
		//TODO:控制发送频率
		if (!ret) {
			TRACE("获取图片失败！ret = %d\r\n", ret);
			Sleep(1);
		}
	}
	TRACE("m_IsClosed is true, watchDlg closed\r\n");

}


void CClientController::threadWatchScreenEntry(void* arg) {
	CClientController* thiz = (CClientController*)arg;
	thiz->threadWatchScreen();
	_endthread();
}


//void CClientController::threadFunc() {
//	MSG msg;
//	while (::GetMessage(&msg, NULL, 0, 0)) {
//		TranslateMessage(&msg);
//		DispatchMessage(&msg);
//		if (msg.message == WM_SEND_MESSAGE) {
//			MSGINFO* pmsg = (MSGINFO*)msg.wParam;
//			HANDLE hEvent = (HANDLE)msg.lParam;
//			auto it = m_mapFunc.find(pmsg->msg.message);
//			if (it != m_mapFunc.end()) {
//				pmsg->result = (this->*it->second)(pmsg->msg.message, pmsg->msg.wParam, pmsg->msg.lParam);
//			}
//			else {
//				pmsg->result = -1;
//			}
//			SetEvent(hEvent); //置为有信号
//		}
//
//	}
//
//}

//unsigned __stdcall CClientController::threadEntry(void* arg) {
//	CClientController* thiz = (CClientController*)arg;
//
//	thiz->threadFunc();
//
//	_endthreadex(0);
//	return 0;
//}



//LRESULT CClientController::OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam)
//{
//	return m_statusDlg.ShowWindow(SW_SHOW); //将 m_statusDlg 所代表的窗口显示出来。
//}
//
//LRESULT CClientController::OnShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam)
//{
//	return m_watchDlg.DoModal();
//}
