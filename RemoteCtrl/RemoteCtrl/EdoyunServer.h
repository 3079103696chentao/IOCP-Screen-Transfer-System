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
		//TODO
	}
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
	EdoyunClient() :m_isbusy(false),m_overlapped(new ACCEPTOVERLAPPED()) {
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
	sockaddr_in& GetLocaladdr() { return m_laddr; }
	sockaddr_in& GetRemoteaddr() { return m_raddr; }
	
private:
	SOCKET m_sock;
	DWORD m_received;
	std::shared_ptr<ACCEPTOVERLAPPED>m_overlapped;
	std::vector<char>m_buffer;
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

	
	int threadIocp() {
		DWORD transferred;
		ULONG_PTR Completionkey;
		OVERLAPPED* lpOverlapped;
		if (GetQueuedCompletionStatus(m_hIOCP, &transferred, &Completionkey, &lpOverlapped, INFINITY)) {
			if (transferred != 0 && Completionkey != 0) {
				EdoyunOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, EdoyunOverlapped, m_overlapped);
				switch (pOverlapped->m_operator) {
				case EAccept: {
					ACCEPTOVERLAPPED* pAccept = (ACCEPTOVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pAccept->m_worker);
				}
				case ERecv: {
					RECVOVERLAPPED* pRecv = (RECVOVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pRecv->m_worker);
					break;
				}
				case ESend: {
					SENDOVERLAPPED* pSend = (SENDOVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pSend->m_worker);
					break;
				}
				case EError: {
					ERROROVERLAPPED* pError = (ERROROVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pError->m_worker);
					break;
				}


				}

			}
			else {
				return -1;
			}

		}
		return 0;
	}
private:
	EdoyunThreadPool m_pool;
	HANDLE m_hIOCP;
	SOCKET m_sock;
	sockaddr_in m_addr;
	std::map<SOCKET, std::shared_ptr<EdoyunClient>>m_client;
};

