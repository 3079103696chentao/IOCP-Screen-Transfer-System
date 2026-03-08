#include "pch.h"
#include "ClientSocket.h"

//CClientSocket Client;
CClientSocket* CClientSocket::m_instance = nullptr;
CClientSocket* pClient = CClientSocket::getInstance();
CClientSocket::CHelper CClientSocket::m_helper;
CClientSocket::CClientSocket():
	m_nIP(INADDR_ANY), m_nPort(0), m_socket(INVALID_SOCKET),
	m_nThreadID(ERROR_INVALID_THREAD_ID), m_hThread(INVALID_HANDLE_VALUE)
{
	if (InitSockEnv() == false)
	{
		MessageBox(NULL, _T("无法初始化套接字环境,请检查网络设置"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
		exit(0);
	}

	struct {
		UINT message;
		MSGFUNC func;
	}funcs[] = {
		{WM_SEND_PACK,&CClientSocket::SendPack },
		{(UINT)-1, nullptr}
	};

	for (int i = 0; funcs[i].message != -1; i++) {
		m_mapFunc.insert(m_mapFunc.end(),
			{ funcs[i].message, funcs[i].func });
	}

	m_hEventInvoke = CreateEvent(NULL, true, FALSE, NULL);

	m_hThread = (HANDLE)_beginthreadex(NULL, 0,
		&CClientSocket::threadEntry, this, 0, &m_nThreadID);
	TRACE(" thread Entry begins m_ThreadID = %d\r\n", m_nThreadID);

	if (WaitForSingleObject(m_hEventInvoke, 100) == WAIT_TIMEOUT) {
		TRACE("网络消息处理线程启动失败了");
	}

	CloseHandle(m_hEventInvoke);

	m_buffer.resize(BUFFER_SIZE);

	memset(m_buffer.data(), 0, BUFFER_SIZE);

}

CClientSocket::~CClientSocket() {

	if (m_socket != INVALID_SOCKET) {
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}
	WSACleanup();
}

CClientSocket* CClientSocket::getInstance()
{
	if (m_instance == nullptr)
	{
		m_instance = new CClientSocket();
	}
	return m_instance;
}

void CClientSocket::realseInstance()
{
	if (m_instance != nullptr)
	{
		CClientSocket* tmp = m_instance;
		m_instance = nullptr;
		delete tmp;
	}
}

bool CClientSocket::InitSockEnv()
{
	//套接字初始化
	WSADATA data;
	if (WSAStartup(MAKEWORD(1, 1), &data) != 0)
	{
		return false;
	}
	return true;
}

bool CClientSocket::InitSocket()
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
	serv_adr.sin_addr.s_addr = htonl(m_nIP);
	serv_adr.sin_port = htons(m_nPort);
	if (serv_adr.sin_addr.s_addr == INADDR_NONE) {
		AfxMessageBox(_T("指定的ip地址，不存在！"));
		return false;
	}
	int ret = connect(m_socket, (sockaddr*)&serv_adr, sizeof(serv_adr));
	if (ret == -1) {
		AfxMessageBox(_T("连接失败"));
		TRACE("连接失败：%d %s\r\n", WSAGetLastError(), CEdoyunTool::GetErrorInfo(WSAGetLastError()).c_str());
		return false;
	}

	return true;
}

bool CClientSocket::Send(const char* pData, size_t nSize)
{
	if (m_socket == -1) return false;
	return send(m_socket, pData, nSize, 0) > 0;
}

bool CClientSocket::Send(const CPacket& pack) {
	TRACE("m_socket = %d\r\n", m_socket);
	if (m_socket == -1) return false;
	std::string strOut;
	return send(m_socket, pack.Data(strOut), pack.Size(), 0) > 0;
}
//函数内部调用PostThreadMessage()函数来通知线程函数threadFunc()的GetMessage()
bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed) {
	
	UINT nMode = isAutoClosed ? CSM_AUTOCLOSE : 0;

	std::string strOut;
	bool ret =  PostThreadMessage(m_nThreadID, WM_SEND_PACK,
		(WPARAM)new PACKET_DATA(pack.Data(strOut), pack.Size(), isAutoClosed), (LPARAM)hWnd);
	TRACE("PostThreadMessage result is %d ,m_nThreadID is %d\r\n", ret, m_nThreadID);
	return ret;
}

