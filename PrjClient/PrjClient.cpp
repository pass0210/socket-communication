#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "resource.h"

#define SERVERIPV4  "127.0.0.1"
#define SERVERIPV6  "::1"
#define SERVERPORT  9000

#define BUFSIZE     256                    // ���� �޽��� ��ü ũ��
#define MSGSIZE     (BUFSIZE-sizeof(int))  // ä�� �޽��� �ִ� ����

#define CHATTING    1000                   // �޽��� Ÿ��: ä��
#define DRAWLINE    1001                   // �޽��� Ÿ��: �� �׸���
#define DRAWSQUA	1002	//�޼��� Ÿ��: �簢��
#define DRAWCURC	1003	//�޼��� Ÿ��: ��
#define DRAWTRAG	1004	//�޼��� Ÿ��: �����ﰢ��
#define DRAWRHOM	1005	//�޼��� Ÿ��: ������
#define DRAWISTR	1007	//�޼��� Ÿ��: �̵�ﰢ��

#define WM_DRAWIT   (WM_USER+1)            // ����� ���� ������ �޽���

// ���� �޽��� ����
// sizeof(COMM_MSG) == 256
struct COMM_MSG
{
	int  type;
	char dummy[MSGSIZE];
};

// ä�� �޽��� ����
// sizeof(CHAT_MSG) == 256
struct CHAT_MSG
{
	int  type;
	char id[32];
	char buf[MSGSIZE-sizeof(char[32])];
};



// �� �׸��� �޽��� ����
// sizeof(DRAWLINE_MSG) == 256
struct DRAW_MSG
{
	int  type;
	int  color;
	int  x0, y0;
	int  x1, y1;
	char dummy[BUFSIZE - 6 * sizeof(int)];
};

/*�߰� ���� ����ü*/
//�� ����ü
struct CURC_MSG {
	int  type;
	int  color;
	int  x0, y0;
	int	 r;
	char dummy[BUFSIZE - 5 * sizeof(int)];
};

//�ﰢ�� ����ü
struct TRAG_MSG {
	int  type;
	int  color;
	int  x0, y0;
	int  base, height;
	char dummy[BUFSIZE - 6 * sizeof(int)];
};

//������ ����ü
struct RHOM_MSG {
	int  type;
	int  color;
	int  x0, y0; // �� �߾� ��
	int  width, height;
	char dummy[BUFSIZE - 6 * sizeof(int)];
};


static HINSTANCE     g_hInst; // ���� ���α׷� �ν��Ͻ� �ڵ�
static HWND          g_hDrawWnd; // �׸��� �׸� ������
static HWND          g_hButtonSendMsg; // '�޽��� ����' ��ư
static HWND          g_hEditStatus; // ���� �޽��� ���
static char          g_ipaddr[64]; // ���� IP �ּ�
static u_short       g_port; // ���� ��Ʈ ��ȣ
static BOOL          g_isIPv6; // IPv4 or IPv6 �ּ�?
static HANDLE        g_hClientThread; // ������ �ڵ�
static volatile BOOL g_bStart; // ��� ���� ����
static SOCKET        g_sock; // Ŭ���̾�Ʈ ����
static HANDLE        g_hReadEvent, g_hWriteEvent; // �̺�Ʈ �ڵ�
static CHAT_MSG      g_chatmsg; // ä�� �޽��� ����
static DRAW_MSG		 g_drawmsg; // �� �׸��� �޽��� ����
static int           g_drawcolor; // �� �׸��� ����
static int			 now_drawcolor;

/*�߰� ����*/
static CURC_MSG		 g_drawcurc; //�� �׸��� �޽��� ����
static TRAG_MSG		 g_drawtrag; //�� �׸��� �޽��� ����
static RHOM_MSG		 g_drawrhom; //�� �׸��� �޽��� ����
static int	g_drawtype;  //Ŭ���̾�Ʈ �׸��� Ÿ��
static int  now_drawtype; //���� �׸��� Ÿ��



static HWND hCombo;
TCHAR* text[] = { TEXT("��"), TEXT("�簢��"), TEXT("��"), TEXT("�����ﰢ��"), TEXT("�̵�ﰢ��"), TEXT("������")};
int index;
char str[32], tmp_id[64];

