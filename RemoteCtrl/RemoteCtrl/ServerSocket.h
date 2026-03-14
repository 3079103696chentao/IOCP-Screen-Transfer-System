#pragma once
#include"pch.h"
#include "framework.h"
#include"Packet.h"
#include<list>

#define BUFFER_SIZE 4096

typedef struct MouseEvent {
	MouseEvent() {
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction; //点击、移动、双击
	WORD nButton; //左键、右键、中键
	POINT ptXY;//坐标
}MOUSEEV, * PMOUSEEV;

typedef struct file_info {
	file_info() {
		IsInvalid = false;
		memset(szFileName, 0, sizeof(szFileName));
		isDirectory = -1;
		HaNext = true;
	}
	BOOL IsInvalid; //是否有效
	BOOL isDirectory; //是否为目录 0否 1是
	BOOL HaNext;//是否还有后续 0 没有 1 有
	char szFileName[260];//文件名
}FILEINFO, * PILEINFO;

typedef void(*SOCKET_CALLBACK)(void*, int, std::list<CPacket>&, CPacket& inPacket);


class CServerSocket
{
public:

	static CServerSocket* getInstance()
	{
		if (m_instance == nullptr)
		{
			m_instance = new CServerSocket();
		}
		return m_instance;
	}

	int Run(SOCKET_CALLBACK callback, void* arg, short port = 9527) {
		//1.进度的可控性 2.对接的方便性 3.可行性评估，提早暴露风险
		   // TODO: socket bind listen accept read write close
		bool ret = InitSocket(port);
		if (ret == false) return -1;
		std::list<CPacket>lstPacket;
		m_callback = callback;
		m_arg = arg;
		int count = 0;
		while (true) {
			if (AcceptClient() == false) {
				if (count >= 3) {
					return -2;
				}
				count++;
			}
			int ret = DealCommand();
			if (ret > 0) {
				m_callback(m_arg, ret, lstPacket, m_packet);
				while (lstPacket.size() > 0) {
					Send(lstPacket.front());
					lstPacket.pop_front();
				}
			}
			CloseClient();
		}

	}

protected:

	bool InitSocket(short port = 9527)
	{
		m_socket = socket(PF_INET, SOCK_STREAM, 0);
		if (m_socket == -1) return false;
		sockaddr_in serv_adr;
		memset(&serv_adr, 0, sizeof(serv_adr));
		serv_adr.sin_family = AF_INET;
		serv_adr.sin_addr.s_addr = INADDR_ANY;
		serv_adr.sin_port = htons(port);
		//绑定
		if (bind(m_socket, (sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
			return false;
		}
		//TODO
		if (listen(m_socket, 1) == -1) {
			return false;
		}
		return true;
	}

	bool AcceptClient()
	{
		TRACE("Enter Accept\r\n");
		sockaddr_in client_adr;
		int cli_sz = sizeof(client_adr);

		m_client = accept(m_socket, (sockaddr*)&client_adr, &cli_sz);
		TRACE("m_client = %d\r\n", m_client);

		if (m_client == -1) return false;

		return true;
	}

	int DealCommand()
	{
		TRACE("在DealCommand里面\r\n");
		if (m_client == -1) return -1;
		char* buffer = new char[BUFFER_SIZE];
		if (buffer == NULL) {
			TRACE("内存不足\r\n");
			return -2;
		}
		memset(buffer, 0, sizeof(buffer));
		size_t index = 0;
		while (true)
		{
			size_t len = (size_t)recv(m_client, buffer + index, BUFFER_SIZE - index, 0);
			if (len <= 0 && index == 0) {
				delete[]buffer;
				return -1;
			}
			index += len;
			len = index; //等于buffer中数据的大小
			m_packet = CPacket((BYTE*)buffer, len);
			if (len > 0) {
				memmove(buffer, buffer + len, BUFFER_SIZE - len);
				index -= len;
				delete[]buffer;
				return m_packet.sCmd;
			}

		}
		delete[]buffer;
		return -1;
	}

	bool Send(const char* pData, size_t nSize)
	{
		if (m_client == -1) return false;
		return send(m_client, pData, nSize, 0) > 0;
	}

	bool Send(CPacket& pack) {
		if (m_client == -1) return false;
		return send(m_client, pack.Data(), pack.Size(), 0) > 0;
	}

	void CloseClient() {
		if (m_client != INVALID_SOCKET) {
			closesocket(m_client);
			m_client = INVALID_SOCKET;
		}
	}
private:
	SOCKET_CALLBACK m_callback;
	void* m_arg;
	SOCKET m_client;
	SOCKET m_socket;
	CPacket m_packet;
	//赋值运算符重载函数
	CServerSocket& operator=(const CServerSocket& ss) = delete;
	//拷贝构造函数
	CServerSocket(const CServerSocket& ss) = delete;
	//构造函数
	CServerSocket() {
		m_socket = INVALID_SOCKET;
		m_client = INVALID_SOCKET;

		if (InitSockEnv() == false)
		{
			MessageBox(NULL, _T("无法初始化套接字环境,请检查网络设置"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
			exit(0);
		}
	}
	//析构函数
	~CServerSocket() {
		if (m_client != INVALID_SOCKET) {
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
		if (WSAStartup(MAKEWORD(2, 0), &data) != 0) //支持重叠结构
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


