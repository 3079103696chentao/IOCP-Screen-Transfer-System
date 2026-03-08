
#pragma once
#include"pch.h"
#include "framework.h"
#include<string>
#include<vector>
#include<list>
#include<map>
#include<mutex>
#include"Packet.h"
#include"EdoyunTool.h"
#define WM_SEND_PACK (WM_USER+1) //发送包数据
#ifndef WM_SEND_PACK_ACK
#define WM_SEND_PACK_ACK (WM_USER+2) //应答
#endif
#define BUFFER_SIZE 2048000


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
}FILEINFO, * PFILEINFO;

typedef struct PacketData{
	std::string strData;
	UINT nMode;
	PacketData(const char* pData, size_t length, UINT mode) {
		strData.resize(length);
		memcpy((char*)strData.c_str(), pData, length);
		nMode = mode;
	}
	PacketData(const PacketData& packet) {
		strData = packet.strData;
		nMode = packet.nMode;
	}
	PacketData& operator=(const PacketData& packet) {
		auto tmp = packet;
		swap(tmp);
		return *this;

	}
	void swap(PacketData& packet) {
		std::swap(strData, packet.strData);
		std::swap(nMode, packet.nMode);
	}
}PACKET_DATA;
enum {
	CSM_AUTOCLOSE = 1
};

class CClientSocket
{
public:
	
	static CClientSocket* getInstance();
	bool InitSocket();
	int DealCommand();
	bool SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed);
	void SendPack(UINT nMsg, WPARAM wParam/*缓冲区的值*/, LPARAM lParam/*缓冲区的长度*/);
	bool GetFilePath(std::string& strPath);
	bool GetMouseEvent(MOUSEEV& mouse);
	CPacket& GetPacket();
	void CloseSocket();
	void UpdateAddress(int nIP, int nPort);
	bool Send(const char* pData, size_t nSize);
	bool Send(const CPacket& pack);
	static unsigned __stdcall threadEntry(void* arg);
	void threadFunc();
	bool InitSockEnv();
private:

	typedef void(CClientSocket::* MSGFUNC)(UINT nMsg,
		WPARAM wParam, LPARAM lParam);
	std::map<UINT, MSGFUNC>m_mapFunc;
	int m_nIP;//地址
	int m_nPort;//端口
	SOCKET m_socket;
	UINT m_nThreadID;
	HANDLE m_hThread;
	HANDLE m_hEventInvoke;
	std::mutex m_lock;
	std::list<CPacket>m_lstSend;
	std::map<HANDLE, std::list<CPacket>>m_mapAck;
	
	std::vector<char>m_buffer;
	
	CPacket m_packet;
	static  CClientSocket* m_instance;
	
	//赋值运算符重载函数
	CClientSocket& operator=(const CClientSocket& ss) = delete;
	//拷贝构造函数
	CClientSocket(const CClientSocket& ss) = delete;
	//构造函数
	CClientSocket();
	//析构函数
	~CClientSocket();
	
	static void realseInstance();

	class CHelper {
	public:
		CHelper() {
			CClientSocket::getInstance();
		}
		~CHelper() {
			CClientSocket::realseInstance();
		}
	};
	static CHelper m_helper;
};
//extern CClientSocket pClient;


