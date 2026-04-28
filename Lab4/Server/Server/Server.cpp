#include <iostream>
#include <vector>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#pragma pack(push, 1)
struct DataHeader {
    uint32_t matrix_size;
    uint32_t thread_count;
    uint32_t payload_size;
};
#pragma pack(pop)

struct ClientSession {
    vector<vector<int>> matrix;
    int threads_to_use = 1;
};

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr = { AF_INET, htons(55555), INADDR_ANY };

    bind(srv, (sockaddr*)&addr, sizeof(addr));
    listen(srv, SOMAXCONN);

    cout << "Server initialized. Waiting for connections..." << endl;

    while (true) {
        SOCKET cl = accept(srv, nullptr, nullptr);
        if (cl != INVALID_SOCKET) {
            cout << "New client connected!" << endl;
            closesocket(cl);
        }
    }

    closesocket(srv);
    WSACleanup();
    return 0;
}