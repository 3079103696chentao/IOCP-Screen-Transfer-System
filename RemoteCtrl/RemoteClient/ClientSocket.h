
#pragma once
#include"pch.h"
#include "framework.h"
#include<string>
#include<vector>
#pragma pack(push)
#pragma pack(1)
class CPacket {
public:
	CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
	//打包
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
		nLength = (DWORD)nSize + 4;
		sCmd = nCmd;
		if (nSize > 0) {
			strData.resize(nSize);
			memcpy((void*)strData.c_str(), pData, nSize);
		}
		else {
			strData.clear();
		}
		sSum = 0;
		for (size_t j = 0; j < strData.size(); j++) {
			sSum += BYTE(strData[j] & 0xFF);
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
		for (; i < nSize; i++) {
			if (*(WORD*)(pData + i) == 0xFEFF) {

				//TRACE("%02X\r\n", *(WORD*)(pData + i));
				sHead = *(WORD*)(pData + i);
				
				i += 2; //解析到包头了，所以i+=2；跳过包头了，继续往后找，如果没有数据，还是要返回
				break;
			}
		}
		if (i + 4 + 2 + 2 > nSize) {//包数据可能不全，或者包未能全部接收到
			nSize = 0;
			return;
		}
		nLength = *(DWORD*)(pData + i); i += 4;
		if (nLength + i > nSize) {//包未完全接受到，数据不全
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + i); i += 2;
		if (nLength > 4) {
			strData.resize(nLength - 2 - 2);

			/*for (size_t j = 0; j < nLength - 4; j++) {
				TRACE("%02X\r\n", ((BYTE*)(pData + i))[j]);
			}*/

			memcpy((void*)strData.c_str(), pData + i, nLength - 4);
			i += nLength - 4;
			//TRACE("%d\r\n", sizeof(bool));
			//TRACE("%02X\r\n", *(WORD*)(pData + i));
			TRACE("%s\r\n", strData.c_str()+3);
		}
		sSum = *(WORD*)(pData + i); i += 2;
		WORD sum = 0;
		for (size_t j = 0; j < strData.size(); j++) {
			sum += BYTE(strData[j] & 0xFF);
		}
		if (sum == sSum) {
			nSize = i;//还要包括前面废弃的数据不能是 nLength + 4+2
			return;
		}
		nSize = 0;

	}
	~CPacket() = default;
	CPacket& operator=(CPacket pack) noexcept {
		swap(pack);
		return *this;
	}
	void swap(CPacket& pack) noexcept {
		std::swap(pack.sHead, sHead);
		std::swap(pack.nLength, nLength);
		std::swap(pack.sCmd, sCmd);
		std::swap(pack.strData, strData);
		std::swap(pack.sSum, sSum);
	}
	int Size() {//包数据的大小
		return nLength + 6;
	}
	const char* Data() {
		strOut.resize(nLength + 6);
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData += 2;
		*(DWORD*)pData = nLength; pData += 4;
		*(WORD*)pData = sCmd; pData += 2;
		memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
		*(WORD*)pData = sSum;
		return strOut.c_str();
	}
public:
	WORD sHead;//固定位FE FF
	DWORD nLength;//包长度（从控制命令开始，到和校验结束）
	WORD sCmd;//控制命令
	std::string strData;//包数据
	WORD sSum;//和校验
	std::string strOut; //整个包的数据
};
#pragma pack(pop)
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
	bool IsInvalid; //是否有效
	bool isDirectory; //是否为目录 0否 1是
	bool HaNext;//是否还有后续 0 没有 1 有
	char szFileName[260];//文件名
}FILEINFO, * PFILEINFO;

inline std::string GetErrorInfo(int wsaErrCode) {
	std::string ret;
	LPVOID lpMsgBuf = NULL;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		wsaErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	ret = (char*)lpMsgBuf;
	return ret;
}
inline void Dump(BYTE* pData, size_t nSize) {
	std::string strOut;
	for (size_t i = 0; i < nSize; i++) {
		char buf[8] = "";
		if (i > 0 && (i % 16 == 0)) strOut += "\n";
		snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF); //不足两位补0，和0xFF，确保只取低八位
		strOut += buf;
	}
	strOut += "\n";
	OutputDebugStringA(strOut.c_str());
}
class CClientSocket
{
public:
	/*static CClientSocket getInstance()
	{
		static CClientSocket serve;
		return serve;
	}*/
	static CClientSocket* getInstance()
	{
		if (m_instance == nullptr)
		{
			m_instance = new CClientSocket();
		}
		return m_instance;
	}

