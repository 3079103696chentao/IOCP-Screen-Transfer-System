#pragma once
#include"pch.h"
#include<mutex>
#include<atomic>
#include<list>
#include"EdoyunThread.h"
template<typename T>
class CEdoyunQueue
{
public:
	enum {
		EQNone,
		EQPush,
		EQPop,
		EQSize,
		EQClear
	};
	typedef struct IocpParam {
		int nOperator;//操作
		T Data;//数据
		HANDLE hEvent;//pop操作需要的
		IocpParam(int op, const T& data, HANDLE hEve = NULL) {
			nOperator = op;
			Data = data;
			hEvent = hEve;
		}
		IocpParam() {
			nOperator = EQNone;
		}
	}PPARAM;//Post Parameter 用于投递信息的结构体

	//线程安全的队列，利用IOCP
public:
	CEdoyunQueue() {
		m_lock.store(false);
		m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
		//epoll是单线程处理的 ,完成端口映射可以允许有多个线程
		m_hThread = INVALID_HANDLE_VALUE;
		if (m_hCompletionPort != NULL) {
			m_hThread = (HANDLE)_beginthread(&CEdoyunQueue<T>::threadEntry, 0, this);
		}
	}
	virtual ~CEdoyunQueue() {
		if (m_lock.load() == true) return;
		m_lock.store(true);
		PostQueuedCompletionStatus(m_hCompletionPort, 0, NULL, NULL);
		WaitForSingleObject(m_hThread, INFINITE);
		if (m_hCompletionPort) {
			HANDLE htemp = m_hCompletionPort;
			m_hCompletionPort = NULL;
			CloseHandle(m_hCompletionPort);
		}
	}
	bool PushBack(const T& data) {
		PPARAM* pParam = new PPARAM(EQPush, data);
		if (m_lock.load() == true) {
			delete pParam;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM),
			(ULONG_PTR)pParam, NULL);
		if (ret == false) delete pParam;
		return ret;

	}
	virtual bool PopFront(T& data) {
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		PPARAM Param(EQPop, data, hEvent);
		if (m_lock.load() == true) {
			if (hEvent) CloseHandle(hEvent);
			return false;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM),
			(ULONG_PTR)&Param, NULL);
		if (ret == false) {
			CloseHandle(hEvent);
			return false;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			data = Param.Data;
		}
		return ret;
	}
	size_t Size() {

		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		PPARAM Param(EQSize, T(), hEvent);
		if (m_lock.load() == true) {
			if (hEvent) CloseHandle(hEvent);
			return -1;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM),
			(ULONG_PTR)&Param, NULL);
		if (ret == false) {
			CloseHandle(hEvent);
			return -1;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			return Param.nOperator;
		}
		return ret;
	}
	bool clear() {
		if (m_lock.load() == true) return false;
		PPARAM* pParam = new PPARAM(EQClear, T());
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort,
			sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (ret == false) delete pParam;
		return ret;

	}
protected:
	static void threadEntry(void* arg) {
		CEdoyunQueue* thiz = (CEdoyunQueue*)arg;
		thiz->threadmain();
		_endthread();
	}
	virtual void DealParam(PPARAM* pParam) {

		switch (pParam->nOperator) {
		case EQPush: {
			m_lstData.push_back(pParam->Data);
			delete pParam;
			break;
		}
		case EQPop: {
			if (m_lstData.size() > 0) {
				pParam->Data = m_lstData.front();
				m_lstData.pop_front();
			}
			if (pParam->hEvent != NULL) {
				SetEvent(pParam->hEvent);
			}
			break;
		}
		case EQSize: {
			pParam->nOperator = m_lstData.size();
			if (pParam->hEvent != NULL) {
				SetEvent(pParam->hEvent);
			}
			break;
		}
		case EQClear: {
			m_lstData.clear();
			delete pParam;
			break;
		}
		default:
			OutputDebugStringA("unkown operator!\r\n");
			break;

		}
	}
	virtual void threadmain() {
		DWORD dwTransferred = 0;
		PPARAM* pParam = nullptr;
		ULONG_PTR Completionkey = 0;
		LPOVERLAPPED pOverlapped = nullptr;//取这个变量的地址
		while (GetQueuedCompletionStatus(m_hCompletionPort, &dwTransferred, &Completionkey,
			&pOverlapped, INFINITE)) {
			if (dwTransferred == 0 && (Completionkey == NULL)) {
				/*printf("get count = %d, count0 = %d\r\n", count, count0);
				printf("thread is preparing to exit\r\n");*/
				break;
			}
			pParam = (PPARAM*)Completionkey;
			DealParam(pParam);
		}
		while (GetQueuedCompletionStatus(m_hCompletionPort, &dwTransferred, &Completionkey,
			&pOverlapped, 0)) {
			if (dwTransferred == 0 && (Completionkey == NULL)) {
				/*printf("get count = %d, count0 = %d\r\n", count, count0);
				printf("thread is preparing to exit\r\n");*/
				break;
			}
			pParam = (PPARAM*)Completionkey;
			DealParam(pParam);
		}
		HANDLE htemp = m_hCompletionPort;
		m_hCompletionPort = NULL;
		CloseHandle(m_hCompletionPort);
	}
