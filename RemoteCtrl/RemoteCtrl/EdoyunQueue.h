#pragma once
template<typename T>
class CEdoyunQueue
{
	//线程安全的队列，利用IOCP
public:
	CEdoyunQueue();
	~CEdoyunQueue();
	bool void PushBack(const T& data);
	bool PopFront();
	size_t Size();
	void clear();
private:
	static void threadEntry(void* arg);
	void threadmain();
private:
	std::list<T>m_lstData;
	HANDLE m_hCompletionPort;
	HANDLE m_hThread;
public:
	typedef struct IocpParam {
		int nOperator;//操作
		T strData;//数据
		HANDLE hEvent;//pop操作需要的
		IocpParam(int op, const char* sData, _beginthread_proc_type cb = NULL) {
			nOperator = op;
			strData = sData;
			cbFunc = cb;
		}
		IocpParam() {
			nOperator = -1;
			cbFunc = nullptr;
		}

	}PPARAM;//Post Parameter 用于投递信息的结构体
	enum {
		EQPush,
		EQPop,
		EQSize,
		EQClear
	};
};