// ��ȭ���� ���ν���
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// ���� ��� ������ �Լ�
DWORD WINAPI ClientMain(LPVOID arg);
DWORD WINAPI ReadThread(LPVOID arg);
DWORD WINAPI WriteThread(LPVOID arg);
// �ڽ� ������ ���ν���
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
// ���� ��Ʈ�� ��� �Լ�
void DisplayText(char* fmt, ...);
// ����� ���� ������ ���� �Լ�
int recvn(SOCKET s, char* buf, int len, int flags);
// ���� ��� �Լ�
void err_quit(char* msg);
void err_display(char* msg);
void change_dtype(char* str);


// ���� �Լ�
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,LPSTR lpCmdLine, int nCmdShow)
{
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

	// �̺�Ʈ ����
	g_hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (g_hReadEvent == NULL) return 1;
	g_hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (g_hWriteEvent == NULL) return 1;

	// ���� �ʱ�ȭ(�Ϻ�)
	g_chatmsg.type = CHATTING;
	g_drawmsg.type = DRAWLINE;
	g_drawcolor = RGB(255, 0, 0);
	now_drawcolor = RGB(255, 0, 0);
	g_drawmsg.color = g_drawcolor;

	// ��ȭ���� ����
	g_hInst = hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// �̺�Ʈ ����
	CloseHandle(g_hReadEvent);
	CloseHandle(g_hWriteEvent);

	// ���� ����
	WSACleanup();
	return 0;
}