protected:
	std::list<T>m_lstData;
	HANDLE m_hCompletionPort;
	HANDLE m_hThread;
	bool m_IsEnd;//队列正在析构
	std::atomic<bool> m_lock;
};



template<typename T>
class EdoyunSendQueue :public CEdoyunQueue<T>, public ThreadFuncBase {
public:
	typedef int(ThreadFuncBase::* EDYCALLBACK)(T& data);
	EdoyunSendQueue(ThreadFuncBase* obj, EDYCALLBACK callback)
		:CEdoyunQueue<T>(), m_base(obj), m_callback(callback) {
		m_thread.Start();
		m_thread.UpdateWorker(::ThreadWorker(this, (FUNCTYPE)&EdoyunSendQueue<T>::threadTick));
	}

	virtual ~EdoyunSendQueue(){
		m_base = nullptr;
		m_callback = nullptr;
		m_thread.Stop();
	}
protected:

	virtual bool PopFront(T& data) {
		return false;
	}
    bool PopFront() {
		//HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		typename CEdoyunQueue<T>::PPARAM* Param = new typename CEdoyunQueue<T>::PPARAM(CEdoyunQueue<T>::EQPop, T());
		if (CEdoyunQueue<T>::m_lock.load() == true) {
			delete Param;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(CEdoyunQueue<T>::m_hCompletionPort, sizeof(*Param),
			(ULONG_PTR)&Param, NULL);
		if (ret == false) {
			//CloseHandle(hEvent);
			delete Param;
			return false;
		}
		return ret;
	}
	int threadTick() {
		if (WaitForSingleObject(CEdoyunQueue<T>::m_hThread, 0) != WAIT_TIMEOUT) return 0;
		if (CEdoyunQueue<T>::m_lstData.size() > 0) {
			PopFront();
		}
		return 0;
	}
	virtual void DealParam(typename CEdoyunQueue<T>::PPARAM* pParam) {

		switch (pParam->nOperator) {
		case  CEdoyunQueue<T>::EQPush: {
			CEdoyunQueue<T>::m_lstData.push_back(pParam->Data);
			delete pParam;
			break;
		}
		case  CEdoyunQueue<T>::EQPop: {
			if (CEdoyunQueue<T>::m_lstData.size() > 0) {
				pParam->Data = CEdoyunQueue<T>::m_lstData.front();
				if ((m_base->*m_callback)(pParam->Data) == 0) {
					CEdoyunQueue<T>::m_lstData.pop_front();
				}
			}
			/*if (pParam->hEvent != NULL) {
				SetEvent(pParam->hEvent);
			}*/
			delete pParam;
			break;
		}
		case  CEdoyunQueue<T>::EQSize: {
			pParam->nOperator = CEdoyunQueue<T>::m_lstData.size();
			if (pParam->hEvent != NULL) {
				SetEvent(pParam->hEvent);
			}
			break;
		}
		case  CEdoyunQueue<T>::EQClear: {
			CEdoyunQueue<T>::m_lstData.clear();
			delete pParam;
			break;
		}
		default:
			OutputDebugStringA("unkown operator!\r\n");
			break;

		}
	}


	virtual void threadmain() {
		DWORD dwTransferred = 0;
		typename CEdoyunQueue<T>::PPARAM* pParam = nullptr;
		ULONG_PTR Completionkey = 0;
		LPOVERLAPPED pOverlapped = nullptr;//取这个变量的地址
		while (GetQueuedCompletionStatus(CEdoyunQueue<T>::m_hCompletionPort, &dwTransferred, &Completionkey,
			&pOverlapped, INFINITE)) {
			if (dwTransferred == 0 && (Completionkey == NULL)) {
				/*printf("get count = %d, count0 = %d\r\n", count, count0);
				printf("thread is preparing to exit\r\n");*/
				break;
			}
			pParam = (typename CEdoyunQueue<T>::PPARAM*)Completionkey;
			DealParam(pParam);
		}
		while (GetQueuedCompletionStatus(CEdoyunQueue<T>::m_hCompletionPort, &dwTransferred, &Completionkey,
			&pOverlapped, 0)) {
			if (dwTransferred == 0 && (Completionkey == NULL)) {
				/*printf("get count = %d, count0 = %d\r\n", count, count0);
				printf("thread is preparing to exit\r\n");*/
				break;
			}
			pParam = (typename CEdoyunQueue<T>::PPARAM*)Completionkey;
			DealParam(pParam);
		}
		HANDLE htemp = CEdoyunQueue<T>::m_hCompletionPort;
		CEdoyunQueue<T>::m_hCompletionPort = NULL;
		CloseHandle(CEdoyunQueue<T>::m_hCompletionPort);
	}
private:
	ThreadFuncBase* m_base;
	EDYCALLBACK m_callback;
	EdoyunThread m_thread;
};

typedef EdoyunSendQueue<std::vector<char>>::EDYCALLBACK SENDCALLBACK;
