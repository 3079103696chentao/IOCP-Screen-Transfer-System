#pragma once
#include<map>
#include<atlimage.h>
#include"ServerSocket.h"
#include<direct.h>
#include<io.h>//包含_findfist等函数
#include<list>
#include<string>
#include"EdoyunTool.h"
#include"LockDialog.h"
#include"resource.h"
#pragma warning (disable 4996)
class CCommand
{
public:
	CCommand();
	~CCommand() {}
    int ExcuteCommand(int nCmd);
protected:
	typedef int(CCommand::* CMDFUNC)(); //成员函数指针
	std::map<int, CMDFUNC>m_mapFunction;//从命令号到功能的映射
	CLockDialog dlg;
	unsigned threadid ;
protected:
	int MakeDriverInfo();
	int MakeDirectoryInfo();
	int RunFile();
	int DownloadFile();
	int MouseEvent();
	int SendScreen();
	static unsigned _stdcall threadLockDlg(void* arg); //_beginthreadex需要
	void threadLockDlgMain();
	int LockMachine();
	int UnlockMachine();
	int DeleteLocalFile();
	int TestConnect();
};

