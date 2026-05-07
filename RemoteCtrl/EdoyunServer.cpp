#include "pch.h"
#include "EdoyunServer.h"

#include <algorithm>

namespace {
constexpr DWORD kAddressBufferLength = sizeof(sockaddr_in) + 16;
constexpr int kPendingAcceptCount = 8;
constexpr size_t kInitialRecvBuffer = 256 * 1024;
}

EdoyunServer::IoContext::IoContext()
	: operation(IoOperation::Stop),
	  wsabuf{} {
	ZeroMemory(&overlapped, sizeof(overlapped));
	wsabuf.buf = buffer.data();
	wsabuf.len = 0;
}

void EdoyunServer::IoContext::Reset(IoOperation op)
{
	ZeroMemory(&overlapped, sizeof(overlapped));
	operation = op;
	wsabuf.buf = buffer.data();
	wsabuf.len = 0;
}

EdoyunServer::Session::Session(EdoyunServer* serverPtr, SOCKET s)
	: server(serverPtr),
	  socket(s),
	  sendOffset(0),
	  sendInFlight(false),
	  closing(false),
	  closeAfterFlush(false),
	  primaryCommand(0),
	  lane(CommandLane::Task) {
	ZeroMemory(&localAddr, sizeof(localAddr));
	ZeroMemory(&remoteAddr, sizeof(remoteAddr));
	recvCache.reserve(kInitialRecvBuffer);
}

EdoyunServer::Session::~Session()
{
	if (socket != INVALID_SOCKET) {
		closesocket(socket);
		socket = INVALID_SOCKET;
	}
}

EdoyunServer::CommandExecutor::CommandExecutor(const char* executorName)
	: name(executorName),
	  stopRequested(false) {
}

EdoyunServer::EdoyunServer(const std::string& ip, unsigned short port)
	: m_ip(ip),
	  m_port(port),
	  m_listenSocket(INVALID_SOCKET),
	  m_iocp(NULL),
	  m_started(false),
	  m_stopping(false),
	  m_acceptEx(nullptr),
	  m_getAcceptExSockaddrs(nullptr),
	  m_taskExecutor("task"),
	  m_screenExecutor("screen"),
	  m_controlExecutor("control") {
}

EdoyunServer::~EdoyunServer()
{
	StopService();
}

bool EdoyunServer::StartService()
{
	if (m_started.load()) {
		return true;
	}

	m_stopping.store(false);

	if (!InitializeWinsock()) {
		return false;
	}
	if (!CreateListenSocket()) {
		StopService();
		return false;
	}
	if (!BindAndListen()) {
		StopService();
		return false;
	}
	if (!CreateCompletionPort()) {
		StopService();
		return false;
	}
	if (!LoadExtensionFunctions()) {
		StopService();
		return false;
	}
	if (!StartCommandExecutors()) {
		StopService();
		return false;
	}
	if (!StartWorkerThreads()) {
		StopService();
		return false;
	}

	for (int i = 0; i < kPendingAcceptCount; ++i) {
		if (!PostAccept()) {
			StopService();
			return false;
		}
	}

	m_started.store(true);
	TRACE("IOCP server started on %s:%d\r\n", m_ip.c_str(), m_port);
	return true;
}