// ��ȭ���� ���ν���
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hButtonIsIPv6;
	static HWND hEditIPaddr;
	static HWND hEditPort;
	static HWND hButtonConnect;
	static HWND hEditMsg;
	static HWND hColorRed;
	static HWND hColorGreen;
	static HWND hColorBlue;
	static HWND hId;

	switch (uMsg) {
	case WM_INITDIALOG:
		// ��Ʈ�� �ڵ� ���
		hButtonIsIPv6 = GetDlgItem(hDlg, IDC_ISIPV6);
		hEditIPaddr = GetDlgItem(hDlg, IDC_IPADDR);
		hEditPort = GetDlgItem(hDlg, IDC_PORT);
		hButtonConnect = GetDlgItem(hDlg, IDC_CONNECT);
		g_hButtonSendMsg = GetDlgItem(hDlg, IDC_SENDMSG);
		hEditMsg = GetDlgItem(hDlg, IDC_MSG);
		g_hEditStatus = GetDlgItem(hDlg, IDC_STATUS);
		hColorRed = GetDlgItem(hDlg, IDC_COLORRED);
		hColorGreen = GetDlgItem(hDlg, IDC_COLORGREEN);
		hColorBlue = GetDlgItem(hDlg, IDC_COLORBLUE);
		hCombo = GetDlgItem(hDlg, IDC_COMBOBOX); //�޺��ڽ� �ڵ� ���
		hId = GetDlgItem(hDlg, IDC_ID); //���̵� �ڵ鷯 ���

		// ��Ʈ�� �ʱ�ȭ
		SendMessage(hEditMsg, EM_SETLIMITTEXT, MSGSIZE, 0);
		EnableWindow(g_hButtonSendMsg, FALSE);
		SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
		SetDlgItemInt(hDlg, IDC_PORT, SERVERPORT, FALSE);
		SendMessage(hColorRed, BM_SETCHECK, BST_CHECKED, 0);
		SendMessage(hColorGreen, BM_SETCHECK, BST_UNCHECKED, 0);
		SendMessage(hColorBlue, BM_SETCHECK, BST_UNCHECKED, 0);

		// ������ Ŭ���� ���
		WNDCLASS wndclass;
		wndclass.style = CS_HREDRAW | CS_VREDRAW;
		wndclass.lpfnWndProc = WndProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = g_hInst;
		wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = "MyWndClass";
		if (!RegisterClass(&wndclass)) return 1;

		// �ڽ� ������ ����
		g_hDrawWnd = CreateWindow("MyWndClass", "�׸� �׸� ������", WS_CHILD,
			450, 38, 425, 415, hDlg, (HMENU)NULL, g_hInst, NULL);
		if (g_hDrawWnd == NULL) return 1;
		ShowWindow(g_hDrawWnd, SW_SHOW);
		UpdateWindow(g_hDrawWnd);

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ISIPV6:
			g_isIPv6 = SendMessage(hButtonIsIPv6, BM_GETCHECK, 0, 0);
			if (g_isIPv6 == false)
				SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
			else
				SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV6);
			return TRUE;

		case IDC_CONNECT:
			GetDlgItemText(hDlg, IDC_IPADDR, g_ipaddr, sizeof(g_ipaddr));
			g_port = GetDlgItemInt(hDlg, IDC_PORT, NULL, FALSE);
			g_isIPv6 = SendMessage(hButtonIsIPv6, BM_GETCHECK, 0, 0);
			if (GetDlgItemText(hDlg, IDC_ID, g_chatmsg.id, sizeof(g_chatmsg.id) )== NULL) {
				MessageBox(hDlg, "���̵� �Է��� �ּ���", "����!", MB_ICONERROR);
			}
			else {
				// ���� ��� ������ ����
				g_hClientThread = CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);
				if (g_hClientThread == NULL) {
					MessageBox(hDlg, "Ŭ���̾�Ʈ�� ������ �� �����ϴ�."
						"\r\n���α׷��� �����մϴ�.", "����!", MB_ICONERROR);
					EndDialog(hDlg, 0);
				}
				else {
					EnableWindow(hButtonConnect, FALSE);
					while (g_bStart == FALSE); // ���� ���� ���� ��ٸ�
					EnableWindow(hButtonIsIPv6, FALSE);
					EnableWindow(hEditIPaddr, FALSE);
					EnableWindow(hEditPort, FALSE);
					EnableWindow(hId, FALSE);
					EnableWindow(g_hButtonSendMsg, TRUE);
					SetFocus(hEditMsg);
				}
			}
			return TRUE;
		case IDC_ID:
			SendMessage(hId, WM_GETTEXT, sizeof(tmp_id), (LPARAM)tmp_id);
			return true;

		case IDC_COMBOBOX: //���� ������ ���� �޺� �ڽ�
			switch (HIWORD(wParam)) {
			case CBN_SELCHANGE:
				index = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
				SendMessage(hCombo, CB_GETLBTEXT, index, (LPARAM)str);
				SetWindowText(hDlg, str);
				change_dtype(str);
				break;
			case CBN_EDITCHANGE:
				GetWindowText(hCombo, str, 32);
				SetWindowText(hDlg, str);
				change_dtype(str);
				break;
			}
			return TRUE;


		case IDC_SENDMSG:
			// �б� �ϷḦ ��ٸ�
			WaitForSingleObject(g_hReadEvent, INFINITE);
			GetDlgItemText(hDlg, IDC_MSG, g_chatmsg.buf, sizeof(g_chatmsg.buf));
			// ���� �ϷḦ �˸�
			SetEvent(g_hWriteEvent);
			// �Էµ� �ؽ�Ʈ ��ü�� ���� ǥ��
			SendMessage(hEditMsg, EM_SETSEL, 0, -1);
			return TRUE;

		case IDC_COLORRED:
			g_drawcolor = RGB(255, 0, 0);
			return TRUE;

		case IDC_COLORGREEN:
			g_drawcolor = RGB(0, 255, 0);
			return TRUE;

		case IDC_COLORBLUE:
			g_drawcolor = RGB(0, 0, 255);
			return TRUE;

		case IDCANCEL:
			if (MessageBox(hDlg, "������ �����Ͻðڽ��ϱ�?",
				"����", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				closesocket(g_sock);
				EndDialog(hDlg, IDCANCEL);
			}
			return TRUE;

		}
		return FALSE;
	}

	return FALSE;
}

