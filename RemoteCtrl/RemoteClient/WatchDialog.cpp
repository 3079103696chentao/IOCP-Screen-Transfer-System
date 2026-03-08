// WatchDialog.cpp: 实现文件
//

#include "pch.h"
#include "RemoteClient.h"
#include "afxdialogex.h"
#include "WatchDialog.h"
#include"RemoteClientDlg.h"
#include"ClientSocket.h"
#include"ClientController.h"

// CWatchDialog 对话框

IMPLEMENT_DYNAMIC(CWatchDialog, CDialogEx)

CWatchDialog::CWatchDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_WATCH, pParent)
{
	m_nObjWidth = -1;
	m_nObjHeight = -1;
}

CWatchDialog::~CWatchDialog()
{
}

void CWatchDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_WATCH, m_picture);
}


BEGIN_MESSAGE_MAP(CWatchDialog, CDialogEx)
	ON_WM_TIMER()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_STN_CLICKED(IDC_WATCH, &CWatchDialog::OnStnClickedWatch)
	ON_BN_CLICKED(IDC_BTN_UNLOCK, &CWatchDialog::OnBnClickedBtnUnlock)
	ON_BN_CLICKED(IDC_BTN_LOCK, &CWatchDialog::OnBnClickedBtnLock)
	ON_MESSAGE(WM_SEND_PACK_ACK, &CWatchDialog::OnSendPackAck)
END_MESSAGE_MAP()




CPoint CWatchDialog::UserPoint2RemoteScreenPoint(CPoint& point, bool isScreen)
{
	
	CRect clientRect;
	if (isScreen) ScreenToClient(&point);
	TRACE("x = %d y = %d\r\n", point.x, point.y);

	m_picture.GetWindowRect(clientRect);
	TRACE(_T("m_picture HWND: %p, rect: %d,%d,%d,%d, width: %d, height: %d\r\n"),
			m_picture.m_hWnd, clientRect.left, clientRect.top, clientRect.right, clientRect.bottom,
			clientRect.Width(), clientRect.Height());
	TRACE("m_nObjWidth = %d, m_nObjHeight = %d\r\n", m_nObjWidth, m_nObjHeight);
	ScreenToClient(&clientRect);
	point.x -= clientRect.left;
	point.y -= clientRect.top;
    //本地坐标到远程坐标
    return CPoint(point.x * m_nObjWidth/ clientRect.Width(), point.y * m_nObjHeight / clientRect.Height());
	
}

BOOL CWatchDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  在此添加额外的初始化
	m_isFull = false;

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void CWatchDialog::OnTimer(UINT_PTR nIDEvent)
{
	Sleep(50);
	//当图片缓存更新的时候，将图片显示出来，然后更改图片缓存状态
	CDialogEx::OnTimer(nIDEvent);
}

LRESULT CWatchDialog::OnSendPackAck(WPARAM wParam, LPARAM lParam)
{
	if (lParam < 0) {
		//错误处理
	}
	else if (lParam == 1) {
		//对方关闭了套接字
	}
	else {
		CPacket* pPacket = (CPacket*)wParam;
		if (pPacket != NULL) {
			switch (pPacket->sCmd) {
			case 6://发送屏幕内容
			{
				if (m_isFull) {//感觉有问题，不应该是！m_isFull
					//DC device context
					CEdoyunTool::Bytes2Image(m_image, pPacket->strData);
					CRect rect;
					m_picture.GetWindowRect(rect);
					/*pParent->GetImage().BitBlt(m_picture.GetDC()->GetSafeHdc()
						, 0, 0, SRCCOPY);*/
					m_nObjWidth = m_image.GetWidth();
					m_nObjHeight = m_image.GetHeight();

					m_image.StretchBlt(m_picture.GetDC()->GetSafeHdc()
						, 0, 0, rect.Width(), rect.Height(), SRCCOPY);//缩放
					m_picture.InvalidateRect(NULL); //标记控件的整个客户区域内容过时，请在合适的时机重新绘制
					m_image.Destroy();
					m_isFull = false;
					TRACE("更新图片完成 %d %d\r\n", m_nObjWidth, m_nObjHeight);
				}

				break;
			}
			case 5://鼠标操作
			case 7:
			case 8:
			default:
				break;
			}
		}
	}
	return 0;
}

