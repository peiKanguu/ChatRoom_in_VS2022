#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define MAX_LEN 200  // 最大消息长度
#define NUM_COLORS 6 // 颜色数量

using namespace std;

// 客户端结构体，包含客户端ID、名字、套接字和线程
struct terminal {
    int id;
    string name;
    SOCKET socket;
    thread th;

    // 构造函数
    terminal(int id, const string& name, SOCKET socket, thread&& th)
        : id(id), name(name), socket(socket), th(move(th)) {}
};

vector<terminal> clients; // 存储所有连接的客户端
string def_col = "\033[0m"; // 默认颜色
string colors[] = { "\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m","\033[36m" };
int seed = 0; // 用于生成唯一ID的种子
mutex cout_mtx, clients_mtx; // 互斥锁，保证线程安全

// 函数声明
string color(int code);
void set_name(int id, const string& name);
void shared_print(const string& str, bool endLine = true);
int broadcast_message(const string& message, int sender_id);
int broadcast_message(int num, int sender_id);
void end_connection(int id);
void handle_client(SOCKET client_socket, int id);

int main()
{
    // 初始化Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed: " << iResult << endl;
        return 1;
    }

    // 创建服务器套接字
    SOCKET server_socket;
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        cerr << "socket: " << WSAGetLastError() << endl;
        WSACleanup();
        return -1;
    }

    // 设置服务器地址和端口
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(10000);
    server.sin_addr.s_addr = INADDR_ANY;
    memset(&server.sin_zero, 0, sizeof(server.sin_zero));

    // 绑定服务器套接字
    if (bind(server_socket, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
    {
        cerr << "bind error: " << WSAGetLastError() << endl;
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }

    // 监听客户端连接
    if (listen(server_socket, 8) == SOCKET_ERROR)
    {
        cerr << "listen error: " << WSAGetLastError() << endl;
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }

    // 接受客户端连接
    struct sockaddr_in client;
    SOCKET client_socket;
    int len = sizeof(sockaddr_in);

    cout << colors[NUM_COLORS - 1] << "\n\t  ====== 欢迎来到聊天室 ======   " << endl << def_col;

    // 无限循环，等待客户端连接
    while (1)
    {
        if ((client_socket = accept(server_socket, (struct sockaddr*)&client, &len)) == INVALID_SOCKET)
        {
            cerr << "accept error: " << WSAGetLastError() << endl;
            closesocket(server_socket);
            WSACleanup();
            return -1;
        }
        seed++;
        thread t(handle_client, client_socket, seed); // 创建新线程处理客户端
        {
            lock_guard<mutex> guard(clients_mtx);
            clients.emplace_back(seed, "Anonymous", client_socket, move(t)); // 将新客户端添加到列表中
        }
    }

    // 等待所有客户端线程结束
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].th.joinable())
            clients[i].th.join();
    }

    // 关闭服务器套接字
    closesocket(server_socket);
    WSACleanup();
    return 0;
}

// 根据代码生成颜色
string color(int code)
{
    return colors[code % NUM_COLORS];
}

// 设置客户端名称
void set_name(int id, const string& name)
{
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id == id)
        {
            clients[i].name = name;
        }
    }
}

// 同步输出，确保不会发生输出混乱
void shared_print(const string& str, bool endLine)
{
    lock_guard<mutex> guard(cout_mtx);
    cout << str;
    if (endLine)
        cout << endl;
}

// 广播消息给除了发送者之外的所有客户端
int broadcast_message(const string& message, int sender_id)
{
    char temp[MAX_LEN];
    strcpy_s(temp, message.c_str());
    lock_guard<mutex> guard(clients_mtx); // 确保对客户端列表的访问时线程安全的
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id != sender_id)
        {
            send(clients[i].socket, temp, sizeof(temp), 0);
        }
    }
    return 0;
}

// 广播数字给除了发送者之外的所有客户端
int broadcast_message(int num, int sender_id)
{
    lock_guard<mutex> guard(clients_mtx);
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id != sender_id)
        {
            send(clients[i].socket, (char*)&num, sizeof(num), 0);
        }
    }
    return 0;
}

// 结束与客户端的连接
void end_connection(int id)
{
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id == id)
        {
            lock_guard<mutex> guard(clients_mtx);
            clients[i].th.detach(); // 分离线程
            closesocket(clients[i].socket); // 关闭套接字
            clients.erase(clients.begin() + i); // 从客户端列表中移除
            break;
        }
    }
}

// 处理客户端消息
void handle_client(SOCKET client_socket, int id)
{
    char name[MAX_LEN], str[MAX_LEN];
    recv(client_socket, name, sizeof(name), 0); // 接收客户端名称
    set_name(id, name);

    // 显示欢迎消息
    string welcome_message = string(name) + string(" 加入了聊天");
    broadcast_message("#NULL", id);
    broadcast_message(id, id);
    broadcast_message(welcome_message, id);
    shared_print(color(id) + welcome_message + def_col);

    // 接收和发送消息
    while (1)
    {
        int bytes_received = recv(client_socket, str, sizeof(str), 0);
        if (bytes_received <= 0)
            return;
        if (strcmp(str, "#exit") == 0)
        {
            // 显示离开消息
            string message = string(name) + string(" 离开了聊天");
            broadcast_message("#NULL", id);
            broadcast_message(id, id);
            broadcast_message(message, id);
            shared_print(color(id) + message + def_col);
            end_connection(id);
            return;
        }
        broadcast_message(string(name), id);
        broadcast_message(id, id);
        broadcast_message(string(str), id);
        shared_print(color(id) + name + " : " + def_col + str);
    }
}
