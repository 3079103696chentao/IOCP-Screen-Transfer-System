#include "pch.h"
#include "ClientSocket.h"

CClientSocket* CClientSocket::m_instance = NULL;
CClientSocket::CHelper CClientSocket::m_helper;

std::string GetErrInfo(int wsaErrCode)
{
	std::string ret;
	LPVOID lpMsgBuf = NULL;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		wsaErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0,
		NULL);
	ret = (char*)lpMsgBuf;
	LocalFree(lpMsgBuf);
	return ret;
}

void Dump(BYTE* pData, size_t nSize)
{
	std::string strOut;
	for (size_t i = 0; i < nSize; ++i) {
		char buf[8] = "";
		if (i > 0 && (i % 16 == 0)) {
			strOut += "\n";
		}
		snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
		strOut += buf;
	}
	strOut += "\n";
	OutputDebugStringA(strOut.c_str());
}

CClientSocket::CClientSocket(const CClientSocket& ss)
	: m_nIP(ss.m_nIP),
	  m_nPort(ss.m_nPort),
	  m_taskChannel(ChannelType::Task, "task"),
	  m_screenChannel(ChannelType::Screen, "screen"),
	  m_controlChannel(ChannelType::Control, "control")
{
}

CClientSocket::CClientSocket()
	: m_nIP(INADDR_ANY),
	  m_nPort(0),
	  m_taskChannel(ChannelType::Task, "task"),
	  m_screenChannel(ChannelType::Screen, "screen"),
	  m_controlChannel(ChannelType::Control, "control")
{
	if (InitSockEnv() == FALSE) {
		MessageBox(NULL, _T("初始化网络环境失败，程序无法继续运行"), _T("初始化失败"), MB_OK | MB_ICONERROR);
		exit(0);
	}

	StartChannel(m_taskChannel);
	StartChannel(m_screenChannel);
	StartChannel(m_controlChannel);
}

CClientSocket::~CClientSocket()
{
	StopChannel(m_taskChannel);
	StopChannel(m_screenChannel);
	StopChannel(m_controlChannel);
	WSACleanup();
}

BOOL CClientSocket::InitSockEnv()
{
	WSADATA data;
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
		return FALSE;
	}
	return TRUE;
}

void CClientSocket::StartChannel(ChannelState& channel)
{
	channel.buffer.resize(BUFFER_SIZE);
	channel.worker = std::thread([this, &channel]() {
		ChannelWorker(channel);
	});
}

void CClientSocket::StopChannel(ChannelState& channel)
{
	{
		std::lock_guard<std::mutex> lock(channel.mutex);
		channel.stop = true;
	}
	channel.cv.notify_all();
	Disconnect(channel);
	if (channel.worker.joinable()) {
		channel.worker.join();
	}
}

void CClientSocket::ChannelWorker(ChannelState& channel)
{
	while (true) {
		PacketData request;
		{
			std::unique_lock<std::mutex> lock(channel.mutex);
			channel.cv.wait(lock, [&channel]() {
				return channel.stop || !channel.queue.empty();
			});

			if (channel.stop && channel.queue.empty()) {
				break;
			}

			request = channel.queue.front();
			channel.queue.pop_front();
		}

		Disconnect(channel);
		if (!Connect(channel)) {
			TRACE("[%s] connect failed\r\n", channel.name.c_str());
			PostAck(request, NULL, -2);
			continue;
		}

		if (!SendBuffer(channel, request.strData)) {
			TRACE("[%s] send failed\r\n", channel.name.c_str());
			Disconnect(channel);
			PostAck(request, NULL, -1);
			continue;
		}

		ReceiveResponses(channel, request);
		Disconnect(channel);
	}
}

CClientSocket::ChannelState& CClientSocket::GetChannel(ChannelType type)
{
	switch (type) {
	case ChannelType::Screen:
		return m_screenChannel;
	case ChannelType::Control:
		return m_controlChannel;
	case ChannelType::Task:
	default:
		return m_taskChannel;
	}
}

CClientSocket::ChannelType CClientSocket::SelectChannel(WORD cmd) const
{
	switch (cmd) {
	case 5:
	case 7:
	case 8:
		return ChannelType::Control;
	case 6:
		return ChannelType::Screen;
	default:
		return ChannelType::Task;
	}
}