// ���� ��� ������ �Լ�
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;
	strcpy(g_chatmsg.id, tmp_id);

	if (g_isIPv6 == false) {
		// socket()
		g_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (g_sock == INVALID_SOCKET) err_quit("socket()");

		// connect()
		SOCKADDR_IN serveraddr;
		ZeroMemory(&serveraddr, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_addr.s_addr = inet_addr(g_ipaddr);
		serveraddr.sin_port = htons(g_port);
		retval = connect(g_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
		if (retval == SOCKET_ERROR) err_quit("connect()");
	}
	else {
		// socket()
		g_sock = socket(AF_INET6, SOCK_STREAM, 0);
		if (g_sock == INVALID_SOCKET) err_quit("socket()");

		// connect()
		SOCKADDR_IN6 serveraddr;
		ZeroMemory(&serveraddr, sizeof(serveraddr));
		serveraddr.sin6_family = AF_INET6;
		int addrlen = sizeof(serveraddr);
		WSAStringToAddress(g_ipaddr, AF_INET6, NULL,
			(SOCKADDR*)&serveraddr, &addrlen);
		serveraddr.sin6_port = htons(g_port);
		retval = connect(g_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
		if (retval == SOCKET_ERROR) err_quit("connect()");
	}
	MessageBox(NULL, "������ �����߽��ϴ�.", "����!", MB_ICONINFORMATION);

	// �б� & ���� ������ ����
	HANDLE hThread[2];
	hThread[0] = CreateThread(NULL, 0, ReadThread, NULL, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, WriteThread, NULL, 0, NULL);
	if (hThread[0] == NULL || hThread[1] == NULL) {
		MessageBox(NULL, "�����带 ������ �� �����ϴ�."
			"\r\n���α׷��� �����մϴ�.",
			"����!", MB_ICONERROR);
		exit(1);
	}

	g_bStart = TRUE;

	// ������ ���� ���
	retval = WaitForMultipleObjects(2, hThread, FALSE, INFINITE);
	retval -= WAIT_OBJECT_0;
	if (retval == 0)
		TerminateThread(hThread[1], 1);
	else
		TerminateThread(hThread[0], 1);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	g_bStart = FALSE;

	MessageBox(NULL, "������ ������ �������ϴ�", "�˸�", MB_ICONINFORMATION);
	EnableWindow(g_hButtonSendMsg, FALSE);

	closesocket(g_sock);
	return 0;
}

// ������ �ޱ�
DWORD WINAPI ReadThread(LPVOID arg)
{
	int retval;
	COMM_MSG comm_msg;
	CHAT_MSG* chat_msg;
	int len;
	while (1) {
		retval = recvn(g_sock, (char*)&len, sizeof(int), 0);
		if (retval == 0 || retval == SOCKET_ERROR) {
			break;
		}

		retval = recvn(g_sock, (char*)&comm_msg, len, 0);
		if (retval == 0 || retval == SOCKET_ERROR) {
			break;
		}

		if (comm_msg.type == CHATTING) {
			chat_msg = (CHAT_MSG*)&comm_msg;
			DisplayText("[���� �޽���] %s : %s\r\n", chat_msg->id,chat_msg->buf);
		}

		else if (comm_msg.type == DRAWLINE || comm_msg.type == DRAWSQUA || comm_msg.type == DRAWCURC || comm_msg.type == DRAWTRAG || comm_msg.type == DRAWRHOM|| comm_msg.type == DRAWISTR) {
			
			if(comm_msg.type == DRAWCURC) {
				CURC_MSG* draw_msg;
				draw_msg = (CURC_MSG*)&comm_msg;
				now_drawcolor = draw_msg->color;
				now_drawtype = draw_msg->type;
				SendMessage(g_hDrawWnd, WM_DRAWIT,
					MAKEWPARAM(draw_msg->x0 - draw_msg->r, draw_msg->y0 - draw_msg->r),
					MAKELPARAM(draw_msg->x0 + draw_msg->r, draw_msg->y0 + draw_msg->r));
			}
			else if (comm_msg.type == DRAWTRAG) {
				TRAG_MSG* draw_msg = (TRAG_MSG*)&comm_msg;
				now_drawcolor = draw_msg->color;
				now_drawtype = draw_msg->type;
				SendMessage(g_hDrawWnd, WM_DRAWIT,
					MAKEWPARAM(draw_msg->x0, draw_msg->y0),
					MAKELPARAM(draw_msg->base, draw_msg->height));
			}
			else if (comm_msg.type == DRAWISTR) {
				TRAG_MSG* draw_msg = (TRAG_MSG*)&comm_msg;
				now_drawcolor = draw_msg->color;
				now_drawtype = draw_msg->type;
				SendMessage(g_hDrawWnd, WM_DRAWIT,
					MAKEWPARAM(draw_msg->x0, draw_msg->y0),
					MAKELPARAM(draw_msg->base, draw_msg->height));
			}
			else if (comm_msg.type == DRAWRHOM) {
				RHOM_MSG* draw_msg;
				draw_msg = (RHOM_MSG*)&comm_msg;
				now_drawcolor = draw_msg->color;
				now_drawtype = draw_msg->type;
				SendMessage(g_hDrawWnd, WM_DRAWIT,
					MAKEWPARAM(draw_msg->x0, draw_msg->y0),
					MAKELPARAM(draw_msg->width, draw_msg->height));
			}
			else {
				DRAW_MSG* draw_msg;
				draw_msg = (DRAW_MSG*)&comm_msg;
				now_drawcolor = draw_msg->color;
				now_drawtype = draw_msg->type;
				SendMessage(g_hDrawWnd, WM_DRAWIT,
					MAKEWPARAM(draw_msg->x0, draw_msg->y0),
					MAKELPARAM(draw_msg->x1, draw_msg->y1));
			}
		}
	}

	return 0;
}

// ������ ������
DWORD WINAPI WriteThread(LPVOID arg)
{
	int retval;

	// ������ ������ ���
	while (1) {
		// ���� �Ϸ� ��ٸ���
		WaitForSingleObject(g_hWriteEvent, INFINITE);

		int msg_len = sizeof(g_chatmsg);

		// ���ڿ� ���̰� 0�̸� ������ ����
		if (msg_len == 0) {
			// '�޽��� ����' ��ư Ȱ��ȭ
			EnableWindow(g_hButtonSendMsg, TRUE);
			// �б� �Ϸ� �˸���
			SetEvent(g_hReadEvent);
			continue;
		}

		// ������ ������(��������)
		retval = send(g_sock, (char*)&msg_len, sizeof(int), 0);
		if (retval == SOCKET_ERROR) {
			break;
		}
		//������ ������ (���� ����)
		retval = send(g_sock, (char*)&g_chatmsg, msg_len, 0);
		if (retval == SOCKET_ERROR) {
			break;
		}

		// '�޽��� ����' ��ư Ȱ��ȭ
		EnableWindow(g_hButtonSendMsg, TRUE);
		// �б� �Ϸ� �˸���
		SetEvent(g_hReadEvent);
	}

	return 0;
}

// �ڽ� ������ ���ν���
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hDC;
	int cx, cy;
	PAINTSTRUCT ps;
	RECT rect;
	HPEN hPen, hOldPen;
	static HBITMAP hBitmap;
	static HDC hDCMem;
	static int x0, y0;
	static int x1, y1;
	static BOOL bDrawing = FALSE;
	int tmp1, tmp2;
	int retval;
	switch (uMsg) {
	case WM_CREATE:
		hDC = GetDC(hWnd);

		for (int i = 0; i < 6; i++) {
			SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)text[i]);
		}

		// ȭ���� ������ ��Ʈ�� ����
		cx = GetDeviceCaps(hDC, HORZRES);
		cy = GetDeviceCaps(hDC, VERTRES);
		hBitmap = CreateCompatibleBitmap(hDC, cx, cy);

		// �޸� DC ����
		hDCMem = CreateCompatibleDC(hDC);

		// ��Ʈ�� ���� �� �޸� DC ȭ���� ������� ĥ��
		SelectObject(hDCMem, hBitmap);
		SelectObject(hDCMem, GetStockObject(WHITE_BRUSH));
		SelectObject(hDCMem, GetStockObject(WHITE_PEN));
		Rectangle(hDCMem, 0, 0, cx, cy);

		ReleaseDC(hWnd, hDC);
		return 0;
	case WM_LBUTTONDOWN:
		x0 = LOWORD(lParam);
		y0 = HIWORD(lParam);
		if (g_drawmsg.type == DRAWLINE)
			bDrawing = TRUE;
		else
			bDrawing = FALSE;
		return 0;
	case WM_MOUSEMOVE:
		if (bDrawing && g_bStart) {
			x1 = LOWORD(lParam);
			y1 = HIWORD(lParam);

			// �� �׸��� �޽��� ������
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y0;
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y1;
			g_drawmsg.color = g_drawcolor;

			int g_drawmsg_len = sizeof(struct DRAW_MSG);
			retval = send(g_sock, (char*)&g_drawmsg_len, sizeof(int), 0);
			if (retval == SOCKET_ERROR) {
				break;
			}
			retval = send(g_sock, (char*)&g_drawmsg, g_drawmsg_len, 0);
			if (retval == SOCKET_ERROR) {
				break;
			}
			x0 = x1;
			y0 = y1;
		}
		return 0;
	case WM_LBUTTONUP:
		g_drawmsg.x0 = x0;
		g_drawmsg.y0 = y0;
		g_drawmsg.x1 = LOWORD(lParam);
		g_drawmsg.y1 = HIWORD(lParam);
		g_drawmsg.color = g_drawcolor;
		//������ ���
		tmp1 = (g_drawmsg.x1 - x0) / 2;
		tmp2 = (g_drawmsg.y1 - y0) / 2;
		if (g_drawtype == DRAWCURC) {
			g_drawcurc.type = DRAWCURC;
			g_drawcurc.color = g_drawcolor;
			g_drawcurc.x0 = g_drawmsg.x0 + tmp1;
			g_drawcurc.y0 = g_drawmsg.y0 + tmp2;
			if (abs(tmp1) >= abs(tmp2)) {
				g_drawcurc.r = abs(tmp2);
			}
			else {
				g_drawcurc.r = abs(tmp1);
			}
			int g_drawmsg_len = sizeof(struct CURC_MSG);
			send(g_sock, (char*)&g_drawmsg_len, sizeof(int), 0);
			send(g_sock, (char*)&g_drawcurc, g_drawmsg_len, 0);
		}
		else if (g_drawtype == DRAWRHOM) {
			g_drawrhom.type = DRAWRHOM;
			g_drawrhom.color = g_drawcolor;
			g_drawrhom.x0 = g_drawmsg.x0 + tmp1;
			g_drawrhom.y0 = g_drawmsg.y0 + tmp2;
			g_drawrhom.width = abs(tmp1);
			g_drawrhom.height = abs(tmp2);
			int g_drawmsg_len = sizeof(struct RHOM_MSG);
			send(g_sock, (char*)&g_drawmsg_len, sizeof(int), 0);
			send(g_sock, (char*)&g_drawrhom, g_drawmsg_len, 0);
		}
		else if (g_drawtype == DRAWTRAG ) {
			
			g_drawtrag.type = DRAWTRAG;
			g_drawtrag.color = g_drawcolor;
			g_drawtrag.x0 = g_drawmsg.x0 + tmp1;
			g_drawtrag.y0 = g_drawmsg.y0 + tmp2;
			g_drawtrag.base = abs(tmp1);
			g_drawtrag.height = abs(tmp2);
			
			int g_drawmsg_len = sizeof(struct TRAG_MSG);
			send(g_sock, (char*)&g_drawmsg_len, sizeof(int), 0);
			send(g_sock, (char*)&g_drawtrag, g_drawmsg_len, 0);
		}
		else if (g_drawtype == DRAWISTR) {

			g_drawtrag.type = DRAWISTR;
			g_drawtrag.color = g_drawcolor;
			g_drawtrag.x0 = g_drawmsg.x0 + tmp1;
			g_drawtrag.y0 = g_drawmsg.y0 + tmp2;
			g_drawtrag.base = abs(tmp1);
			g_drawtrag.height = abs(tmp2);

			int g_drawmsg_len = sizeof(struct TRAG_MSG);
			send(g_sock, (char*)&g_drawmsg_len, sizeof(int), 0);
			send(g_sock, (char*)&g_drawtrag, g_drawmsg_len, 0);
		}
		else if (g_drawtype == DRAWLINE) {
			bDrawing = FALSE;
		}
		else {
			int g_drawmsg_len = sizeof(struct DRAW_MSG);
			send(g_sock, (char*)&g_drawmsg_len, sizeof(int), 0);
			send(g_sock, (char*)&g_drawmsg, g_drawmsg_len, 0);
		}
		return 0;
	case WM_DRAWIT:
		hDC = GetDC(hWnd);
		hPen = CreatePen(PS_SOLID, 3, now_drawcolor);

		// ȭ�鿡 �׸���
		hOldPen = (HPEN)SelectObject(hDC, hPen);
		if (now_drawtype == DRAWLINE) {
			MoveToEx(hDC, LOWORD(wParam), HIWORD(wParam), NULL);
			LineTo(hDC, LOWORD(lParam), HIWORD(lParam));
		}
		else if (now_drawtype == DRAWSQUA) {
			MoveToEx(hDC, LOWORD(wParam), HIWORD(wParam), NULL);
			LineTo(hDC, LOWORD(lParam), HIWORD(wParam));
			MoveToEx(hDC, LOWORD(lParam), HIWORD(wParam), NULL);
			LineTo(hDC, LOWORD(lParam), HIWORD(lParam));
			MoveToEx(hDC, LOWORD(lParam), HIWORD(lParam), NULL);
			LineTo(hDC, LOWORD(wParam), HIWORD(lParam));
			MoveToEx(hDC, LOWORD(wParam), HIWORD(lParam), NULL);
			LineTo(hDC, LOWORD(wParam), HIWORD(wParam));
		}
		else if (now_drawtype == DRAWCURC) {
			Ellipse(hDC, LOWORD(wParam), HIWORD(wParam), LOWORD(lParam), HIWORD(lParam));
		}
		else if (now_drawtype == DRAWTRAG) {
			MoveToEx(hDC, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) - HIWORD(lParam), NULL);
			LineTo(hDC, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam));
			MoveToEx(hDC, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam), NULL);
			LineTo(hDC, LOWORD(wParam) + LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam));
			MoveToEx(hDC, LOWORD(wParam) + LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam), NULL);
			LineTo(hDC, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) - HIWORD(lParam));
		}
		else if (now_drawtype == DRAWRHOM) {
			MoveToEx(hDC, LOWORD(wParam), HIWORD(wParam) - HIWORD(lParam), NULL);
			LineTo(hDC, LOWORD(wParam) + LOWORD(lParam), HIWORD(wParam));
			MoveToEx(hDC, LOWORD(wParam) + LOWORD(lParam), HIWORD(wParam), NULL);
			LineTo(hDC, LOWORD(wParam), HIWORD(wParam) + HIWORD(lParam));
			MoveToEx(hDC, LOWORD(wParam), HIWORD(wParam) + HIWORD(lParam), NULL);
			LineTo(hDC, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam));
			MoveToEx(hDC, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam), NULL);
			LineTo(hDC, LOWORD(wParam), HIWORD(wParam) - HIWORD(lParam));
		}
		else if (now_drawtype == DRAWISTR) {
			MoveToEx(hDC, LOWORD(wParam), HIWORD(wParam) - HIWORD(lParam), NULL);
			LineTo(hDC, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam));
			MoveToEx(hDC, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam), NULL);
			LineTo(hDC, LOWORD(wParam) + LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam));
			MoveToEx(hDC, LOWORD(wParam) + LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam), NULL);
			LineTo(hDC, LOWORD(wParam), HIWORD(wParam) - HIWORD(lParam));
		}
		SelectObject(hDC, hOldPen);

		// �޸� ��Ʈ�ʿ� �׸���
		hOldPen = (HPEN)SelectObject(hDCMem, hPen);
		if (now_drawtype == DRAWLINE) {
			MoveToEx(hDCMem, LOWORD(wParam), HIWORD(wParam), NULL);
			LineTo(hDCMem, LOWORD(lParam), HIWORD(lParam));
		}
		else if (now_drawtype == DRAWSQUA) {
			MoveToEx(hDCMem, LOWORD(wParam), HIWORD(wParam), NULL);
			LineTo(hDCMem, LOWORD(lParam), HIWORD(wParam));
			MoveToEx(hDCMem, LOWORD(lParam), HIWORD(wParam), NULL);
			LineTo(hDCMem, LOWORD(lParam), HIWORD(lParam));
			MoveToEx(hDCMem, LOWORD(lParam), HIWORD(lParam), NULL);
			LineTo(hDCMem, LOWORD(wParam), HIWORD(lParam));
			MoveToEx(hDCMem, LOWORD(wParam), HIWORD(lParam), NULL);
			LineTo(hDCMem, LOWORD(wParam), HIWORD(wParam));
		}
		else if (now_drawtype == DRAWCURC) {
			Ellipse(hDCMem, LOWORD(wParam), HIWORD(wParam), LOWORD(lParam), HIWORD(lParam));
		}
		else if (now_drawtype == DRAWTRAG) {//���� �ﰢ��
			MoveToEx(hDCMem, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) - HIWORD(lParam), NULL);
			LineTo(hDCMem, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam));
			MoveToEx(hDCMem, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam), NULL);
			LineTo(hDCMem, LOWORD(wParam) + LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam));
			MoveToEx(hDCMem, LOWORD(wParam) + LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam), NULL);
			LineTo(hDCMem, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) - HIWORD(lParam));
		}
		else if (now_drawtype == DRAWRHOM) {//������
			MoveToEx(hDCMem, LOWORD(wParam), HIWORD(wParam) - HIWORD(lParam), NULL);
			LineTo(hDCMem, LOWORD(wParam)+LOWORD(lParam), HIWORD(wParam));
			MoveToEx(hDCMem, LOWORD(wParam) + LOWORD(lParam), HIWORD(wParam), NULL);
			LineTo(hDCMem, LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam));
			MoveToEx(hDCMem, LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam), NULL);
			LineTo(hDCMem, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam));
			MoveToEx(hDCMem, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam), NULL);
			LineTo(hDCMem, LOWORD(wParam), HIWORD(wParam) - HIWORD(lParam));
		}
		else if (now_drawtype == DRAWISTR) {
			MoveToEx(hDCMem, LOWORD(wParam), HIWORD(wParam) - HIWORD(lParam), NULL);
			LineTo(hDCMem, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam));
			MoveToEx(hDCMem, LOWORD(wParam) - LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam), NULL);
			LineTo(hDCMem, LOWORD(wParam) + LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam));
			MoveToEx(hDCMem, LOWORD(wParam) + LOWORD(lParam), HIWORD(wParam) + HIWORD(lParam), NULL);
			LineTo(hDCMem, LOWORD(wParam), HIWORD(wParam) - HIWORD(lParam));
		}
		now_drawtype = g_drawtype;
		now_drawcolor = g_drawcolor;
		SelectObject(hDCMem, hOldPen);

		DeleteObject(hPen);
		ReleaseDC(hWnd, hDC);
		return 0;
	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);

		// �޸� ��Ʈ�ʿ� ����� �׸��� ȭ�鿡 ����
		GetClientRect(hWnd, &rect);
		BitBlt(hDC, 0, 0, rect.right - rect.left,
			rect.bottom - rect.top, hDCMem, 0, 0, SRCCOPY);

		EndPaint(hWnd, &ps);
		return 0;
	case WM_DESTROY:
		DeleteObject(hBitmap);
		DeleteDC(hDCMem);
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// ����Ʈ ��Ʈ�ѿ� ���ڿ� ���
void DisplayText(char* fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);

	char cbuf[1024];
	vsprintf(cbuf, fmt, arg);

	int nLength = GetWindowTextLength(g_hEditStatus);
	SendMessage(g_hEditStatus, EM_SETSEL, nLength, nLength);
	SendMessage(g_hEditStatus, EM_REPLACESEL, FALSE, (LPARAM)cbuf);

	va_end(arg);
}

