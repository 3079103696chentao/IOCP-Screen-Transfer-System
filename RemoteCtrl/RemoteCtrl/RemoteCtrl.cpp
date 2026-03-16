// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。


#include "pch.h"

#include<iostream>
#include "framework.h"
#include "RemoteCtrl.h"
#include "command.h"
#include "EdoyunQueue.h"
#include"EdoyunServer.h"
#include <conio.h>
#include<list>
#include<MSWSock.h>
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

class COverlapped {
public:
	OVERLAPPED m_overlapped;
	DWORD m_operator;
	char m_buffer[4096];
	COverlapped() {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_operator = 0;
		memset(&m_buffer, 0, sizeof(m_buffer));
	}
};


/*void iocp() {
	//SOCKET socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	SOCKET socket_fd = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (socket_fd == INVALID_SOCKET) {
		CEdoyunTool::ShowError();
		return;
	}
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9527);
	addr.sin_addr.s_addr = inet_addr("0.0.0.0");

	if (bind(socket_fd, (sockaddr*)&addr, sizeof(sockaddr_in)) == SOCKET_ERROR) {
		printf("failed to bind ip = %s, port = %d\r\n", "0.0.0.0", 9527);
		return;
	}

	listen(socket_fd, 5);

	HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, socket_fd, 4);
	SOCKET client = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (client == INVALID_SOCKET) {
		CEdoyunTool::ShowError();
	}
	CreateIoCompletionPort((HANDLE)socket_fd, hIOCP, 0, 0);

	COverlapped overlapped;
	memset(&overlapped, 0, sizeof(OVERLAPPED));
	overlapped.m_operator = 1;
	DWORD received = 0;
	if (AcceptEx(socket_fd, client, overlapped.m_buffer, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &received, &overlapped.m_overlapped) == false) {
		CEdoyunTool::ShowError();
	}

	WSASend();
	WSARecv();

	//开启线程
	while (true) {
		//代表一个线程
		LPOVERLAPPED pOverlapped = NULL;
		DWORD transferred = 0;
		ULONG_PTR key = 0;
		if (GetQueuedCompletionStatus(hIOCP, &transferred, &key, &pOverlapped, INFINITE)) {
			COverlapped* pO = CONTAINING_RECORD(pOverlapped, COverlapped, m_overlapped);
			switch (pO->m_operator) {
			case 1:
				//处理accept
			}
		}
	}
}
*/

void iocp() {
	EdoyunServer server;
	server.StartService();
	getchar();
}
int main()
{
	
	if (!CEdoyunTool::Init()) return 1;
	iocp();
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



