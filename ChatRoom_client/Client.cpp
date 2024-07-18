#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <signal.h>

#pragma comment(lib, "Ws2_32.lib")

#define MAX_LEN 200  // 最大消息长度
#define NUM_COLORS 6 // 颜色数量

using namespace std;

bool exit_flag = false; // 退出标志
thread t_send, t_recv; // 发送和接收线程
SOCKET client_socket; // 客户端套接字
string def_col = "\033[0m"; // 默认颜色
string colors[] = { "\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m" }; // 颜色列表

// 函数声明
void catch_ctrl_c(int signal);
string color(int code);
int eraseText(int cnt);
void send_message(SOCKET client_socket);
void recv_message(SOCKET client_socket);

int main()
{
	// 初始化Winsock
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		cout << "WSAStartup failed: " << iResult << endl;
		return 1;
	}

	// 创建套接字
	client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket == INVALID_SOCKET)
	{
		cout << "Socket creation failed: " << WSAGetLastError() << endl;
		WSACleanup();
		return 1;
	}

	// 设置服务器地址和端口
	struct sockaddr_in client; // 服务器地址
	client.sin_family = AF_INET; // IPv4
	client.sin_port = htons(10000); // 服务器端口号
	inet_pton(AF_INET, "127.0.0.1", &client.sin_addr); // 提供服务器IP地址
	memset(&client.sin_zero, 0, sizeof(client.sin_zero));

	// 连接到服务器
	if (connect(client_socket, (struct sockaddr*)&client, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		cout << "Connect failed: " << WSAGetLastError() << endl;
		closesocket(client_socket);
		WSACleanup();
		return 1;
	}

	// 捕捉Ctrl + C信号
	signal(SIGINT, catch_ctrl_c);

	// 输入用户名并发送给服务器
	char name[MAX_LEN];
	cout << "Enter your name : ";
	cin.getline(name, MAX_LEN);
	send(client_socket, name, sizeof(name), 0);

	cout << colors[NUM_COLORS - 1] << "\n\t  ====== 欢迎来到聊天室 ======   " << endl << def_col;

	// 创建发送和接收线程
	thread t1(send_message, client_socket);
	thread t2(recv_message, client_socket);

	t_send = move(t1);
	t_recv = move(t2);

	if (t_send.joinable())
		t_send.join();
	if (t_recv.joinable())
		t_recv.join();

	closesocket(client_socket);
	WSACleanup();
	return 0;
}

// 捕捉Ctrl + C信号的处理函数
void catch_ctrl_c(int signal)
{
	char str[MAX_LEN] = "#exit";
	send(client_socket, str, sizeof(str), 0);
	exit_flag = true;
	t_send.detach();
	t_recv.detach();
	closesocket(client_socket);
	WSACleanup();
	exit(signal);
}

// 根据代码返回对应颜色
string color(int code)
{
	return colors[code % NUM_COLORS];
}

// 从终端中删除文本
int eraseText(int cnt)
{
	char back_space = 8;
	for (int i = 0; i < cnt; i++)
	{
		cout << back_space;
	}
	return 0;
}

// 发送消息给所有人
void send_message(SOCKET client_socket)
{
	while (1)
	{
		cout << colors[1] << "You : " << def_col;
		char str[MAX_LEN];
		cin.getline(str, MAX_LEN);
		send(client_socket, str, sizeof(str), 0);
		if (strcmp(str, "#exit") == 0)
		{
			exit_flag = true;
			t_recv.detach();
			closesocket(client_socket);
			return;
		}
	}
}

// 接收消息
void recv_message(SOCKET client_socket)
{
	while (1)
	{
		if (exit_flag)
			return;
		char name[MAX_LEN], str[MAX_LEN];
		int color_code;
		int bytes_received = recv(client_socket, name, sizeof(name), 0);
		if (bytes_received <= 0)
			continue;
		recv(client_socket, (char*)&color_code, sizeof(color_code), 0);
		recv(client_socket, str, sizeof(str), 0);
		eraseText(6);
		if (strcmp(name, "#NULL") != 0)
			cout << color(color_code) << name << " : " << def_col << str << endl;
		else
			cout << color(color_code) << str << endl;
		cout << colors[1] << "You : " << def_col;
		fflush(stdout);
	}
}
