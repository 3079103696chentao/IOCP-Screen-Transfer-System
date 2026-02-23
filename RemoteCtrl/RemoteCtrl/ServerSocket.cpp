#include "pch.h"
#include "ServerSocket.h"

//CServerSocket server;
CServerSocket* CServerSocket:: m_instance = nullptr;
CServerSocket* pServer = CServerSocket::getInstance();
CServerSocket::CHelper CServerSocket::m_helper;