	bool InitSocket(int nIP, int nPort)
	{
		if (m_socket != INVALID_SOCKET) CloseSocket();

		m_socket = socket(PF_INET, SOCK_STREAM, 0);
		if (m_socket == -1) return false;
		sockaddr_in serv_adr;
		memset(&serv_adr, 0, sizeof(serv_adr));
		serv_adr.sin_family = AF_INET;
		//TRACE("addr %08X nIP %08X\r\n", inet_addr("127.0.0.1"), nIP);//\r是回车，将光标移动到当前行的开头
		//\n换行，将光标移动到下一行
		//serv_adr.sin_addr.s_addr = inet_addr("127.0.0.1");
		serv_adr.sin_addr.s_addr = htonl(nIP);
		serv_adr.sin_port = htons(nPort);
		if (serv_adr.sin_addr.s_addr == INADDR_NONE) {
			AfxMessageBox(_T("指定的ip地址，不存在！"));
			return false;
		}
		int ret = connect(m_socket, (sockaddr*)&serv_adr, sizeof(serv_adr));
		if (ret == -1) {
			AfxMessageBox(_T("连接失败"));
			TRACE("连接失败：%d %s\r\n", WSAGetLastError(),GetErrorInfo(WSAGetLastError()).c_str());
			return false;
		}

		return true;
	}
	
#define BUFFER_SIZE 4096
	int DealCommand()
	{
		if (m_socket == -1) return -1;
		char* buffer = m_buffer.data();
		//memset(buffer, 0, m_buffer.size());
		static size_t index = 0;
		while (true)
		{
			size_t len = recv(m_socket, buffer + index, BUFFER_SIZE - index, 0);
			TRACE("recv len:%d,  index :%d\r\n", len, index);
			if (len <= 0 && index <=0) {
				return -1;
			}
			Dump((BYTE*)buffer, len);
			index += len;
			len = index; //等于buffer中数据的大小
			m_packet = CPacket((BYTE*)buffer, len);
			if (len > 0) {
				memmove(buffer, buffer + len, BUFFER_SIZE - len);
				index -= len;
				//TRACE("index :%d\r\n", index);
				return m_packet.sCmd;
			}

		}
	}
	bool Send(const char* pData, size_t nSize)
	{
		if (m_socket == -1) return false;
		return send(m_socket, pData, nSize, 0) > 0;
	}
	bool Send(CPacket& pack) {
		TRACE("m_socket = %d\r\n", m_socket);
		if (m_socket == -1) return false;
		return send(m_socket, pack.Data(), pack.Size(), 0) > 0;
	}
	bool GetFilePath(std::string& strPath) { //获取文件列表
		if (m_packet.sCmd >= 2 && m_packet.sCmd <= 4) {
			strPath = m_packet.strData;
			return true;
		}
		return false;
	}
	bool GetMouseEvent(MOUSEEV& mouse) {
		if (m_packet.sCmd == 5) {
			memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
			return true;
		}
		return false;
	}

	CPacket& GetPacket() {
		return m_packet;
	}
	void CloseSocket() {
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}
private:
	std::vector<char>m_buffer;
	SOCKET m_socket;
	CPacket m_packet;
	//赋值运算符重载函数
	CClientSocket& operator=(const CClientSocket& ss) = delete;
	//拷贝构造函数
	CClientSocket(const CClientSocket& ss) = delete;
	//构造函数
	CClientSocket() {
		m_socket = INVALID_SOCKET;

		if (InitSockEnv() == false)
		{
			MessageBox(NULL, _T("无法初始化套接字环境,请检查网络设置"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
			exit(0);
		}
		m_buffer.resize(BUFFER_SIZE);
		memset(m_buffer.data(), 0, BUFFER_SIZE);
	}
	//析构函数
	~CClientSocket() {
	
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
			CClientSocket* tmp = m_instance;
			m_instance = nullptr;
			delete tmp;
		}
	}
	static  CClientSocket* m_instance;

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


