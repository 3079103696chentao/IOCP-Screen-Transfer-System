#pragma once
#include"ClientSocket.h"
#include"WatchDialog.h"
#include"RemoteClientDlg.h"
#include"StatusDlg.h"
#include"resource.h"
#include<map>
#include "EdoyunTool.h"


//#define WM_SEND_DATA (WM_USER+2) //发送数据
#define WM_SHOW_STATUS (WM_USER+3) //展示状态
#define WM_SHOW_WATCH (WM_USER+4) //远程监控
#define WM_SEND_MESSAGE (WM_USER +0x1000) //自定义消息处理



class CClientController
{
public:
	
	//获取全局唯一对象
	static CClientController* getInstance();

	int InitController();
	//启动
	int Invoke(CWnd* pMainWnd);

	//发送消息
	LRESULT SendMessage(MSG msg);

	//更新网络服务器的地址
	void UpdateAddress(int nIp, int nPort);

	int DealCommand() {
		return CClientSocket::getInstance()->DealCommand();
	}
	void CloseSocket() {
		CClientSocket::getInstance()->CloseSocket();
	}
	//1.查看磁盘分区
//2.查看指定目录下的文件
//3.打开文件
//4.下载文件
//9.删除文件
//5 鼠标操作
//  6://发送屏幕内容==>发送屏幕的截图
// 7://锁机
// 8.解锁
//返回值，true成功 false失败
	bool SendCommandPacket(
		HWND hWnd, //数据包收到后，需要应答的窗口
		int nCmd,
		bool bAutoClose = true,
		BYTE* pData = NULL,
		size_t nLength = 0
		);
	int GetImage(CImage& image);
	int DownFile(CString strPath);

	void StartWatchScreen();
protected:
	void threadWatchScreen();
	static void threadWatchScreenEntry(void* arg);
	void threadDownloadFile();
	static void threadDownloadFileEntry(void* arg);
	CClientController():m_statusDlg(&m_remoteDlg)
		,m_watchDlg(&m_remoteDlg)
	{
		m_hThread = INVALID_HANDLE_VALUE;
		m_hThreadDownload = INVALID_HANDLE_VALUE;
		m_hThreadWatch = INVALID_HANDLE_VALUE;
		m_nThreadID = -1;
		m_bIsClosed = true;
	}

	~CClientController() {
		WaitForSingleObject(m_hThread, 100);
	}

	void threadFunc();

	static unsigned __stdcall threadEntry(void* arg);

	static void releaseInstance();
	
	LRESULT OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam);

private:
	typedef struct MsgInfo {

		MSG msg;
		LRESULT result;
		MsgInfo(MSG m) {
			result = 0;
			memcpy(&msg, &m, sizeof(MSG));
		}
		MsgInfo() {
			result = 0;
			memset(&msg, 0, sizeof(MSG));
		}
		MsgInfo(const MsgInfo& m) {
			memcpy(&msg, &m.msg, sizeof(MSG));
			result = m.result;
		}
		MsgInfo& operator=(const MsgInfo& m){
			if (this != &m) {
				memcpy(&msg, &m, sizeof(MsgInfo));
				result = 0;
			}
			return *this;
		}

	}MSGINFO;
	typedef LRESULT(CClientController::* MSGFUNC)(UINT nMsg,
		WPARAM wParam, LPARAM lParam);
	static std::map<UINT, MSGFUNC>m_mapFunc;
	
	CRemoteClientDlg m_remoteDlg;
	CWatchDialog m_watchDlg;
	CStatusDlg m_statusDlg;
	HANDLE m_hThread;
	HANDLE m_hThreadDownload;
	HANDLE m_hThreadWatch;
	unsigned m_nThreadID;
	//下载文件的远程路径
	CString m_strRemote;
	//下载文件的本地保存路径
	CString m_strLocal; 

	bool m_bIsClosed; //监视是否开着



	static CClientController* m_instance;

	class CHelper {
	public:
		CHelper() {}
		~CHelper() {
			CClientController::releaseInstance();
		}
	};
	static CHelper m_helper;

};

