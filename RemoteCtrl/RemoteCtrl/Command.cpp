#include "pch.h"
#include "Command.h"

CCommand::CCommand():threadid(0)
{
	struct {
		int nCmd;
		CMDFUNC func;
	}data[] = {
		{1,&CCommand::MakeDriverInfo},
		{2,&CCommand::MakeDirectoryInfo},
		{3,&CCommand::RunFile},
		{4,&CCommand::DownloadFile},
        {5,&CCommand::MouseEvent},
		{6,&CCommand::SendScreen},
		{7,&CCommand::LockMachine},
		{8,&CCommand::UnlockMachine},
		{9,&CCommand::DeleteLocalFile},
		{1981,&CCommand::TestConnect},
		{-1,nullptr}
	};
	for (int i = 0; data[i].nCmd != -1; i++) {
		m_mapFunction.insert({ data[i].nCmd, data[i].func });
	}
}

int CCommand::ExcuteCommand(int nCmd, std::list<CPacket>& lstPacket, CPacket& inPacket)
{
	auto it = m_mapFunction.find(nCmd);
	if (it == m_mapFunction.end()) {
		return -1;
	}
	int ret = (this->*(it->second))(lstPacket, inPacket);
	return ret;
}