bool CClientSocket::Connect(ChannelState& channel)
{
	int currentIp = INADDR_ANY;
	int currentPort = 0;
	{
		std::lock_guard<std::mutex> lock(m_addrLock);
		currentIp = m_nIP;
		currentPort = m_nPort;
	}

	SOCKET sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		return false;
	}

	sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(currentIp);
	servAddr.sin_port = htons(currentPort);

	if (servAddr.sin_addr.s_addr == INADDR_NONE) {
		closesocket(sock);
		return false;
	}

	if (connect(sock, (sockaddr*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR) {
		TRACE("[%s] connect error=%d %s\r\n", channel.name.c_str(), WSAGetLastError(), GetErrInfo(WSAGetLastError()).c_str());
		closesocket(sock);
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(channel.mutex);
		if (channel.stop) {
			closesocket(sock);
			return false;
		}
		channel.socket = sock;
	}
	return true;
}

bool CClientSocket::SendBuffer(ChannelState& channel, const std::string& buffer)
{
	SOCKET sock = INVALID_SOCKET;
	{
		std::lock_guard<std::mutex> lock(channel.mutex);
		sock = channel.socket;
	}

	size_t sent = 0;
	while (sent < buffer.size()) {
		const int ret = send(sock, buffer.c_str() + sent, static_cast<int>(buffer.size() - sent), 0);
		if (ret <= 0) {
			return false;
		}
		sent += static_cast<size_t>(ret);
	}
	return true;
}

bool CClientSocket::ReceiveResponses(ChannelState& channel, const PacketData& request)
{
	SOCKET sock = INVALID_SOCKET;
	{
		std::lock_guard<std::mutex> lock(channel.mutex);
		sock = channel.socket;
	}

	size_t used = 0;
	bool hasResponse = false;
	while (sock != INVALID_SOCKET) {
		const int length = recv(sock, channel.buffer.data() + used, BUFFER_SIZE - static_cast<int>(used), 0);
		if (length > 0) {
			used += static_cast<size_t>(length);
			while (used > 0) {
				size_t packetSize = used;
				CPacket packet((BYTE*)channel.buffer.data(), packetSize);
				if (packetSize == 0) {
					break;
				}

				hasResponse = true;
				StorePacket(packet);
				PostAck(request, new CPacket(packet), static_cast<LPARAM>(request.wParam));
				if ((request.nMode & CSM_AUTOCLOSE) != 0) {
					return true;
				}

				memmove(channel.buffer.data(), channel.buffer.data() + packetSize, used - packetSize);
				used -= packetSize;
			}
			continue;
		}

		if (length == 0) {
			if (!hasResponse) {
				PostAck(request, new CPacket(request.nCmd, NULL, 0), 1);
			}
			return hasResponse;
		}

		TRACE("[%s] recv failed error=%d\r\n", channel.name.c_str(), WSAGetLastError());
		if (!hasResponse) {
			PostAck(request, NULL, -1);
		}
		return false;
	}

	return hasResponse;
}

void CClientSocket::Disconnect(ChannelState& channel)
{
	SOCKET sock = INVALID_SOCKET;
	{
		std::lock_guard<std::mutex> lock(channel.mutex);
		sock = channel.socket;
		channel.socket = INVALID_SOCKET;
	}

	if (sock != INVALID_SOCKET) {
		shutdown(sock, SD_BOTH);
		closesocket(sock);
	}
}

void CClientSocket::PostAck(const PacketData& request, CPacket* packet, LPARAM result)
{
	if (request.hWnd != NULL) {
		::PostMessage(request.hWnd, WM_SEND_PACK_ACK, reinterpret_cast<WPARAM>(packet), result);
		return;
	}

	if (packet != NULL) {
		delete packet;
	}
}

void CClientSocket::StorePacket(const CPacket& packet)
{
	std::lock_guard<std::mutex> lock(m_packetLock);
	m_packet = packet;
}

bool CClientSocket::IsReplaceableMoveCommand(const CPacket& pack) const
{
	if (pack.sCmd != 5 || pack.strData.size() != sizeof(MOUSEEV)) {
		return false;
	}

	const MOUSEEV* mouse = reinterpret_cast<const MOUSEEV*>(pack.strData.c_str());
	return mouse->nButton == 4 && mouse->nAction == 4;
}

bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed, WPARAM wParam)
{
	std::string strOut;
	pack.Data(strOut);
	const UINT mode = isAutoClosed ? CSM_AUTOCLOSE : 0;
	ChannelState& channel = GetChannel(SelectChannel(pack.sCmd));
	PacketData request(strOut, mode, hWnd, wParam, pack.sCmd, IsReplaceableMoveCommand(pack));

	{
		std::lock_guard<std::mutex> lock(channel.mutex);
		if (channel.stop) {
			return false;
		}
		if (request.replaceableMove && !channel.queue.empty() && channel.queue.back().replaceableMove) {
			channel.queue.back() = request;
		}
		else {
			channel.queue.push_back(request);
		}
	}
	channel.cv.notify_one();
	return true;
}

void CClientSocket::CloseSocket()
{
	Disconnect(m_taskChannel);
	Disconnect(m_screenChannel);
	Disconnect(m_controlChannel);
}

void CClientSocket::UpdateAddress(int nIP, int nPort)
{
	std::lock_guard<std::mutex> lock(m_addrLock);
	m_nIP = nIP;
	m_nPort = nPort;
}