void CWatchDialog::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (m_nObjHeight != -1 && m_nObjWidth != -1) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0; //左键
		event.nAction = 1; //双击
		//发送
		CClientController* pController = CClientController::getInstance();
		pController->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(MOUSEEV));
	}

	CDialogEx::OnLButtonDblClk(nFlags, point);
}

void CWatchDialog::OnLButtonDown(UINT nFlags, CPoint point)
{
	

	if (m_nObjHeight != -1 && m_nObjWidth != -1) {
		//坐标转换
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		TRACE(_T("x= %d, y=%d \r\n", remote.x, remote.y));
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0; //左键
		event.nAction = 2; //按下
		//发送
		//发送
		CClientController* pController = CClientController::getInstance();
		pController->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(MOUSEEV));
	}
	CDialogEx::OnLButtonDown(nFlags, point);
}

void CWatchDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_nObjHeight != -1 && m_nObjWidth != -1) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0; //左键
		event.nAction = 3; //放开
		//发送
		//发送
		CClientController* pController = CClientController::getInstance();
		pController->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(MOUSEEV));
	}
	CDialogEx::OnLButtonUp(nFlags, point);
}

void CWatchDialog::OnRButtonDblClk(UINT nFlags, CPoint point)
{
	if (m_nObjHeight != -1 && m_nObjWidth != -1) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1; //右键
		event.nAction = 1; //双击
		//发送
		//发送
		CClientController* pController = CClientController::getInstance();
		pController->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(MOUSEEV));
	}
	CDialogEx::OnRButtonDblClk(nFlags, point);
}

void CWatchDialog::OnRButtonDown(UINT nFlags, CPoint point)
{
	if (m_nObjHeight != -1 && m_nObjWidth != -1) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1; //右键
		event.nAction = 2; //按下
		//发送
		//发送
		CClientController* pController = CClientController::getInstance();
		pController->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(MOUSEEV));
	}
	CDialogEx::OnRButtonDown(nFlags, point);
}

void CWatchDialog::OnRButtonUp(UINT nFlags, CPoint point)
{
	if (m_nObjHeight != -1 && m_nObjWidth != -1) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1; //右键
		event.nAction = 3; //放开
		//发送
		//发送
		CClientController* pController = CClientController::getInstance();
		pController->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(MOUSEEV));
	}
	CDialogEx::OnRButtonUp(nFlags, point);
}

void CWatchDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_nObjHeight != -1 && m_nObjWidth != -1) {
		TRACE(_T("point in picture is x:%d, y:%d\r\n"), point.x, point.y);
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		TRACE(_T("point in remote is x:%d, y:%d\r\n"), remote.x, remote.y);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 9; //没有按键
		event.nAction = 0; //没有操作
		//发送
		TRACE("鼠标移动 client x = %d, y = %d\r\n", remote.x, remote.y);
		//发送
		CClientController* pController = CClientController::getInstance();
		pController->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(MOUSEEV));
	}
	CDialogEx::OnMouseMove(nFlags, point);
}

void CWatchDialog::OnStnClickedWatch()
{
	if (m_nObjHeight != -1 && m_nObjWidth != -1) {
		CPoint point;
		GetCursorPos(&point);
		//这个point是屏幕全局坐标
		CPoint remote = UserPoint2RemoteScreenPoint(point, true);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0; //没有按键
		event.nAction = 0; //没有操作
		//发送
		CClientController* pController = CClientController::getInstance();
		pController->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(MOUSEEV));
	}

}

void CWatchDialog::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialogEx::OnOK();
}

void CWatchDialog::OnBnClickedBtnLock()
{
	//发送
	CClientController* pController = CClientController::getInstance();
	pController->SendCommandPacket(GetSafeHwnd(), 7);
}


void CWatchDialog::OnBnClickedBtnUnlock()
{
	//发送
	CClientController* pController = CClientController::getInstance();
	pController->SendCommandPacket(GetSafeHwnd(), 8);
}