void EdoyunServer::StopService()
{
	if (m_stopping.exchange(true)) {
		return;
	}

	for (CommandExecutor* executor : { &m_taskExecutor, &m_screenExecutor, &m_controlExecutor }) {
		{
			std::lock_guard<std::mutex> lock(executor->mutex);
			executor->stopRequested = true;
		}
		executor->cv.notify_all();
	}

	if (m_listenSocket != INVALID_SOCKET) {
		closesocket(m_listenSocket);
		m_listenSocket = INVALID_SOCKET;
	}

	std::vector<std::shared_ptr<Session>> sessions;
	{
		std::lock_guard<std::mutex> lock(m_sessionMutex);
		for (auto& item : m_sessions) {
			sessions.push_back(item.second);
		}
		m_sessions.clear();
	}
	for (auto& session : sessions) {
		CloseSession(session);
	}

	if (m_iocp != NULL) {
		for (size_t i = 0; i < m_iocpThreads.size(); ++i) {
			PostQueuedCompletionStatus(m_iocp, 0, 0, NULL);
		}
	}

	for (auto& worker : m_iocpThreads) {
		if (worker.joinable()) {
			worker.join();
		}
	}
	m_iocpThreads.clear();

	for (CommandExecutor* executor : { &m_taskExecutor, &m_screenExecutor, &m_controlExecutor }) {
		if (executor->worker.joinable()) {
			executor->worker.join();
		}
	}

	m_acceptContexts.clear();

	if (m_iocp != NULL) {
		CloseHandle(m_iocp);
		m_iocp = NULL;
	}

	WSACleanup();
	m_started.store(false);
}

bool EdoyunServer::InitializeWinsock()
{
	WSADATA wsaData;
	const int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0) {
		TRACE("WSAStartup failed: %d\r\n", ret);
		return false;
	}
	return true;
}

bool EdoyunServer::CreateListenSocket()
{
	m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_listenSocket == INVALID_SOCKET) {
		TRACE("WSASocket(listen) failed: %d\r\n", WSAGetLastError());
		return false;
	}

	BOOL reuse = TRUE;
	setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
	return true;
}

bool EdoyunServer::BindAndListen()
{
	sockaddr_in addr;
	ZeroMemory(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_port);

	if (m_ip == "0.0.0.0") {
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else {
		const int ret = InetPtonA(AF_INET, m_ip.c_str(), &addr.sin_addr);
		if (ret != 1) {
			TRACE("InetPtonA failed for %s\r\n", m_ip.c_str());
			return false;
		}
	}

	if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
		TRACE("bind failed: %d\r\n", WSAGetLastError());
		return false;
	}
	if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		TRACE("listen failed: %d\r\n", WSAGetLastError());
		return false;
	}
	return true;
}

bool EdoyunServer::CreateCompletionPort()
{
	m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (m_iocp == NULL) {
		TRACE("CreateIoCompletionPort failed: %lu\r\n", GetLastError());
		return false;
	}
	return AssociateSocket(m_listenSocket, reinterpret_cast<ULONG_PTR>(this));
}

