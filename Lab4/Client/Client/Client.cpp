#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#pragma pack(push, 1)
struct DataHeader {
    uint32_t matrix_size;
    uint32_t thread_count;
    uint32_t payload_size;
};
#pragma pack(pop)

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(55555);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cout << "Server is offline." << endl;
    }
    else {
        cout << "Connected to server!" << endl;
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}