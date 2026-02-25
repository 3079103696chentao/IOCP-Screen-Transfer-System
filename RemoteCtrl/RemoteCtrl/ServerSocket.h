#pragma once
#include"pch.h"
#include "framework.h"

#pragma pack(push)
#pragma pack(1)
class CPacket {
public:
	CPacket():sHead(0), nLength(0), sCmd(0), sSum(0) {}
	//打包
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
		nLength = (DWORD)nSize + 4;
		sCmd = nCmd;
		if (nSize > 0) {
			strData.resize(nSize);
			memcpy(&strData[0], pData, nSize);
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
				sHead = *(WORD*)(pData + i);
				i += 2; //解析到包头了，所以i+=2；跳过包头了，继续往后找，如果没有数据，还是要返回
				break;
			}
		}
		if (i + 4 + 2 + 2 >  nSize) {//包数据可能不全，或者包未能全部接收到
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
			memcpy((void*)strData.c_str(), pData + i, nLength - 4);
			i += nLength - 4;
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
	CPacket& operator=( CPacket pack) noexcept {
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


typedef struct MouseEvent{
	MouseEvent() {
		nAction = 0;
		nButton = -1; 
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction; //点击、移动、双击
	WORD nButton; //左键、右键、中键
	POINT ptXY;//坐标
}MOUSEEV,*PMOUSEEV;

typedef struct file_info {
	file_info() {
		IsInvalid = false;
		memset(szFileName, 0, sizeof(szFileName));
		isDirectory = -1;
		HaNext = true;
	}
	bool IsInvalid; //是否有效
	char szFileName[260];//文件名
	bool isDirectory; //是否为目录 0否 1是
	bool HaNext;//是否还有后续 0 没有 1 有
}FILEINFO, * PILEINFO;




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
		TRACE("Enter Accept\r\n");
		sockaddr_in client_adr;
		int cli_sz = sizeof(client_adr);

		m_client = accept(m_socket, (sockaddr*)&client_adr, &cli_sz);
		TRACE("m_client = %d\r\n", m_client);

		if (m_client == -1) return false;

		return true;
	}
#define BUFFER_SIZE 4096
	int DealCommand()
	{
		TRACE("在DealCommand里面\r\n");
		if (m_client == -1) return -1;
		char* buffer = new char[BUFFER_SIZE];
		if (buffer == NULL) {
			TRACE("内存不足\r\n");
			return -2;
		}
		memset(buffer, 0,sizeof(buffer));
		size_t index = 0;
		while (true)
		{
			size_t len = recv(m_client, buffer + index, BUFFER_SIZE-index, 0);
			if (len == -1) {
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
	bool Send( CPacket& pack) {
		if (m_client == -1) return false;
		return send(m_client, pack.Data(), pack.Size(), 0) > 0;
	}
	bool GetFilePath(std::string& strPath) { //获取文件列表
		if (m_packet.sCmd >=2 && m_packet.sCmd <=4) {
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
	void CloseClient() {
		closesocket(m_client);
		m_client = INVALID_SOCKET;
	}
private:
	SOCKET m_client;
	SOCKET m_socket;
	CPacket m_packet;
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


