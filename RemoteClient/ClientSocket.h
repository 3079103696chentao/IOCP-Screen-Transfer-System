#pragma once

#include "pch.h"
#include "framework.h"

#include <condition_variable>
#include <deque>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define WM_SEND_PACK (WM_USER + 1)
#define WM_SEND_PACK_ACK (WM_USER + 2)

#pragma pack(push)
#pragma pack(1)

class CPacket
{
public:
	CPacket() : sHead(0), nLength(0), sCmd(0), sSum(0) {}
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
		nLength = static_cast<DWORD>(nSize + 4);
		sCmd = nCmd;
		if (nSize > 0) {
			strData.resize(nSize);
			memcpy((void*)strData.c_str(), pData, nSize);
		}
		else {
			strData.clear();
		}
		sSum = 0;
		for (size_t j = 0; j < strData.size(); ++j) {
			sSum += BYTE(strData[j]) & 0xFF;
		}
	}
	CPacket(const CPacket& pack) {
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		strData = pack.strData;
		sSum = pack.sSum;
	}
	CPacket(const BYTE* pData, size_t& nSize) {
		size_t i = 0;
		for (; i < nSize; ++i) {
			if (*(WORD*)(pData + i) == 0xFEFF) {
				sHead = *(WORD*)(pData + i);
				i += 2;
				break;
			}
		}
		if (i + 4 + 2 + 2 > nSize) {
			nSize = 0;
			return;
		}
		nLength = *(DWORD*)(pData + i);
		i += 4;
		if (nLength + i > nSize) {
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + i);
		i += 2;
		if (nLength > 4) {
			strData.resize(nLength - 4);
			memcpy((void*)strData.c_str(), pData + i, nLength - 4);
			i += nLength - 4;
		}
		sSum = *(WORD*)(pData + i);
		i += 2;
		WORD sum = 0;
		for (size_t j = 0; j < strData.size(); ++j) {
			sum += BYTE(strData[j]) & 0xFF;
		}
		if (sum == sSum) {
			nSize = i;
			return;
		}
		nSize = 0;
	}
	~CPacket() {}
	CPacket& operator=(const CPacket& pack) {
		if (this != &pack) {
			sHead = pack.sHead;
			nLength = pack.nLength;
			sCmd = pack.sCmd;
			strData = pack.strData;
			sSum = pack.sSum;
		}
		return *this;
	}
	int Size() const {
		return static_cast<int>(nLength + 6);
	}
	const char* Data(std::string& strOut) const {
		strOut.resize(nLength + 6);
		BYTE* pBuffer = (BYTE*)strOut.c_str();
		*(WORD*)pBuffer = sHead;
		pBuffer += 2;
		*(DWORD*)pBuffer = nLength;
		pBuffer += 4;
		*(WORD*)pBuffer = sCmd;
		pBuffer += 2;
		memcpy(pBuffer, strData.c_str(), strData.size());
		pBuffer += strData.size();
		*(WORD*)pBuffer = sSum;
		return strOut.c_str();
	}

public:
	WORD sHead;
	DWORD nLength;
	WORD sCmd;
	std::string strData;
	WORD sSum;
};

#pragma pack(pop)

typedef struct MouseEvent {
	MouseEvent() {
		nAction = 0;
		nButton = static_cast<WORD>(-1);
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction;
	WORD nButton;
	POINT ptXY;
} MOUSEEV, * PMOUSEEV;

typedef struct file_info {
	file_info() {
		IsInvalid = FALSE;
		IsDirectory = -1;
		HasNext = TRUE;
		memset(szFileName, 0, sizeof(szFileName));
	}
	BOOL IsInvalid;
	BOOL IsDirectory;
	BOOL HasNext;
	char szFileName[256];
} FILEINFO, * PFILEINFO;

enum {
	CSM_AUTOCLOSE = 1,
};

struct PacketData {
	std::string strData;
	UINT nMode;
	WPARAM wParam;
	HWND hWnd;
	WORD nCmd;
	bool replaceableMove;

	PacketData() : nMode(0), wParam(0), hWnd(NULL), nCmd(0), replaceableMove(false) {}
	PacketData(const std::string& data, UINT mode, HWND targetWnd, WPARAM param, WORD cmd, bool canReplaceMove)
		: strData(data), nMode(mode), wParam(param), hWnd(targetWnd), nCmd(cmd), replaceableMove(canReplaceMove) {}
};

std::string GetErrInfo(int wsaErrCode);
void Dump(BYTE* pData, size_t nSize);

class CClientSocket
{
public:
	static CClientSocket* getInstance() {
		if (m_instance == NULL) {
			m_instance = new CClientSocket();
			TRACE("CClientSocket size is %d\r\n", sizeof(*m_instance));
		}
		return m_instance;
	}

	bool SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed = true, WPARAM wParam = 0);
	int DealCommand() {
		return -1;
	}
	CPacket& GetPacket() {
		return m_packet;
	}
	void CloseSocket();
	void UpdateAddress(int nIP, int nPort);

private:
	enum class ChannelType {
		Task,
		Screen,//- 6 -> Screen
		Control //- 5 / 7 / 8 -> Control
	};

	struct ChannelState {
		ChannelType type;
		std::string name;
		std::thread worker;
		std::mutex mutex;
		std::condition_variable cv;
		std::deque<PacketData> queue;
		std::vector<char> buffer;
		SOCKET socket;
		bool stop;

		ChannelState(ChannelType t = ChannelType::Task, const char* channelName = "")
			: type(t), name(channelName), socket(INVALID_SOCKET), stop(false) {}
	};

private:
	CClientSocket();
	CClientSocket(const CClientSocket& ss);
	CClientSocket& operator=(const CClientSocket& ss);
	~CClientSocket();

	BOOL InitSockEnv();
	void StartChannel(ChannelState& channel);
	void StopChannel(ChannelState& channel);
	void ChannelWorker(ChannelState& channel);
	ChannelState& GetChannel(ChannelType type);
	ChannelType SelectChannel(WORD cmd) const;
	bool Connect(ChannelState& channel);
	bool SendBuffer(ChannelState& channel, const std::string& buffer);
	bool ReceiveResponses(ChannelState& channel, const PacketData& request);
	void Disconnect(ChannelState& channel);
	void PostAck(const PacketData& request, CPacket* packet, LPARAM result);
	void StorePacket(const CPacket& packet);
	bool IsReplaceableMoveCommand(const CPacket& pack) const;

private:
	enum {
		BUFFER_SIZE = 4096000
	};

	std::mutex m_addrLock;
	std::mutex m_packetLock;
	int m_nIP;
	int m_nPort;
	CPacket m_packet;
	ChannelState m_taskChannel;
	ChannelState m_screenChannel;
	ChannelState m_controlChannel;

private:
	static void releaseInstance() {
		TRACE("CClientSocket has been called!\r\n");
		if (m_instance != NULL) {
			CClientSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
			TRACE("CClientSocket has released!\r\n");
		}
	}
	static CClientSocket* m_instance;
	class CHelper {
	public:
		CHelper() {
			CClientSocket::getInstance();
		}
		~CHelper() {
			CClientSocket::releaseInstance();
		}
	};
	static CHelper m_helper;
};
