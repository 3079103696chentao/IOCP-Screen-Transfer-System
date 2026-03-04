#include "pch.h"
#include "ClientController.h"
CClientController* CClientController::m_instance = nullptr;

CClientController::CHelper CClientController::m_helper;

std::map<UINT, CClientController::MSGFUNC>CClientController::m_mapFunc;

CClientController* CClientController::getInstance() {
	if (m_instance == nullptr) {
		m_instance = new CClientController();
	}
	struct { UINT nMsg; MSGFUNC func; }MsgFuncs[] = 
	{
		{WM_SEND_PACK, &CClientController::OnSendPack},
		{WM_SEND_DATA, &CClientController::OnSendData},
		{WM_SHOW_STATUS, &CClientController::OnShowStatus},
		{WM_SHOW_WATCH, &CClientController::OnShowWatcher},
	    {(UINT)-1, nullptr}
	};

	for (int i = 0; MsgFuncs[i].nMsg != -1; i++) {
		m_mapFunc.insert({ MsgFuncs[i].nMsg, MsgFuncs[i].func});
	}

	return m_instance;
}

void CClientController::releaseInstance() {
	if (m_instance != nullptr) {
		delete m_instance;
		m_instance = nullptr;
	}
}

int CClientController::InitController()
{
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientController::threadEntry,
		this, 0, &m_nThreadID);
	m_statusDlg.Create(IDD_DLG_STATUS, &m_remoteDlg);
	return 0;
}

int CClientController::Invoke(CWnd* pMainWnd){

	pMainWnd = &m_remoteDlg;

	return m_remoteDlg.DoModal();
}

LRESULT CClientController::SendMessage(MSG msg)
{
	//线程间通信 事件 同步对象 可以阻塞线程，让出CPU，知道条件满足，不存储事件，仅用于同步
	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); //创建事件，初始无信号得到事件句柄
	if (hEvent == NULL) return -2;  
	MSGINFO info(msg);

	PostThreadMessage(m_nThreadID, WM_SEND_MESSAGE,
		(WPARAM)&info, (LPARAM)&hEvent);//传递给工作线程

	WaitForSingleObject(hEvent, -1);//等待该事件
	return info.result;
}

void CClientController::threadFunc(){
	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_SEND_MESSAGE) {
			MSGINFO* pmsg = (MSGINFO*)msg.wParam;
			HANDLE hEvent = (HANDLE)msg.lParam;
			auto it = m_mapFunc.find(pmsg->msg.message);
			if (it != m_mapFunc.end()) {
				pmsg->result = (this->*it->second)(pmsg->msg.message, pmsg->msg.wParam, pmsg->msg.lParam);
			}
			else {
				pmsg->result = -1;
			}
			SetEvent(hEvent); //置为有信号
		}
		
	}

}

unsigned __stdcall CClientController::threadEntry(void* arg){
	CClientController* thiz = (CClientController*)arg;

	thiz->threadFunc();

	_endthreadex(0);
	return 0;
}

LRESULT CClientController::OnSendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return LRESULT();
}

LRESULT CClientController::OnSendData(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return LRESULT();
}

LRESULT CClientController::OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return m_statusDlg.ShowWindow(SW_SHOW); //将 m_statusDlg 所代表的窗口显示出来。
}

LRESULT CClientController::OnShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return m_watchDlg.DoModal();
}
