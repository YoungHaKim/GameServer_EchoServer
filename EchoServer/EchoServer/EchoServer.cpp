// Game Server Programming Homework 1
// Create echo server using WSAAyncSelect
// Need to create a window to do this, so i decided to create a dialog application

#define _WINSOCKAPI_    // prevent inclusion of winsock.h from windows.h
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <algorithm>
#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include <vector>
#include "resource.h"

#pragma comment(lib, "Ws2_32.lib")
#define WM_SOCKET_NOTIFY (WM_USER + 1)

#define LISTEN_PORT 9001
#define BUFFER_SIZE 4096

BOOL CALLBACK MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
bool InitSocket(HWND hDlg);
bool CreateListenSocket(HWND hDlg);
void OnAccept(HWND hDlg);
void OnClose(HWND hDlg, SOCKET commSock);
void OnRead(SOCKET commSock);
void AddString(HWND hDlg, UINT nID, LPWSTR msg);

SOCKET g_listenSocket = INVALID_SOCKET;
std::vector<SOCKET> g_CommunicationSockets;
unsigned __int64 g_RecvTotal = 0;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpszCmdLine, int nCmdShow)
{
	HWND hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_ECHO_SERVER), NULL, MainDlgProc);

	if (hDlg == NULL)
		return 0;

	ShowWindow(hDlg, SW_SHOW);

	if (!InitSocket(hDlg))
		return 0;

	if (!CreateListenSocket(hDlg))
		return 0;

	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(hDlg, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	closesocket(g_listenSocket);

	WSACleanup();

	return 0;
}

bool InitSocket(HWND hDlg)
{
	WSADATA wsaData;
	WORD targetVersion = MAKEWORD(2, 2);
	int result = WSAStartup(targetVersion, &wsaData);

	if (result == SOCKET_ERROR ||
		targetVersion != wsaData.wVersion)
	{
		MessageBox(hDlg, L"Failed to start winsock!", L"INIT ERROR", MB_OK);
		return false;
	}

	return true;
}
bool CreateListenSocket(HWND hDlg)
{
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	try
	{
		if (listenSocket == INVALID_SOCKET)
			throw L"failed to create listen socket";

		SOCKADDR_IN addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(LISTEN_PORT);
		addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

		if (bind(listenSocket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
			throw L"Bind Error!";

		if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
			throw L"Listen Error!";

		if (WSAAsyncSelect(listenSocket, hDlg, WM_SOCKET_NOTIFY, FD_ACCEPT) == SOCKET_ERROR)
			throw L"WSAAsyncSelect Error";

		g_listenSocket = listenSocket;
	}
	catch (LPCWSTR errorMessage)
	{
		LPVOID lpOSMessage;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			WSAGetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpOSMessage, 0, NULL);
		MessageBox(hDlg, (LPCTSTR)lpOSMessage, errorMessage, MB_OK);
		LocalFree(lpOSMessage);

		closesocket(listenSocket);

		return false;
	}

	return true;
}

BOOL CALLBACK MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDCANCEL:
		case IDCLOSE:
			DestroyWindow(hDlg);
			PostQuitMessage(0);
			break;

		default:
			break;
		}
		return TRUE;

	case WM_SOCKET_NOTIFY:
		switch (WSAGETSELECTEVENT(lParam))
		{
		case FD_ACCEPT:
			OnAccept(hDlg);
			break;
		case FD_READ:
			OnRead((SOCKET)wParam);
			break;
		case FD_CLOSE:
			OnClose(hDlg, (SOCKET)wParam);
			break;

		default:
			break;
		}
		return TRUE;


	default:
		break;
	}
	return FALSE;
}

void OnAccept(HWND hDlg)
{
	sockaddr_in clientAddr;
	int length = sizeof(clientAddr);
	SOCKET commSock = accept(g_listenSocket, (sockaddr*)&clientAddr, &length);

	WSAAsyncSelect(commSock, hDlg, WM_SOCKET_NOTIFY, FD_READ | FD_CLOSE);

	g_CommunicationSockets.push_back(commSock);

	WCHAR msg[128] = { 0 };

	char addrStr[20] = { 0 };
	inet_ntop(AF_INET, &(clientAddr.sin_addr), addrStr, INET_ADDRSTRLEN);

	wsprintf(msg, 
		L"Client:[%S:%d], Users:[%d] \n", 
		addrStr,
		ntohs(clientAddr.sin_port),
		g_CommunicationSockets.size());
	AddString(hDlg,
		IDC_CONN_LIST,
		msg);
}
void OnClose(HWND hDlg, SOCKET commSock)
{
	closesocket(commSock);
	g_CommunicationSockets.erase(
		std::remove(g_CommunicationSockets.begin(), 
			g_CommunicationSockets.end(), 
			commSock), 
		g_CommunicationSockets.end());

	WCHAR msg[128] = { 0 };
	wsprintf(msg,
		L"Socket Closed, Users:[%d] \n",
		g_CommunicationSockets.size());
	AddString(hDlg,
		IDC_CONN_LIST,
		msg);

	if (g_CommunicationSockets.size() == 0)
	{
		WCHAR statStr[128] = { 0 };
		wsprintf(statStr,
			L"Total Recv Bytes:[%Iu]",
			g_RecvTotal);
		AddString(hDlg,
			IDC_CONN_LIST,
			statStr);
	}
}
void OnRead(SOCKET commSock)
{
	char buf[BUFFER_SIZE];
	int bytesRead = recv(commSock, buf, sizeof(buf), 0);
	if (bytesRead == SOCKET_ERROR)
		return;

	g_RecvTotal += bytesRead;

	send(commSock, buf, bytesRead, 0);

}
void AddString(HWND hDlg, UINT nID, LPWSTR msg)
{
	HWND hList = GetDlgItem(hDlg, nID);
	int n = (int)SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)msg);
	SendMessage(hList, LB_SETTOPINDEX, n, 0);
}