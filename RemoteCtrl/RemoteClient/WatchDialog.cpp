// WatchDialog.cpp: 实现文件
//

#include "pch.h"
#include "RemoteClient.h"
#include "afxdialogex.h"
#include "WatchDialog.h"
#include"RemoteClientDlg.h"
#include"ClientSocket.h"

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
END_MESSAGE_MAP()


// CWatchDialog 消息处理程序

//bool CWatchDialog::GetRealPoint(CPoint& point)
//{
//	CRect  rectPicture;
//	m_picture.GetWindowRect(&rectPicture); //获取图片控件在屏幕上的位置
//	ScreenToClient(&rectPicture); //转换为相对于对话框的坐标
//	if (rectPicture.PtInRect(point)) {
//		//减去图片控件在左上角的偏移，得到相对于图片本身的（0,0）
//		point.x = point.x - rectPicture.left;
//		point.y = point.y - rectPicture.top;
//		return true;
//	}
//	return false;
//}

CPoint CWatchDialog::UserPoint2RemoteScreenPoint(CPoint& point, bool isScreen)
{
	//CRect clientRect;
	//if (m_picture.m_hWnd) {
	//	
	//	m_picture.GetWindowRect(&clientRect);
	//	TRACE(_T("m_picture HWND: %p, rect: %d,%d,%d,%d, width: %d, height: %d\r\n"),
	//		m_picture.m_hWnd, clientRect.left, clientRect.top, clientRect.right, clientRect.bottom,
	//		clientRect.Width(), clientRect.Height());
	//}
	//else {
	//	TRACE(_T("m_picture not subclassed yet!\n"));
	//}
	//TRACE(_T("client in picture: x= %d, y=%d \r\n"), point.x, point.y);
 //   if (isScreen) ScreenToClient(&point);//获取在截取的屏幕上点击的位置坐标,本地坐标,把屏幕坐标换算成某个窗口客户区坐标
	//TRACE(_T("after isScreen client in picture: x= %d, y=%d \r\n"), point.x, point.y);
	//
	//TRACE(_T("m_picture.m_hWnd: %d, \r\n"), m_picture.m_hWnd);
	//TRACE(_T("m_nObjWidth: %d, m_nObjHeight: %d\r\n"), m_nObjWidth, m_nObjHeight);
	//m_picture.GetWindowRect(&clientRect);
	//TRACE(_T("clientRect.Width: %d, clientRect.Height: %d\r\n"), clientRect.Width(), clientRect.Height());
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
	SetTimer(0, 50, NULL);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void CWatchDialog::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	if (nIDEvent == 0) {
		CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent();
		if (pParent->isFull()) {
			//DC device context
			CRect rect; 
			m_picture.GetWindowRect(rect);
			/*pParent->GetImage().BitBlt(m_picture.GetDC()->GetSafeHdc()
				, 0, 0, SRCCOPY);*/
			
			if (m_nObjWidth ==- 1) {
				m_nObjWidth = pParent->GetImage().GetWidth();
			}
			if (m_nObjHeight == -1) {
				m_nObjHeight = pParent->GetImage().GetHeight();
			}
			pParent->GetImage().StretchBlt(m_picture.GetDC()->GetSafeHdc()
				, 0, 0, rect.Width(), rect.Height(),SRCCOPY);//缩放
			m_picture.InvalidateRect(NULL); //标记控件的整个客户区域内容过时，请在合适的时机重新绘制
			pParent->GetImage().Destroy();
			pParent->SetImageStatus();
		}
	}
	CDialogEx::OnTimer(nIDEvent);
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
		CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent(); //获取主对话框指针
		pParent->SendMessage(WM_SEND_PACKET, 5 << 1 | 1, (WPARAM)&event);
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
		CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent(); //获取主对话框指针
		pParent->SendMessage(WM_SEND_PACKET, 5 << 1 | 1, (WPARAM)&event);
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
		CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent(); //获取主对话框指针
		pParent->SendMessage(WM_SEND_PACKET, 5 << 1 | 1, (WPARAM)&event);
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
		CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent(); //获取主对话框指针
		pParent->SendMessage(WM_SEND_PACKET, 5 << 1 | 1, (WPARAM)&event);
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
		CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent(); //获取主对话框指针
		pParent->SendMessage(WM_SEND_PACKET, 5 << 1 | 1, (WPARAM)&event);
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
		CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent(); //获取主对话框指针
		//TODO:存在设计隐患，网络通信和对话框有耦合
		pParent->SendMessage(WM_SEND_PACKET, 5 << 1 | 1, (WPARAM)&event);
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
		CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent(); //TODO:
		pParent->SendMessage(WM_SEND_PACKET, 5 << 1 | 1, (WPARAM)&event);
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
		CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent(); //获取主对话框指针
		pParent->SendMessage(WM_SEND_PACKET, 5 << 1 | 1, (WPARAM)&event);
	}

}

void CWatchDialog::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialogEx::OnOK();
}

void CWatchDialog::OnBnClickedBtnLock()
{
	CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent(); //获取主对话框指针
	pParent->SendMessage(WM_SEND_PACKET, 7 << 1 | 1);
}


void CWatchDialog::OnBnClickedBtnUnlock()
{
	CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent(); //获取主对话框指针
	pParent->SendMessage(WM_SEND_PACKET, 8 << 1 | 1);
}

