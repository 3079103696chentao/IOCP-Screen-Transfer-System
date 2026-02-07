#pragma once
#include"pch.h"
#include "framework.h"
class CServerSocket
{
public:
	/*static CServerSocket getInstance()
	{
		static CServerSocket serve;
		return serve;
	}*/
	static CServerSocket* getInstance()
	{
		if (m_instance == nullptr)
		{
			m_instance = new CServerSocket();
		}
		return m_instance;
	}

	bool InitSocket()
	{
		m_socket = socket(PF_INET, SOCK_STREAM, 0);
		if (m_socket == -1) return false;
		sockaddr_in serv_adr;
		memset(&serv_adr, 0, sizeof(serv_adr));
		serv_adr.sin_family = AF_INET;
		serv_adr.sin_addr.s_addr = INADDR_ANY;
		serv_adr.sin_port = htons(9527);
		//绑定
		if (bind(m_socket, (sockaddr*)&serv_adr, sizeof(serv_adr)) == -1){
			return false;
		}
		//TODO
		if (listen(m_socket, 1) == -1){
			return false;
		}

		return true;
	}
	bool AcceptClient()
	{
		sockaddr_in client_adr;
		int cli_sz = sizeof(client_adr);

		m_client = accept(m_socket, (sockaddr*)&client_adr, &cli_sz);
		if (m_client == -1) return false;

		return true;
	}
	int DealCommand()
	{
		if (m_client == -1) return -1;
		char buffer[1024] = "";
		while (true)
		{
			int len = recv(m_client, buffer, sizeof(buffer), 0);
			if (len == -1) {
				return -1;
			}
			//TODO:处理命令
		}
	}
	bool Send(const char* pData, size_t nSize)
	{
		if (m_client == -1) return false;
		return send(m_client, pData, nSize, 0) > 0;
	}
private:
	SOCKET m_client;
	SOCKET m_socket;

	//赋值运算符重载函数
	CServerSocket& operator=(const CServerSocket& ss) = delete;
	//拷贝构造函数
	CServerSocket(const CServerSocket& ss) = delete;
	//构造函数
	CServerSocket(){
		m_socket = INVALID_SOCKET;
		m_client = INVALID_SOCKET;

		if (InitSockEnv() == false)
		{
			MessageBox(NULL, _T("无法初始化套接字环境,请检查网络设置"), _T("初始化错误！"),MB_OK | MB_ICONERROR);
			exit(0);
		}
	}
	//析构函数
	~CServerSocket(){
		if (m_client != INVALID_SOCKET){
			closesocket(m_client);
			m_client = INVALID_SOCKET;
		}
		if (m_socket != INVALID_SOCKET) {
			closesocket(m_socket);
			m_socket = INVALID_SOCKET;
		}
		WSACleanup();
	}
	bool InitSockEnv()
	{
		//套接字初始化
		WSADATA data;
		if (WSAStartup(MAKEWORD(1, 1), &data) != 0)
		{
			return false;
		}
		return true;
	}
	static void realseInstance()
	{
		if (m_instance != nullptr)
		{
			CServerSocket* tmp = m_instance;
			m_instance = nullptr;
			delete tmp;
		}
	}
	static  CServerSocket* m_instance;

	class CHelper {
	public:
		CHelper() {
			CServerSocket::getInstance();
		}
		~CHelper() {
			CServerSocket::realseInstance();
		}
	};
	static CHelper m_helper;
};
//extern CServerSocket pserver;

