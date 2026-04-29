#include <iostream>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iomanip>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#pragma pack(push, 1)
struct DataHeader {
    uint32_t matrixsize;
    uint32_t thread_count;
    uint32_t payload_size;
};
#pragma pack(pop)

int receive_all(SOCKET sock, char* buffer, int length) {
    int total = 0;
    while (total < length) {
        int n = recv(sock, buffer + total, length - total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

void printMatrix(const vector<vector<int>>& m) {
    if (m.empty()) return;
    cout << "\n--- Matrix (" << m.size() << "x" << m.size() << ") ---" << endl;
    for (const auto& row : m) {
        for (int val : row) {
            cout << setw(6) << val;
        }
        cout << endl;
    }
    cout << "--------------------------" << endl;
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Winsock initialization failed!" << endl;
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cout << "Socket creation failed!" << endl;
        return 1;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cout << "Connection to server failed!" << endl;
        WSACleanup();
        return 1;
    }

    cout << "Connected to server successfully." << endl;

    vector<vector<int>> matrix;
    int n = 0;
    bool data_sent = false, computed = false, running = true;

    while (running) {
        cout << "\nMenu:\n1. Send Data, 2. Compute, 3. Check Status, 4. Get Result, 5. Exit\nChoice: ";
        char choice;
        cin >> choice;

        switch (choice) {
        case '1': {
            int temp_n, temp_t;
            cout << "Enter Matrix Size (N): "; cin >> temp_n;
            cout << "Enter Thread Count: "; cin >> temp_t;

            if (temp_n <= 0 || temp_t <= 0) {
                cout << "Size and threads must be positive!" << endl;
                continue;
            }

            n = temp_n;
            matrix.assign(n, vector<int>(n));
            vector<int> flat;

            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) {
                    matrix[i][j] = rand() % 100;
                    flat.push_back(htonl(matrix[i][j]));
                }
            }

            printMatrix(matrix);

            send(sock, "1", 1, 0);
            DataHeader h = {
                (uint32_t)htonl(n),
                (uint32_t)htonl(temp_t),
                (uint32_t)htonl(flat.size() * sizeof(int))
            };

            send(sock, (char*)&h, sizeof(h), 0);
            send(sock, (char*)flat.data(), flat.size() * sizeof(int), 0);

            char ack;
            recv(sock, &ack, 1, 0);
            if (ack == 'A') {
                data_sent = true;
                computed = false;
                cout << "Data accepted by server." << endl;
            }
            break;
        }

        case '2': {
            if (!data_sent) {
                cout << "Send data first (Option 1)!" << endl;
                continue;
            }
            send(sock, "2", 1, 0);
            char ack;
            recv(sock, &ack, 1, 0);
            if (ack == 'A') {
                computed = true;
                cout << "Server started computing." << endl;
            }
            else {
                cout << "Server failed to start computation." << endl;
            }
            break;
        }

        case '3': {
            send(sock, "3", 1, 0);
            char s;
            recv(sock, &s, 1, 0);
            string status_str = (s == 'R') ? "Ready" : (s == 'P' ? "Processing" : "Idle");
            cout << "Server is: " << status_str << endl;
            break;
        }

        case '4': {
            if (!computed) {
                cout << "Run computation first (Option 2)!" << endl;
                continue;
            }
            send(sock, "4", 1, 0);
            char res_cmd;
            recv(sock, &res_cmd, 1, 0);

            if (res_cmd == 'G') {
                double server_time = 0.0;
                recv(sock, (char*)&server_time, sizeof(double), 0);
                cout << "Server execution time: " << server_time << " ms" << endl;

                uint32_t net_len;
                recv(sock, (char*)&net_len, 4, 0);
                uint32_t data_len = ntohl(net_len);

                vector<int> res_f(data_len / sizeof(int));
                if (receive_all(sock, (char*)res_f.data(), data_len) > 0) {
                    for (int i = 0; i < n; i++) {
                        for (int j = 0; j < n; j++) {
                            matrix[i][j] = ntohl(res_f[i * n + j]);
                        }
                    }
                    printMatrix(matrix);
                }
            }
            else {
                cout << "Server is not ready to give results." << endl;
            }
            break;
        }

        case '5': {
            send(sock, "5", 1, 0);
            running = false;
            break;
        }

        default:
            cout << "Unknown command!" << endl;
        }
    }

    closesocket(sock);
    WSACleanup();
    cout << "Client disconnected." << endl;
    return 0;
}