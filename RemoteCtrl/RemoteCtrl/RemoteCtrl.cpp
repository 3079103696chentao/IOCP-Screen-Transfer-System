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

void initsock() {
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
}

void clearsock() {
	WSACleanup();
}

void udp_server() {

	printf("%s(%d):%s\r\n", __FILE__, __LINE__, __FUNCTION__);
	SOCKET sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		printf("%s(%d):%s:ERROR(%d)!!!\r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());
		return;
	}
	std::list<sockaddr_in>lstclients;
	sockaddr_in server_addr, client_addr;
	memset(&server_addr, 0, sizeof(sockaddr_in));
	memset(&client_addr, 0, sizeof(sockaddr_in));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(20000); //udp端口要设的大一点，两万以上，四万一下
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (bind(sock, (sockaddr*)&server_addr, sizeof(sockaddr)) != 0) {
		printf("%s(%d):%s ERROR %d", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError()); //识别网络错误的专用函数
		return;
	}
	char buffer[4096];
	int len = sizeof(client_addr);
	int ret = 0;
	while (!_kbhit()) {
		//tcp accept客户端获得客户端的套接字之后，在send和recv的时候，操作的是客户端的套接字
		//udp服务端一直在操作自己的套接字
		//udp通过recvfrom获取客户端的ip和port
		//函数不一样：tcp:recv, send udp:recvfrom, sendto
		ret = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&client_addr, &len);
		if (ret > 0) {
			if (lstclients.size() == 0) {
				lstclients.push_back(client_addr);
				printf("%s(%d):%s client1 ip:%08X, recvfrom port: %d\r\n",
					__FILE__, __LINE__, __FUNCTION__, client_addr.sin_addr.s_addr, (ntohs)(client_addr.sin_port));
				/*ret = sendto(sock, buffer, ret, 0, (sockaddr*)&client_addr, len);
				if (ret > 0) {
					printf("%s(%d):%s server send to ip:%08X, port: %d successfully\r\n",
						__FILE__, __LINE__, __FUNCTION__, client_addr.sin_addr.s_addr, client_addr.sin_port);
				}
				else {
					printf("%s(%d):%s server failed send to client ip:%08X, port: %d ret = %d, error(%d) \r\n",
						__FILE__, __LINE__, __FUNCTION__, 
						client_addr.sin_addr.s_addr, client_addr.sin_port, ret, WSAGetLastError());
				}*/
			}
			else if (lstclients.size() > 0) {
				printf("%s(%d):%s client2 ip:%08X, recvfrom port: %d\r\n",
					__FILE__, __LINE__, __FUNCTION__, client_addr.sin_addr.s_addr, (ntohs)(client_addr.sin_port));
				auto addr = lstclients.front();
				lstclients.pop_front();
				memset(buffer, 0, sizeof(buffer));
				memcpy(buffer, (void*)&addr, sizeof(addr));
				//向第二个客户端发送第一个客户端的信息
				int ret = sendto(sock, buffer, sizeof(addr), 0, (sockaddr*)&client_addr, sizeof(client_addr));
				break;
			}
			
			//CEdoyunTool::Dump((BYTE*)buffer, ret);
			/*printf("%s(%d):%s ip:%08X, recvfrom port: %d\r\n",
				__FILE__, __LINE__, __FUNCTION__, client_addr.sin_addr.s_addr, client_addr.sin_port);
			sendto(sock, buffer, ret, 0, (sockaddr*)&client_addr, len);*/
		}
		else {
			printf("%s(%d):%s ERROR(%d)!!! recvfrom ret = %d\r\n", 
				__FILE__, __LINE__, __FUNCTION__, WSAGetLastError(),ret);
			Sleep(2000);
		}
		
	}
	closesocket(sock);
	getchar();
}
	