//bool CClientSocket::SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks) {
//	int flag = 0;
//
//	if (m_socket == INVALID_SOCKET)
//	{
//		flag = 1;
//	}
//
//	if (InitSocket() == false) return false;
//
//	if (flag == 1) {
//		_beginthread(&CClientSocket::threadEntry, 0, this);
//	}
//
//	m_lstSend.push_back(pack);
//
//	WaitForSingleObject(pack.hEvent, INFINITE);
//
//	auto it = m_mapAck.find(pack.hEvent);
//	if (it != m_mapAck.end()) {
//		lstPacks.insert(lstPacks.end(), it->second.begin(), it->second.end());
//		m_mapAck.erase(it);//去除收到的应答包
//		return true;
//	}
//	return false;
//}
// 
//发送数据到，并接收服务端的数据，并且SendMessage到对应的界面回调函数中
void CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	int ret = InitSocket();
	if (ret < 0) {
		TRACE("网络连接失败\r\n");
		return;
	}
	static size_t index = 0;
	char* buffer = m_buffer.data();
	//定义消息数据结构(数据、长度、模式)，回调消息的数据结构（HANDLE MSSAGE）
	 while(m_socket!=INVALID_SOCKET) {
		 if ((PACKET_DATA*)wParam == nullptr) {
			 TRACE("PACKET_DATA为空\r\n");
			 break;
		 }
        PACKET_DATA packet_data = *(PACKET_DATA*)wParam;
		delete (PACKET_DATA*)wParam;

		int ret = send(m_socket, packet_data.strData.c_str(),
			packet_data.strData.size(), 0);

		if (ret > 0) {
			size_t length = recv(m_socket, buffer+index, BUFFER_SIZE - index, 0);
			TRACE("recv length = %d, index = %d\r\n", length, index);
			if (length <= 0 && index <= 0) {
				CloseSocket();
				break;
			}
			index += length;
			length = index;
			CPacket pack((BYTE*)buffer, length);
			if (length > 0) {
				HWND hWnd = (HWND)lParam;
				::SendMessage(hWnd, WM_SEND_PACK_ACK, WPARAM(&pack), LPARAM(10));
				memmove(buffer, buffer + length, index - length);
				index -= length;
				TRACE("Send Message success cmd = %d\r\n", pack.sCmd);
			}
			else {
			 	TRACE("CPacket 解析失败 length =0\r\n");
				break;
			}
			if (packet_data.nMode) {
				CloseSocket();
				break;
			}
		}
		else {
			TRACE("send to serve failed m_socket = %d, ret = %d\r\n", m_socket, ret);
			CloseSocket();
			break;
		}
	}
}

int CClientSocket::DealCommand()
{
	if (m_socket == -1) return -1;
	char* buffer = m_buffer.data();
	//memset(buffer, 0, m_buffer.size());
	static size_t index = 0;
	while (true)
	{
		size_t len = recv(m_socket, buffer + index, BUFFER_SIZE - index, 0);
		TRACE("recv len:%d,  index :%d\r\n", len, index);
		if (len <= 0 && index <= 0) {
			return -1;
		}
		CEdoyunTool::Dump((BYTE*)buffer, len);
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

bool CClientSocket::GetFilePath(std::string& strPath) { //获取文件列表
	if (m_packet.sCmd >= 2 && m_packet.sCmd <= 4) {
		strPath = m_packet.strData;
		return true;
	}
	return false;
}

bool CClientSocket::GetMouseEvent(MOUSEEV& mouse) {
	if (m_packet.sCmd == 5) {
		memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
		return true;
	}
	return false;
}

CPacket& CClientSocket::GetPacket() {
	return m_packet;
}

void CClientSocket::CloseSocket() {
	closesocket(m_socket);
	m_socket = INVALID_SOCKET;
}

void CClientSocket::UpdateAddress(int nIP, int nPort) {
	if (nIP != m_nIP || nPort != m_nPort) {
		m_nIP = nIP;
		m_nPort = nPort;
	}
}

unsigned __stdcall CClientSocket::threadEntry(void* arg) {
	CClientSocket* thiz = (CClientSocket*)arg;
	thiz->threadFunc();
	_endthreadex(0);
	return 0;
}


//void CClientSocket::threadFunc() {
//
//	if (InitSocket() == false) {
//		return;
//	}
//	std::string strBuffer;
//	strBuffer.resize(BUFFER_SIZE);
//	char* pBuffer = (char*)strBuffer.c_str();
//	size_t index = 0;
//	while (m_socket != INVALID_SOCKET) {
//		if (m_lstSend.size() > 0) {
//			TRACE("m_lstSend.size = %d \r\n", m_lstSend.size());
//			m_lock.lock();
//			CPacket& head = m_lstSend.front();
//			m_lock.unlock();
//			TRACE("thread func 要发送 cmd = %d ,并进行等待\r\n", head.sCmd);
//			if (Send(head) == false) {
//				TRACE("发送失败！！！\r\n");
//				continue;
//			}
//			if (m_mapAck.find(head.hEvent) == m_mapAck.end()) {
//				m_mapAck.insert({ head.hEvent, std::list<CPacket>() });
//			}
//
//			size_t length = recv(m_socket, pBuffer + index, BUFFER_SIZE - index, 0);
//			if (length > 0 || index > 0) {
//
//				index += length;
//				length = index;
//				CPacket pack((BYTE*)pBuffer, length);
//				
//				if (length > 0) {
//					//TODO:通知对应事件
//					pack.hEvent = head.hEvent;
//					m_lock.lock();
//					m_mapAck[pack.hEvent].push_back(pack);
//					m_lock.unlock();
//					TRACE("在mememove之前 index = %d, length = %d\r\n", index, length);
//					memmove(pBuffer, pBuffer + length, BUFFER_SIZE - length);
//					index -= length;
//					SetEvent(head.hEvent);
//					
//				}
//			}
//			else{
//				break;
//				CloseSocket();
//			}
//			m_lock.lock();
//			m_lstSend.pop_front();
//			m_lock.unlock();
//		}
//		else {
//			Sleep(1);
//		}
//	}
//	CloseSocket();
//}

void CClientSocket::threadFunc()
{
	SetEvent(m_hEventInvoke);
	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		TRACE("Get Message : %08X \r\n", msg.message);
		auto it = m_mapFunc.find(msg.message);
		if (it != m_mapFunc.end()) {
			TRACE("The message is found sunccess\r\n");
			(this->*it->second)(msg.message, 
				msg.wParam, msg.lParam);
		}
	}
}
