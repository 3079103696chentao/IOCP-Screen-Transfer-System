#include"pch.h"
#include"EdoyunServer.h"
int EdoyunServer::threadIocp() {
	DWORD transferred;
	ULONG_PTR Completionkey;
	OVERLAPPED* lpOverlapped;
	if (GetQueuedCompletionStatus(m_hIOCP, &transferred, &Completionkey, &lpOverlapped, INFINITE)) {
		if (Completionkey != 0) {
			EdoyunOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, EdoyunOverlapped, m_overlapped);
			TRACE("pOverlapped->m_operator%d\r\n", pOverlapped->m_operator);
			pOverlapped->m_server = this;
			switch (pOverlapped->m_operator) {
			case EAccept: {
				ACCEPTOVERLAPPED* pAccept = (ACCEPTOVERLAPPED*)pOverlapped;
				m_pool.DispatchWorker(pAccept->m_worker);
			}
			case ERecv: {
				RECVOVERLAPPED* pRecv = (RECVOVERLAPPED*)pOverlapped;
				m_pool.DispatchWorker(pRecv->m_worker);
				break;
			}
			case ESend: {
				SENDOVERLAPPED* pSend = (SENDOVERLAPPED*)pOverlapped;
				m_pool.DispatchWorker(pSend->m_worker);
				break;
			}
			case EError: {
				ERROROVERLAPPED* pError = (ERROROVERLAPPED*)pOverlapped;
				m_pool.DispatchWorker(pError->m_worker);
				break;
			}


			}
		}
		else {
			return -1;
		}
	}
	return 0;
}