void udp_client(bool ishost = true) {
	
	SOCKET sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		printf("%s(%d):%s:ERROR(%d)!!!\r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());
		return;
	}
	sockaddr_in server_addr, client_addr;
	int len = sizeof(client_addr);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(20000);
	/*if (bind(sock, (sockaddr*)&server_addr, sizeof(sockaddr)) == -1) {
		printf("%s(%d):%s ERROR(%d) \r\n ", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());
		return;
	}*/
	
	if (ishost) {//主客户端代码
		//printf("%s(%d):%s\r\\n", __FILE__, __LINE__, __FUNCTION__);
		//std::string msg = "hello world";
		//int ret = sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&server_addr,sizeof(sockaddr));
		//if (ret > 0) {
		//	printf("%s(%d):%s client send to server sunccessfully ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
		//	ret = recvfrom(sock, const_cast<char*>(msg.c_str()), msg.size(), 0, (sockaddr*)&client_addr, &len);
		//	//printf("%s(%d):%s ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
		//	if (ret > 0) {
		//		printf("%s(%d):%s, client ip : %08X , port:%d\r\n", 
		//			__FILE__, __LINE__, __FUNCTION__, client_addr.sin_addr.s_addr, (ntohs)(client_addr.sin_port));
		//		printf("%s(%d):%s msg is %s\r\n", __FILE__, __LINE__, __FUNCTION__, msg.c_str());
		//	}
		//	else {
		//		printf("%s(%d):%s recvfrom  client ERROR(%d) \r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());;
		//	}
		//}
		//else {
		//	printf("%s(%d):%s send to server ERROR(%d)\r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());
		//}
		std::string msg = "hello world";
		int ret = sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&server_addr, sizeof(sockaddr));//client1向客户端发送消息
		if (ret > 0) {
			msg.resize(1024);
			memset((char*)msg.c_str(), 0, 1024);
			ret = recvfrom(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)&client_addr, &len);//这次接收到到的应该是client2的消息
			if (ret > 0) {
				printf("%s(%d):%s, client2 ip : %08X , port:%d\r\n",
					__FILE__, __LINE__, __FUNCTION__, client_addr.sin_addr.s_addr, (ntohs)(client_addr.sin_port));
				printf("%s\r\n", msg.c_str());
			}
			else {
				printf("%s(%d):%s get from cleint2  ERROR(%d)\r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());
			}
		}
	}
		else {//从客户端代码
			Sleep(100);//client2后发送
			//printf("%s(%d):%s\r\\n", __FILE__, __LINE__, __FUNCTION__);
			//std::string msg = "hello world";
			//int ret = sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&server_addr, sizeof(sockaddr));
			//if (ret > 0) {
			//	printf("%s(%d):%s client send to server sunccessfully ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
			//	ret = recvfrom(sock, const_cast<char*>(msg.c_str()), msg.size(), 0, (sockaddr*)&client_addr, &len);
			//	//printf("%s(%d):%s ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
			//	if (ret > 0) {
			//		printf("%s(%d):%s, client ip : %08X , port:%d\r\n",
			//			__FILE__, __LINE__, __FUNCTION__, client_addr.sin_addr.s_addr, (ntohs)(client_addr.sin_port));
			//		printf("%s(%d):%s msg is %s\r\n", __FILE__, __LINE__, __FUNCTION__, msg.c_str());
			//	}
			//	else {
			//		printf("%s(%d):%s recvfrom  client ERROR(%d) \r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());;
			//	}
			//}
			//else {
			//	printf("%s(%d):%s send to server ERROR(%d)\r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());
			//}
			std::string msg = "hello world";
			int ret = sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&server_addr, sizeof(sockaddr));//client2向服务端发送消息
			if (ret > 0) {
				msg.resize(1024);
				memset((char*)msg.c_str(), 0, 1024);
				ret = recvfrom(sock, const_cast<char*>(msg.c_str()), msg.size(), 0, (sockaddr*)&client_addr, &len);
				if (ret > 0) {
					sockaddr_in* client1_addr = (sockaddr_in*)(msg.c_str());
					printf("%s(%d):%s, client1 ip : %08X , port:%d\r\n",
						__FILE__, __LINE__, __FUNCTION__, client1_addr->sin_addr.s_addr, (ntohs)(client1_addr->sin_port));
					std::string msg1 = "hello, I am client2";
					ret = sendto(sock, msg1.c_str(), msg1.size(), 0, (sockaddr*)client1_addr, sizeof(sockaddr));//client2向client1发送消息
					if (ret > 0) {
						printf("client2向client1发送消息\r\n");
					}
					else {
						printf("%s(%d):%sclient2 failed to send the msg to client1, error(%d)\r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());
					}
				}
				closesocket(sock);
			}
		}
	}


int main(int argc, char* argv[])
{
	
	initsock();
	if (!CEdoyunTool::Init()) return 1;
	
	if (argc == 1) {
		char wstrDir[MAX_PATH];
		GetCurrentDirectoryA(MAX_PATH, wstrDir);
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		memset(&si, 0, sizeof(si));
		memset(&pi, 0, sizeof(pi));
		std::string strCmd = argv[0];
		std::string strCmd1 = (std::string)argv[0]+ " 1";
		BOOL bRet = CreateProcessA(NULL, (LPSTR)strCmd1.c_str(), NULL, NULL, FALSE, 0, NULL, wstrDir,  &si, &pi);
		if (bRet) {
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			TRACE("进程ID:%d\r\n", pi.dwProcessId);
			TRACE("线程ID:%d\r\n", pi.dwThreadId);
			strCmd1 += " 2";
			bRet = CreateProcessA(NULL, (LPSTR)strCmd1.c_str(), NULL, NULL, FALSE, 0, NULL, wstrDir, &si, &pi);
			if (bRet) {
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
				TRACE("进程ID:%d\r\n", pi.dwProcessId);
				TRACE("线程ID:%d\r\n", pi.dwThreadId);
				udp_server();
			}
		}
	}
	else if (argc == 2) {//主客户端
		udp_client();
	}
	else {

		udp_client(false);//从客户端
	}
		
	//iocp();
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
	clearsock();
	return 0;
}