// ����� ���� ������ ���� �Լ�
int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

// ���� �Լ� ���� ��� �� ����
void err_quit(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// ���� �Լ� ���� ���
void err_display(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

void change_dtype(char* str) {
	if (!strcmp(str, "��")) {
		g_drawmsg.type = DRAWLINE;
		g_drawtype = DRAWLINE;
		now_drawtype = DRAWLINE;
	}
	else if (!strcmp(str, "�簢��")) {
		g_drawmsg.type = DRAWSQUA;
		g_drawtype = DRAWSQUA;
		now_drawtype = DRAWSQUA;
	}
	else if (!strcmp(str, "��")) {
		g_drawmsg.type = DRAWCURC;
		g_drawtype = DRAWCURC;
		now_drawtype = DRAWCURC;
	}
	else if (!strcmp(str, "�����ﰢ��")) {
		g_drawmsg.type = DRAWTRAG;
		g_drawtype = DRAWTRAG;
		now_drawtype = DRAWTRAG;
	}
	else if (!strcmp(str, "������")) {
		g_drawmsg.type = DRAWRHOM;
		g_drawtype = DRAWRHOM;
		now_drawtype = DRAWRHOM;
	}
	else if (!strcmp(str, "�̵�ﰢ��")) {
		g_drawmsg.type = DRAWISTR;
		g_drawtype = DRAWISTR;
		now_drawtype = DRAWISTR;
	}
		
}
