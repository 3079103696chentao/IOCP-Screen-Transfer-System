// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。


#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "command.h"
#include <conio.h>
#ifdef _DEBUG
#define new DEBUG_NEW
#endif
//CString strPath = CString(_T("c:\\windows\\System32\\RemoteCtrl.exe"));
#define INVOKE_PATH _T("C:\\Users\\weima\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl.exe");
//CString INVOKE_PATH = CString(_T("C:\\Users\\ctct\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl.exe"));
// 唯一的应用程序对象

CWinApp theApp;

using namespace std;

bool ChooseAutoInvoke(const CString strPath) {


	if (PathFileExists((LPCTSTR)strPath)) {
		return true;
	}

	CString strInfo = _T("该程序只允许用于合法的用途！\n");
	strInfo += _T("继续运行该程序，将使得这台机器处于被监控状态!\n");
	strInfo += _T("如果你不想这样，请按“取消”按钮，退出程序。!\n");
	strInfo += _T("按下“是”按钮，该程序将被复制到你的机器上，并随系统启动而自动运行！\n");
	strInfo += _T("按下“否”按钮，程序只运行一次，不会在系统内留下任何东西！\n");
	int ret = MessageBox(NULL, strInfo, _T("警告"), MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);
	if (ret == IDYES) {
		//WriteRegisterTable(strPath);
		if (!CEdoyunTool::WriteStartupDir(strPath)) {
			MessageBox(NULL, _T("复制文件失败，是否权限不足？\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
			return false;
		}
	}
	else if (ret != IDCANCEL) {
		return false;
	}

	return true;
}

enum {
	IocpListEmpty,
	IocpListPush,
	IocpListPop
};

typedef struct IocpParam {
	int nOperator;//操作
	std::string strData;//数据
	_beginthread_proc_type cbFunc; //回调
	IocpParam(int op, const char* sData, _beginthread_proc_type cb = NULL) {
		nOperator = op;
		strData = sData;
		cbFunc = cb;
	}
	IocpParam() {
		nOperator = -1;
		cbFunc = nullptr;
	}

}IOCP_PARAM;

HANDLE hIOCP = INVALID_HANDLE_VALUE;//IO Complete Port
void threadQueueEntry(HANDLE hIOCP) {

	std::list<std::string>lstString;
	DWORD dwTransfered = 0;
	ULONG_PTR CompletionKey = 0;
	LPOVERLAPPED* pOverlapped = nullptr;
	while(GetQueuedCompletionStatus(hIOCP, &dwTransfered, &CompletionKey, pOverlapped, INFINITE)){
		if (dwTransfered == 0 && (CompletionKey == NULL)) {
			printf("thread is prepare to exit");
			break;
		}
		IOCP_PARAM* pParam = (IOCP_PARAM*)CompletionKey;
		if (pParam->nOperator == IocpListPush) {
			lstString.push_back(pParam->strData);
		}
		else if (pParam->nOperator == IocpListPop) {
			std::string* pStr = NULL;
			if (lstString.size() > 0) {
			    pStr = new std::string(lstString.front());
				lstString.pop_front();
			}
			if (pParam->cbFunc) {
				pParam->cbFunc(pStr);
			}	
		}
		else if (pParam->nOperator == IocpListEmpty) {
			lstString.clear();
		}
		delete pParam;
	}


	_endthread();
}

void func(void* arg) {
	std::string* pstr = (std::string*)arg;
	if (pstr != NULL) {
		printf("pop from list:%s\r\n", pstr->c_str());
		delete pstr;
	}
	else {
		printf("list is empty , no data!\r\n");
	}
}

int main()
{
	if (!CEdoyunTool::Init()) return 1;
	printf("press any key to exit...\r\n");
	hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);//epoll是单线程处理的 ,完成端口映射可以允许有多个线程
	HANDLE hThread = (HANDLE)_beginthread(threadQueueEntry, 0, hIOCP);
	

	DWORD tick = GetTickCount64();
	while (_kbhit() != 0) {//完成端口，把请求和实现分离了

		if (GetTickCount() - tick > 1300) {
			PostQueuedCompletionStatus(hIOCP, sizeof(IOCP_PARAM), (ULONG_PTR)new IOCP_PARAM(IocpListPop, "hello world"), NULL);
		}

		if (GetTickCount() - tick > 2000) {
			PostQueuedCompletionStatus(hIOCP, sizeof(IOCP_PARAM), (ULONG_PTR)new IOCP_PARAM(IocpListPush, "hello world"), NULL);
			tick = GetTickCount64();
		}
		Sleep(1);
	}
	if(hIOCP != NULL) {
		//TODO:唤醒完成端口
		PostQueuedCompletionStatus(hIOCP, 0, NULL, NULL);
		WaitForSingleObject(hIOCP, INFINITE);

	}

	CloseHandle(hIOCP);

	printf("exit done !\r\n");
	/*if (CEdoyunTool::IsAdmain()) {
		OutputDebugString("current is run as admainistrator!\r\n");
		if (!CEdoyunTool::Init()) return 1;
		CString strPath = INVOKE_PATH;
		if (ChooseAutoInvoke(strPath)) {
			CCommand command;
			int ret = CServerSocket::getInstance()->Run(&CCommand::RunCommand, &command);
			switch (ret) {
			case -1:
				MessageBox(NULL, _T("网络初始化异常，未能成功初始化，请检查网络状态！"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
				break;
			case -2:
				MessageBox(NULL, _T("多次无法正常接入用户，结束程序！"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
				break;
			}
		}
    }
	else {
		OutputDebugString("current is run as normal user!\r\n");
		if (!CEdoyunTool::RunAsAdmin()) {
			CEdoyunTool::ShowError();
			return 1;
		}
		return 0;
	}*/

	return 0;
}