bool EdoyunServer::LoadExtensionFunctions()
{
	GUID acceptExGuid = WSAID_ACCEPTEX;
	GUID getSockAddrGuid = WSAID_GETACCEPTEXSOCKADDRS;
	DWORD bytes = 0;

	const int ret1 = WSAIoctl(
		m_listenSocket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&acceptExGuid,
		sizeof(acceptExGuid),
		&m_acceptEx,
		sizeof(m_acceptEx),
		&bytes,
		NULL,
		NULL);
	if (ret1 == SOCKET_ERROR || m_acceptEx == nullptr) {
		TRACE("Load AcceptEx failed: %d\r\n", WSAGetLastError());
		return false;
	}

	const int ret2 = WSAIoctl(
		m_listenSocket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&getSockAddrGuid,
		sizeof(getSockAddrGuid),
		&m_getAcceptExSockaddrs,
		sizeof(m_getAcceptExSockaddrs),
		&bytes,
		NULL,
		NULL);
	if (ret2 == SOCKET_ERROR || m_getAcceptExSockaddrs == nullptr) {
		TRACE("Load GetAcceptExSockaddrs failed: %d\r\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool EdoyunServer::StartWorkerThreads()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	const DWORD workerCount = (std::max<DWORD>)(2, info.dwNumberOfProcessors);
	try {
		for (DWORD i = 0; i < workerCount; ++i) {
			m_iocpThreads.emplace_back(&EdoyunServer::IocpWorkerLoop, this);
		}
	}
	catch (...) {
		TRACE("Failed to create IOCP worker threads\r\n");
		return false;
	}
	return true;
}

bool EdoyunServer::StartCommandExecutors()
{
	for (CommandExecutor* executor : { &m_taskExecutor, &m_screenExecutor, &m_controlExecutor }) {
		{
			std::lock_guard<std::mutex> lock(executor->mutex);
			executor->stopRequested = false;
			std::queue<CommandTask> empty;
			executor->queue.swap(empty);
		}
	}

	try {
		m_taskExecutor.worker = std::thread(&EdoyunServer::CommandWorkerLoop, this, &m_taskExecutor);
		m_screenExecutor.worker = std::thread(&EdoyunServer::CommandWorkerLoop, this, &m_screenExecutor);
		m_controlExecutor.worker = std::thread(&EdoyunServer::CommandWorkerLoop, this, &m_controlExecutor);
	}
	catch (...) {
		TRACE("Failed to create command executors\r\n");
		return false;
	}
	return true;
}

bool EdoyunServer::PostAccept(IoContext* context)
{
	std::shared_ptr<IoContext> holder;
	if (context == nullptr) {
		holder = std::make_shared<IoContext>();
		m_acceptContexts.push_back(holder);
		context = holder.get();
	}

	std::shared_ptr<Session> session = CreateSession();
	if (!session) {
		return false;
	}

	context->Reset(IoOperation::Accept);
	context->owner = session;

	DWORD bytes = 0;
	const BOOL ok = m_acceptEx(
		m_listenSocket,
		session->socket,
		context->buffer.data(),
		0,
		kAddressBufferLength,
		kAddressBufferLength,
		&bytes,
		&context->overlapped);

	if (!ok) {
		const int error = WSAGetLastError();
		if (error != ERROR_IO_PENDING) {
			TRACE("AcceptEx failed: %d\r\n", error);
			context->owner.reset();
			return false;
		}
	}
	return true;
}

bool EdoyunServer::AssociateSocket(SOCKET sock, ULONG_PTR completionKey)
{
	HANDLE ret = CreateIoCompletionPort(reinterpret_cast<HANDLE>(sock), m_iocp, completionKey, 0);
	if (ret == NULL) {
		TRACE("AssociateSocket failed: %lu\r\n", GetLastError());
		return false;
	}
	return true;
}

std::shared_ptr<EdoyunServer::Session> EdoyunServer::CreateSession(SOCKET existing)
{
	SOCKET sock = existing;
	if (sock == INVALID_SOCKET) {
		sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (sock == INVALID_SOCKET) {
			TRACE("WSASocket(session) failed: %d\r\n", WSAGetLastError());
			return nullptr;
		}
	}
	return std::make_shared<Session>(this, sock);
}

void EdoyunServer::RegisterSession(const std::shared_ptr<Session>& session)
{
	std::lock_guard<std::mutex> lock(m_sessionMutex);
	m_sessions[session->socket] = session;
}

void EdoyunServer::RemoveSession(SOCKET sock)
{
	std::lock_guard<std::mutex> lock(m_sessionMutex);
	m_sessions.erase(sock);
}

bool EdoyunServer::PostRecv(const std::shared_ptr<Session>& session)
{
	std::lock_guard<std::mutex> lock(session->mutex);
	if (session->closing) {
		return false;
	}

	session->recvContext.Reset(IoOperation::Recv);
	session->recvContext.owner = session;
	session->recvContext.wsabuf.len = static_cast<ULONG>(session->recvContext.buffer.size());
	DWORD flags = 0;
	DWORD bytes = 0;
	const int ret = WSARecv(
		session->socket,
		&session->recvContext.wsabuf,
		1,
		&bytes,
		&flags,
		&session->recvContext.overlapped,
		NULL);
	if (ret == SOCKET_ERROR) {
		const int error = WSAGetLastError();
		if (error != WSA_IO_PENDING) {
			TRACE("WSARecv failed: %d\r\n", error);
			session->recvContext.owner.reset();
			return false;
		}
	}
	return true;
}

bool EdoyunServer::PostSend(const std::shared_ptr<Session>& session)
{
	std::lock_guard<std::mutex> lock(session->mutex);
	if (session->closing || session->sendQueue.empty()) {
		session->sendInFlight = false;
		session->sendOffset = 0;
		return false;
	}

	std::vector<char>& current = session->sendQueue.front();
	session->sendContext.Reset(IoOperation::Send);
	session->sendContext.owner = session;
	session->sendContext.wsabuf.buf = current.data() + session->sendOffset;
	session->sendContext.wsabuf.len = static_cast<ULONG>(current.size() - session->sendOffset);

	DWORD bytes = 0;
	DWORD flags = 0;
	const int ret = WSASend(
		session->socket,
		&session->sendContext.wsabuf,
		1,
		&bytes,
		flags,
		&session->sendContext.overlapped,
		NULL);
	if (ret == SOCKET_ERROR) {
		const int error = WSAGetLastError();
		if (error != WSA_IO_PENDING) {
			TRACE("WSASend failed: %d\r\n", error);
			session->sendContext.owner.reset();
			session->sendInFlight = false;
			return false;
		}
	}
	return true;
}

bool EdoyunServer::EnqueueSend(const std::shared_ptr<Session>& session, std::vector<char>&& data)
{
	bool needKick = false;
	{
		std::lock_guard<std::mutex> lock(session->mutex);
		if (session->closing) {
			return false;
		}
		session->sendQueue.push_back(std::move(data));
		if (!session->sendInFlight) {
			session->sendInFlight = true;
			session->sendOffset = 0;
			needKick = true;
		}
	}
	if (needKick) {
		return PostSend(session);
	}
	return true;
}

void EdoyunServer::CloseSession(const std::shared_ptr<Session>& session)
{
	if (!session) {
		return;
	}

	bool alreadyClosing = false;
	SOCKET sock = INVALID_SOCKET;
	{
		std::lock_guard<std::mutex> lock(session->mutex);
		alreadyClosing = session->closing;
		if (!alreadyClosing) {
			session->closing = true;
			sock = session->socket;
			session->sendQueue.clear();
			session->sendOffset = 0;
			session->sendInFlight = false;
		}
	}
	if (alreadyClosing) {
		return;
	}

	RemoveSession(sock);
	if (sock != INVALID_SOCKET) {
		shutdown(sock, SD_BOTH);
		closesocket(sock);
		session->socket = INVALID_SOCKET;
	}
}

void EdoyunServer::IocpWorkerLoop()
{
	while (true) {
		DWORD transferred = 0;
		ULONG_PTR completionKey = 0;
		OVERLAPPED* overlapped = NULL;
		const BOOL ok = GetQueuedCompletionStatus(m_iocp, &transferred, &completionKey, &overlapped, INFINITE);
		const DWORD errorCode = ok ? ERROR_SUCCESS : GetLastError();

		if (overlapped == NULL) {
			if (m_stopping.load()) {
				break;
			}
			continue;
		}

		IoContext* context = reinterpret_cast<IoContext*>(overlapped);
		switch (context->operation) {
		case IoOperation::Accept:
			HandleAccept(context, transferred, errorCode);
			break;
		case IoOperation::Recv:
			HandleRecv(context, transferred, errorCode);
			break;
		case IoOperation::Send:
			HandleSend(context, transferred, errorCode);
			break;
		default:
			break;
		}
	}
}

void EdoyunServer::CommandWorkerLoop(CommandExecutor* executor)
{
	while (true) {
		CommandTask task;
		{
			std::unique_lock<std::mutex> lock(executor->mutex);
			executor->cv.wait(lock, [executor]() {
				return executor->stopRequested || !executor->queue.empty();
			});

			if (executor->stopRequested && executor->queue.empty()) {
				break;
			}

			task = std::move(executor->queue.front());
			executor->queue.pop();
		}

		HandleCommand(*executor, task);
	}
}

void EdoyunServer::DispatchPackets(const std::shared_ptr<Session>& session)
{
	std::vector<CPacket> packets;
	{
		std::lock_guard<std::mutex> lock(session->mutex);
		while (!session->recvCache.empty()) {
			size_t packetSize = session->recvCache.size();
			CPacket packet(reinterpret_cast<const BYTE*>(session->recvCache.data()), packetSize);
			if (packetSize == 0) {
				break;
			}
			packets.push_back(packet);
			session->recvCache.erase(
				session->recvCache.begin(),
				session->recvCache.begin() + static_cast<std::ptrdiff_t>(packetSize));
		}
	}

	for (const auto& packet : packets) {
		QueueCommand(session, packet);
	}
}

void EdoyunServer::QueueCommand(const std::shared_ptr<Session>& session, const CPacket& packet)
{
	const CommandLane lane = ClassifyCommand(packet.sCmd);
	{
		std::lock_guard<std::mutex> lock(session->mutex);
		if (session->closing) {
			return;
		}
		if (session->primaryCommand == 0) {
			session->primaryCommand = packet.sCmd;
			session->lane = lane;
			session->closeAfterFlush = true;
		}
	}

	CommandExecutor& executor = SelectExecutor(lane);
	{
		std::lock_guard<std::mutex> lock(executor.mutex);
		executor.queue.push(CommandTask{ session, packet, lane });
	}
	executor.cv.notify_one();
}

void EdoyunServer::HandleAccept(IoContext* context, DWORD, DWORD errorCode)
{
	std::shared_ptr<Session> session = context->owner;
	context->owner.reset();

	if (!session) {
		PostAccept(context);
		return;
	}

	if (errorCode != ERROR_SUCCESS) {
		TRACE("Accept completion failed: %lu\r\n", errorCode);
		CloseSession(session);
		if (!m_stopping.load()) {
			PostAccept(context);
		}
		return;
	}

	if (setsockopt(
		session->socket,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		reinterpret_cast<const char*>(&m_listenSocket),
		sizeof(m_listenSocket)) == SOCKET_ERROR) {
		TRACE("SO_UPDATE_ACCEPT_CONTEXT failed: %d\r\n", WSAGetLastError());
		CloseSession(session);
		if (!m_stopping.load()) {
			PostAccept(context);
		}
		return;
	}

	sockaddr* localSockAddr = nullptr;
	sockaddr* remoteSockAddr = nullptr;
	int localLen = 0;
	int remoteLen = 0;
	m_getAcceptExSockaddrs(
		context->buffer.data(),
		0,
		kAddressBufferLength,
		kAddressBufferLength,
		&localSockAddr,
		&localLen,
		&remoteSockAddr,
		&remoteLen);

	if (localSockAddr != nullptr && localLen >= static_cast<int>(sizeof(sockaddr_in))) {
		memcpy(&session->localAddr, localSockAddr, sizeof(sockaddr_in));
	}
	if (remoteSockAddr != nullptr && remoteLen >= static_cast<int>(sizeof(sockaddr_in))) {
		memcpy(&session->remoteAddr, remoteSockAddr, sizeof(sockaddr_in));
	}

	if (!AssociateSocket(session->socket, reinterpret_cast<ULONG_PTR>(session.get()))) {
		CloseSession(session);
		if (!m_stopping.load()) {
			PostAccept(context);
		}
		return;
	}

	RegisterSession(session);
	TRACE("Accepted client socket=%llu\r\n", static_cast<unsigned long long>(session->socket));

	if (!PostRecv(session)) {
		CloseSession(session);
	}

	if (!m_stopping.load()) {
		PostAccept(context);
	}
}

void EdoyunServer::HandleRecv(IoContext* context, DWORD transferred, DWORD errorCode)
{
	std::shared_ptr<Session> session = context->owner;
	context->owner.reset();

	if (!session) {
		return;
	}

	if (errorCode != ERROR_SUCCESS || transferred == 0) {
		TRACE("Recv completion ended, error=%lu bytes=%lu\r\n", errorCode, transferred);
		CloseSession(session);
		return;
	}

	{
		std::lock_guard<std::mutex> lock(session->mutex);
		session->recvCache.insert(
			session->recvCache.end(),
			context->buffer.data(),
			context->buffer.data() + transferred);
	}

	DispatchPackets(session);
	if (!PostRecv(session)) {
		CloseSession(session);
	}
}

void EdoyunServer::HandleSend(IoContext* context, DWORD transferred, DWORD errorCode)
{
	std::shared_ptr<Session> session = context->owner;
	context->owner.reset();

	if (!session) {
		return;
	}

	if (errorCode != ERROR_SUCCESS || transferred == 0) {
		TRACE("Send completion ended, error=%lu bytes=%lu\r\n", errorCode, transferred);
		CloseSession(session);
		return;
	}

	bool hasMore = false;
	bool shouldClose = false;
	{
		std::lock_guard<std::mutex> lock(session->mutex);
		if (session->sendQueue.empty()) {
			session->sendInFlight = false;
			session->sendOffset = 0;
			shouldClose = session->closeAfterFlush;
		}
		else {
			session->sendOffset += transferred;
			std::vector<char>& current = session->sendQueue.front();
			if (session->sendOffset >= current.size()) {
				session->sendQueue.pop_front();
				session->sendOffset = 0;
			}

			hasMore = !session->sendQueue.empty();
			if (!hasMore) {
				session->sendInFlight = false;
				shouldClose = session->closeAfterFlush;
			}
		}
	}

	if (hasMore) {
		if (!PostSend(session)) {
			CloseSession(session);
		}
		return;
	}

	if (shouldClose) {
		CloseSession(session);
	}
}

void EdoyunServer::HandleCommand(CommandExecutor& executor, const CommandTask& task)
{
	if (!task.session) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(task.session->mutex);
		if (task.session->closing) {
			return;
		}
	}

	std::list<CPacket> responses;
	CPacket request = task.packet;
	const int ret = executor.command.ExcuteCommand(request.sCmd, responses, request);
	if (ret != 0) {
		TRACE("[%s] command %d finished with ret=%d\r\n", executor.name.c_str(), request.sCmd, ret);
	}

	if (responses.empty()) {
		CloseSession(task.session);
		return;
	}

	for (auto& response : responses) {
		std::vector<char> data = SerializePacket(response);
		if (!EnqueueSend(task.session, std::move(data))) {
			CloseSession(task.session);
			return;
		}
	}
}

EdoyunServer::CommandLane EdoyunServer::ClassifyCommand(WORD cmd) const
{
	switch (cmd) {
	case 5:
	case 7:
	case 8:
		return CommandLane::Control;
	case 6:
		return CommandLane::Screen;
	default:
		return CommandLane::Task;
	}
}

EdoyunServer::CommandExecutor& EdoyunServer::SelectExecutor(CommandLane lane)
{
	switch (lane) {
	case CommandLane::Screen:
		return m_screenExecutor;
	case CommandLane::Control:
		return m_controlExecutor;
	case CommandLane::Task:
	default:
		return m_taskExecutor;
	}
}

const char* EdoyunServer::LaneName(CommandLane lane)
{
	switch (lane) {
	case CommandLane::Screen:
		return "screen";
	case CommandLane::Control:
		return "control";
	case CommandLane::Task:
	default:
		return "task";
	}
}

std::vector<char> EdoyunServer::SerializePacket(CPacket& packet)
{
	const int size = packet.Size();
	std::vector<char> data(size);
	const char* serialized = packet.Data();
	memcpy(data.data(), serialized, size);
	return data;
}
