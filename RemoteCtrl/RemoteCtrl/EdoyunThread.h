#pragma once
#include"pch.h"
#include<atomic>
#include<vector>
#include<mutex>
#include<windows.h>
class ThreadFuncBase {};
typedef int(ThreadFuncBase::* FUNCTYPE)();
class ThreadWorker {
public:
	ThreadWorker():thiz(NULL), func(NULL){}
	ThreadWorker(ThreadFuncBase* obj, FUNCTYPE f):thiz(obj),func(f) {}
	ThreadWorker(const ThreadWorker& worker) {
		thiz = worker.thiz;
		func = worker.func;
	}
	ThreadWorker& operator=(const ThreadWorker& worker) {
		if (this != &worker) {
			thiz = worker.thiz;
			func = worker.func;
		}
		return *this;
	}
	
	int operator()() {
		if (IsValid()) {
			return (thiz->*func)(); //运算符优先级，->*的优先级低于()
		}
		return -1;
	}
	bool IsValid() const{
		return (thiz != nullptr) && (func != nullptr);
	}
private:
	ThreadFuncBase* thiz;
	int(ThreadFuncBase::* func)();
};
class EdoyunThread
{
public:
	EdoyunThread() {
		m_hThread = NULL;
	}
	~EdoyunThread() {
		Stop();
	}
	//true表示成功， false表示失败
	bool Start() {
		m_bStatus = true;
		m_hThread = (HANDLE)_beginthread(ThreadEntry, 0, this);
		if (!IsValid()) {
			m_bStatus = false;
		}
		return m_bStatus;
	}
	bool IsValid() {
		if (m_hThread == NULL || m_hThread == INVALID_HANDLE_VALUE) return false;
		return WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT;//证明这个线程一直存活着，一直有效的
		//线程已经结束会返回 WAIT_OBJECT_0
		//线程仍在运行，会返回WAIT_TIMEOUT
		//等待失败，句柄无效，WAIT_FAILED
	}
	bool Stop() {
		if (!m_bStatus) return true;
		m_bStatus = false;
		bool ret = WaitForSingleObject(m_hThread, INFINITE) == WAIT_OBJECT_0;
		//if (m_worker.load() == nullptr) {
		//	::ThreadWorker* pWorker = m_worker.load();
		//	m_worker.store(nullptr);
			//delete pWorker;
		//}
		UpdateWorker(*m_worker);
		return ret;
		
	}
	void UpdateWorker(const ::ThreadWorker& worker = ::ThreadWorker()) {
		if (!worker.IsValid()) {
			m_worker.store(nullptr);
			return;
		}
		if (m_worker.load() != nullptr) {
			::ThreadWorker* pWorker = m_worker.load();
			m_worker.store(nullptr);
			delete pWorker;
		}
		m_worker.store(new ::ThreadWorker(worker));
    }
	//true表示空闲 false表示已经分配了工作
	bool IsIdle() {
		return !m_worker.load()->IsValid();
	}
	
private:
	virtual void ThreadWorker() {
		while (m_bStatus) {
			::ThreadWorker worker = *m_worker.load();//这个函数返回的是临时对象，
			//需要const T&或者右值引用才可以绑定到临时对象
			if (worker.IsValid()) {
				int ret = worker();//返回值小于0,则终止线程循环，大于零，则警告日志，等于0表示正常
				if (ret != 0) {
					CString str;
					str.Format("thread found waring code %d\r\n", ret);
					OutputDebugString(str);
				}
				if (ret < 0) {
					m_worker.store(nullptr);
				}
			}
			else {
				Sleep(1);
			}
		}
	}
	static void ThreadEntry(void* arg) {
		EdoyunThread* thiz = (EdoyunThread*)arg;
		if (thiz) {
			thiz->ThreadWorker();
		}
		_endthread();
	}
private:
	HANDLE m_hThread;
	bool m_bStatus;//false 表示线程将要关闭 true表示线程存在
	std::atomic<::ThreadWorker*>m_worker;
};

class EdoyunThreadPool {
public:
	EdoyunThreadPool(size_t size) {
		m_threads.resize(size);
		for (size_t i = 0; i < size; i++) {
			m_threads[i] = new EdoyunThread();
		}
	}
	EdoyunThreadPool() {}
	~EdoyunThreadPool() {
		Stop();
		m_threads.clear();
	}
	bool Invoke() {
		bool ret = true;
		for (size_t i = 0; i < m_threads.size(); i++) {
			if (m_threads[i]->Start() == false) {
				ret = false;
				break;
			}
		}
		if (ret == false) {
			for (size_t i = 0; i < m_threads.size(); i++) {
				m_threads[i]->Stop();
			}
		}
		return ret;
	}
	void Stop() {
		for (size_t i = 0; i < m_threads.size(); i++) {
			m_threads[i]->Stop();
		}
	}
	//返回-1，表示分配失败，所有线程都在忙，大于等于0，表示第n+1个线程分配来做这个事情
	int DispatchWorker(const ThreadWorker& worker) {
		int index = -1;//可以搞一个队列，来存放空闲线程的index,当需要分配工作的时候，从队列中取
		m_lock.lock();
		for (size_t i = 0; i < m_threads.size(); i++) {
			if (m_threads[i]->IsIdle()) {
				m_threads[i]->UpdateWorker(worker);
				index = i;
				break;
			}
		}
		m_lock.unlock();
		return index;
	}
	bool CheckThreadValid(size_t index) {
		if (index < m_threads.size()) {
			return m_threads[index]->IsValid();
		}
		return false;
	}
private:
	std::mutex m_lock;
	std::vector<::EdoyunThread*>m_threads;
};

