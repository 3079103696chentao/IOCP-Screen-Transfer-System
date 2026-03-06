#include "pch.h"
#include "ClientSocket.h"

//CClientSocket Client;
CClientSocket* CClientSocket::m_instance = nullptr;
CClientSocket* pClient = CClientSocket::getInstance();
CClientSocket::CHelper CClientSocket::m_helper;

void CClientSocket::threadEntry(void* arg) {
	CClientSocket* thiz = (CClientSocket*)arg;
	thiz->threadFunc();
	_endthread();
}

void CClientSocket::threadFunc() {
	if (InitSocket() == false) {
		return;
	}
	std::string strBuffer;
	strBuffer.resize(BUFFER_SIZE);
	char* pBuffer = (char*)strBuffer.c_str();
	int index = 0;
	while (m_socket != INVALID_SOCKET) {
		if (m_lstSend.size() > 0) {
			CPacket& head = m_lstSend.front();
			if (Send(head) == false) {
				TRACE("发送失败！！！\r\n");
				continue;
			}
			if (m_mapAck.find(head.hEvent) == m_mapAck.end()) {
				m_mapAck.insert({ head.hEvent, std::list<CPacket>() });
			}
			int length = recv(m_socket, pBuffer + index, BUFFER_SIZE - index, 0);
			if (length > 0 || index > 0) {
				size_t size = (size_t)index;
				CPacket pack((BYTE*)pBuffer, size);
				
				if (size > 0) {
					//TODO:通知对应事件
					pack.hEvent = head.hEvent;
					m_mapAck[pack.hEvent].push_back(pack);
					SetEvent(head.hEvent);
				}
			}
			else{
				CloseSocket();
			}
			m_lstSend.pop_front();
		}
		else {
			Sleep(1);
		}
	}
}