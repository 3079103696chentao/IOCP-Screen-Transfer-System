#pragma once
#include "EdoyunThread.h"
#include"EdoyunQueue.h"
#include<map>
#include<memory>
#include<MSWSock.h>
#pragma warning(disable:4407)

enum EdoyunOperator {
	ENone,
	EAccept,
	ERecv,
	ESend,
	EError
};
class EdoyunServer;
class EdoyunClient;
typedef std::shared_ptr<EdoyunClient> PCLIENT;

class EdoyunOverlapped {
public:
	EdoyunOverlapped(){}
	OVERLAPPED m_overlapped;
	DWORD m_operator;//操作 参见EdoyunOperator
	std::vector<char>m_buffer;//缓冲区
	ThreadWorker m_worker;//处理函数
	EdoyunServer* m_server;
	PCLIENT m_client;//对应的客户端
	WSABUF m_wsabuffer;
};

template<EdoyunOperator>
class AcceptOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
	AcceptOverlapped() {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_operator = EAccept;
		m_buffer.resize(1024);
		m_worker = ThreadWorker(this, (FUNCTYPE)&AcceptOverlapped::AcceptWorker);
		m_server = nullptr;
	}
	int AcceptWorker() {
		INT lLength = 0, rLength = 0;
		if (*(LPDWORD)*m_client.get() > 0) {
			GetAcceptExSockaddrs((PVOID)*m_client.get(), 0,
				sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
				(sockaddr**)&(m_client->GetLocaladdr()), &lLength, //本地地址
				(sockaddr**)&(m_client->GetRemoteaddr()), &rLength); //远程地址
			int ret = WSARecv((SOCKET)*m_client, m_client->RecvWSABuffer(), 1, *m_client, &m_client->flags(), *m_client, NULL);
			if (ret == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING)) {
				//TODO:报错

			}
			if (!m_server->NewAccept()) {
				return -2;
			}
		}
		return -1;
	}
	PCLIENT m_client;
};
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;

template<EdoyunOperator>
class RecvOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
	RecvOverlapped() 
	{
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_operator = ERecv;
		m_buffer.resize(1024);
		m_worker = ThreadWorker(this, (FUNCTYPE)&RecvOverlapped::RecvWorker);
		m_server = nullptr;
	}
	int RecvWorker() {
		int ret = m_client->Recv();
		return ret;
	}
	PCLIENT m_client;
};
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;

template<EdoyunOperator>
class SendOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
	SendOverlapped() 
	{
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_operator = ESend;
		m_buffer.resize(1024);
		m_worker = ThreadWorker(this, (FUNCTYPE)&SendOverlapped::SendtWorker);
		m_server = nullptr;
	}
	int SendtWorker() {
		//TODO
		return 0;
	}
};
typedef SendOverlapped<ESend> SENDOVERLAPPED;

template<EdoyunOperator>
class ErrorOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
	ErrorOverlapped() 
	{
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_operator= EError;
		m_buffer.resize(1024);
		m_worker = ThreadWorker(this, (FUNCTYPE)&ErrorOverlapped::ErrorWorker);
		m_server = nullptr;
	}
	int ErrorWorker() {
		//TODO
	}
};
typedef ErrorOverlapped<EError> ERROROVERLAPPED;

class EdoyunClient {
public:
	EdoyunClient()
		:m_isbusy(false), m_flags(0),
		m_overlapped(new ACCEPTOVERLAPPED()),
		m_recv(new RECVOVERLAPPED()),
		m_send(new SENDOVERLAPPED())
	{
		m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		m_buffer.resize(1024);
		memset(&m_laddr, 0, sizeof(m_laddr));
		memset(&m_raddr, 0, sizeof(m_raddr));
	}

	~EdoyunClient() {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}

	void SetOverlapped(PCLIENT& ptr) {
		m_overlapped->m_client = ptr;
		m_recv->m_client = ptr;
		m_send->m_client = ptr;
	}

	operator SOCKET() {
		return m_sock;
	}

	operator PVOID() {
		return &m_buffer[0];
	}
	operator LPOVERLAPPED() {
		return &m_overlapped->m_overlapped;
	}
	operator LPDWORD() {
		return &m_received;
	}
	 LPWSABUF RecvWSABuffer() {
		return &m_recv->m_wsabuffer;
	}
	 LPWSABUF SendWSABuffer() {
		 return &m_send->m_wsabuffer;
	 }
	DWORD& flags() { return m_flags; }
	sockaddr_in& GetLocaladdr() { return m_laddr; }
	sockaddr_in& GetRemoteaddr() { return m_raddr; }
	size_t GetBuffferSize() const { return m_buffer.size(); }
	int Recv() {
		int ret = recv(m_sock, m_buffer.data() + m_used, m_buffer.size() - m_used, 0);
		if (ret <= 0) return -1;
		m_used += ret;
		//解析数据
		return 0;
	}
private:
	SOCKET m_sock;
	DWORD m_received;
	DWORD m_flags;
	std::shared_ptr<ACCEPTOVERLAPPED>m_overlapped;
	std::shared_ptr<RECVOVERLAPPED> m_recv;
	std::shared_ptr<SENDOVERLAPPED>m_send;
	std::vector<char>m_buffer;
	size_t m_used; //已经使用的缓冲区大小
	sockaddr_in m_laddr;
	sockaddr_in m_raddr;
	bool m_isbusy;
	
};



class EdoyunServer :
	public ThreadFuncBase
{
public:
	EdoyunServer(const std::string& ip = "0.0.0.0", short port = 9527) :m_pool(10) {
		m_hIOCP = INVALID_HANDLE_VALUE;
		m_sock = INVALID_SOCKET;
		m_addr.sin_family = AF_INET;
		m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
		m_addr.sin_port = htons(port);

	}
	~EdoyunServer() {}
	bool StartService() {
		CreateSocket();
		if (bind(m_sock, (sockaddr*)&m_addr, sizeof(sockaddr_in)) != 0) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			OutputDebugString("bind failed \r\n");
			return false;
		}
		if (listen(m_sock, 3) == -1) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			return false;
		}
		m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);
		CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);
		if (!m_pool.Invoke()) return false;
		m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&EdoyunServer::threadIocp));
		if (!NewAccept()) return false;
		return true;
	}

	bool NewAccept() {
		PCLIENT pClient = std::make_shared<EdoyunClient>();
		pClient->SetOverlapped(pClient);
		m_client.insert({ *pClient, pClient });

		if (AcceptEx(m_sock, *pClient, *pClient, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
			*pClient, *pClient) == false) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_hIOCP = INVALID_HANDLE_VALUE;
			return false;
		}
		return true;
	}
private:
	void CreateSocket() {
		m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		int opt = 1;
		setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	}

	
	int threadIocp();
private:
	EdoyunThreadPool m_pool;
	HANDLE m_hIOCP;
	SOCKET m_sock;
	sockaddr_in m_addr;
	std::map<SOCKET, std::shared_ptr<EdoyunClient>>m_client;
};

