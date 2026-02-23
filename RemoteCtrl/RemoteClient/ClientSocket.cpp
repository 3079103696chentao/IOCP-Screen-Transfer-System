#include "pch.h"
#include "ClientSocket.h"

//CClientSocket Client;
CClientSocket* CClientSocket::m_instance = nullptr;
CClientSocket* pClient = CClientSocket::getInstance();
CClientSocket::CHelper CClientSocket::m_helper;