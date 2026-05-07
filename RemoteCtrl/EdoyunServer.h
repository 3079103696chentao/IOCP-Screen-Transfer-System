#pragma once

#include "pch.h"

#include <MSWSock.h>
#include <WS2tcpip.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Command.h"

class EdoyunServer {
public:
	EdoyunServer(const std::string& ip = "0.0.0.0", unsigned short port = 9527);
	~EdoyunServer();

	bool StartService();
	void StopService();

private:
	enum class IoOperation {
		Accept,
		Recv,
		Send,
		Stop
	};

	enum class CommandLane {
		Task,
		Screen,
		Control
	};

	struct Session;

	struct IoContext {
		OVERLAPPED overlapped;
		IoOperation operation;
		std::shared_ptr<Session> owner;
		WSABUF wsabuf;
		std::array<char, 64 * 1024> buffer;

		IoContext();
		void Reset(IoOperation op);
	};

	struct Session : std::enable_shared_from_this<Session> {
		Session(EdoyunServer* server, SOCKET s);
		~Session();

		EdoyunServer* server;
		SOCKET socket;
		sockaddr_in localAddr;
		sockaddr_in remoteAddr;
		std::vector<char> recvCache;
		std::deque<std::vector<char>> sendQueue;
		size_t sendOffset;
		bool sendInFlight;
		bool closing;
		bool closeAfterFlush;
		WORD primaryCommand;
		CommandLane lane;
		std::mutex mutex;
		IoContext recvContext;
		IoContext sendContext;
	};

	struct CommandTask {
		std::shared_ptr<Session> session;
		CPacket packet;
		CommandLane lane;
	};

	struct CommandExecutor {
		explicit CommandExecutor(const char* executorName = "");

		std::string name;
		std::thread worker;
		std::mutex mutex;
		std::condition_variable cv;
		std::queue<CommandTask> queue;
		bool stopRequested;
		CCommand command;
	};

private:
	bool InitializeWinsock();
	bool CreateListenSocket();
	bool BindAndListen();
	bool CreateCompletionPort();
	bool LoadExtensionFunctions();
	bool StartWorkerThreads();
	bool StartCommandExecutors();
	bool PostAccept(IoContext* context = nullptr);
	bool AssociateSocket(SOCKET sock, ULONG_PTR completionKey);
	std::shared_ptr<Session> CreateSession(SOCKET existing = INVALID_SOCKET);
	void RegisterSession(const std::shared_ptr<Session>& session);
	void RemoveSession(SOCKET sock);

	bool PostRecv(const std::shared_ptr<Session>& session);
	bool PostSend(const std::shared_ptr<Session>& session);
	bool EnqueueSend(const std::shared_ptr<Session>& session, std::vector<char>&& data);
	void CloseSession(const std::shared_ptr<Session>& session);

	void IocpWorkerLoop();
	void CommandWorkerLoop(CommandExecutor* executor);
	void DispatchPackets(const std::shared_ptr<Session>& session);
	void QueueCommand(const std::shared_ptr<Session>& session, const CPacket& packet);

	void HandleAccept(IoContext* context, DWORD transferred, DWORD errorCode);
	void HandleRecv(IoContext* context, DWORD transferred, DWORD errorCode);
	void HandleSend(IoContext* context, DWORD transferred, DWORD errorCode);
	void HandleCommand(CommandExecutor& executor, const CommandTask& task);

	CommandLane ClassifyCommand(WORD cmd) const;
	CommandExecutor& SelectExecutor(CommandLane lane);
	static const char* LaneName(CommandLane lane);
	static std::vector<char> SerializePacket(CPacket& packet);

private:
	std::string m_ip;
	unsigned short m_port;
	SOCKET m_listenSocket;
	HANDLE m_iocp;
	std::atomic<bool> m_started;
	std::atomic<bool> m_stopping;
	LPFN_ACCEPTEX m_acceptEx;
	LPFN_GETACCEPTEXSOCKADDRS m_getAcceptExSockaddrs;

	std::vector<std::thread> m_iocpThreads;
	std::vector<std::shared_ptr<IoContext>> m_acceptContexts;

	std::mutex m_sessionMutex;
	std::unordered_map<SOCKET, std::shared_ptr<Session>> m_sessions;

	CommandExecutor m_taskExecutor;
	CommandExecutor m_screenExecutor;
	CommandExecutor m_controlExecutor;
};
