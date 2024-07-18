#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define MAX_LEN 200  // �����Ϣ����
#define NUM_COLORS 6 // ��ɫ����

using namespace std;

// �ͻ��˽ṹ�壬�����ͻ���ID�����֡��׽��ֺ��߳�
struct terminal {
    int id;
    string name;
    SOCKET socket;
    thread th;

    // ���캯��
    terminal(int id, const string& name, SOCKET socket, thread&& th)
        : id(id), name(name), socket(socket), th(move(th)) {}
};

vector<terminal> clients; // �洢�������ӵĿͻ���
string def_col = "\033[0m"; // Ĭ����ɫ
string colors[] = { "\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m","\033[36m" };
int seed = 0; // ��������ΨһID������
mutex cout_mtx, clients_mtx; // ����������֤�̰߳�ȫ

// ��������
string color(int code);
void set_name(int id, const string& name);
void shared_print(const string& str, bool endLine = true);
int broadcast_message(const string& message, int sender_id);
int broadcast_message(int num, int sender_id);
void end_connection(int id);
void handle_client(SOCKET client_socket, int id);

int main()
{
    // ��ʼ��Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed: " << iResult << endl;
        return 1;
    }

    // �����������׽���
    SOCKET server_socket;
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        cerr << "socket: " << WSAGetLastError() << endl;
        WSACleanup();
        return -1;
    }

    // ���÷�������ַ�Ͷ˿�
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(10000);
    server.sin_addr.s_addr = INADDR_ANY;
    memset(&server.sin_zero, 0, sizeof(server.sin_zero));

    // �󶨷������׽���
    if (bind(server_socket, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
    {
        cerr << "bind error: " << WSAGetLastError() << endl;
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }

    // �����ͻ�������
    if (listen(server_socket, 8) == SOCKET_ERROR)
    {
        cerr << "listen error: " << WSAGetLastError() << endl;
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }

    // ���ܿͻ�������
    struct sockaddr_in client;
    SOCKET client_socket;
    int len = sizeof(sockaddr_in);

    cout << colors[NUM_COLORS - 1] << "\n\t  ====== ��ӭ���������� ======   " << endl << def_col;

    // ����ѭ�����ȴ��ͻ�������
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
        thread t(handle_client, client_socket, seed); // �������̴߳���ͻ���
        {
            lock_guard<mutex> guard(clients_mtx);
            clients.emplace_back(seed, "Anonymous", client_socket, move(t)); // ���¿ͻ�����ӵ��б���
        }
    }

    // �ȴ����пͻ����߳̽���
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].th.joinable())
            clients[i].th.join();
    }

    // �رշ������׽���
    closesocket(server_socket);
    WSACleanup();
    return 0;
}

// ���ݴ���������ɫ
string color(int code)
{
    return colors[code % NUM_COLORS];
}

// ���ÿͻ�������
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

// ͬ�������ȷ�����ᷢ���������
void shared_print(const string& str, bool endLine)
{
    lock_guard<mutex> guard(cout_mtx);
    cout << str;
    if (endLine)
        cout << endl;
}

// �㲥��Ϣ�����˷�����֮������пͻ���
int broadcast_message(const string& message, int sender_id)
{
    char temp[MAX_LEN];
    strcpy_s(temp, message.c_str());
    lock_guard<mutex> guard(clients_mtx); // ȷ���Կͻ����б�ķ���ʱ�̰߳�ȫ��
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id != sender_id)
        {
            send(clients[i].socket, temp, sizeof(temp), 0);
        }
    }
    return 0;
}

// �㲥���ָ����˷�����֮������пͻ���
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

// ������ͻ��˵�����
void end_connection(int id)
{
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id == id)
        {
            lock_guard<mutex> guard(clients_mtx);
            clients[i].th.detach(); // �����߳�
            closesocket(clients[i].socket); // �ر��׽���
            clients.erase(clients.begin() + i); // �ӿͻ����б����Ƴ�
            break;
        }
    }
}

// ����ͻ�����Ϣ
void handle_client(SOCKET client_socket, int id)
{
    char name[MAX_LEN], str[MAX_LEN];
    recv(client_socket, name, sizeof(name), 0); // ���տͻ�������
    set_name(id, name);

    // ��ʾ��ӭ��Ϣ
    string welcome_message = string(name) + string(" ����������");
    broadcast_message("#NULL", id);
    broadcast_message(id, id);
    broadcast_message(welcome_message, id);
    shared_print(color(id) + welcome_message + def_col);

    // ���պͷ�����Ϣ
    while (1)
    {
        int bytes_received = recv(client_socket, str, sizeof(str), 0);
        if (bytes_received <= 0)
            return;
        if (strcmp(str, "#exit") == 0)
        {
            // ��ʾ�뿪��Ϣ
            string message = string(name) + string(" �뿪������");
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
