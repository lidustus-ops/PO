#include <iostream>
#include <vector>
#include <thread>
#include <winsock2.h>
#include <algorithm>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace std::chrono;

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

int receive_all(SOCKET sock, char* buffer, int length) {
    int total = 0;
    while (total < length) {
        int n = recv(sock, buffer + total, length - total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

void process_logic(int start, int end, vector<vector<int>>& matrix) {
    int n = matrix.size();
    for (int j = start; j < end; ++j) {
        int max_val = matrix[0][j];
        for (int i = 1; i < n; ++i) {
            if (matrix[i][j] > max_val) {
                max_val = matrix[i][j];
            }
        }
        matrix[j][j] = max_val;
    }
}

void handle_client(SOCKET client_socket) {
    ClientSession session;
    char command;
    int client_id = (int)client_socket;

    cout << "Client connected. Socket: " << client_id << endl;

    while (recv(client_socket, &command, 1, 0) > 0) {
        if (command == '1') {
            DataHeader h;
            if (receive_all(client_socket, (char*)&h, sizeof(h)) < 0) break;

            int n = ntohl(h.matrix_size);
            session.threads_to_use = ntohl(h.thread_count);
            int bytes = ntohl(h.payload_size);

            vector<int> flat(n * n);
            if (receive_all(client_socket, (char*)flat.data(), bytes) < 0) break;

            session.matrix.assign(n, vector<int>(n));
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    session.matrix[i][j] = ntohl(flat[i * n + j]);
                }
            }
            send(client_socket, "A", 1, 0); 
        }
        else if (command == '2') { 
            if (session.matrix.empty()) {
                send(client_socket, "E", 1, 0);
                continue;
            }

            int n = (int)session.matrix.size();
            int t_cnt = max(1, session.threads_to_use);
            vector<thread> workers;
            int step = n / t_cnt;

            for (int i = 0; i < t_cnt; ++i) {
                int s = i * step;
                int e = (i == t_cnt - 1) ? n : (i + 1) * step;
                if (s < e) {
                    workers.emplace_back(process_logic, s, e, ref(session.matrix));
                }
            }

            for (auto& w : workers) w.join();
            send(client_socket, "A", 1, 0);
            cout << "Client " << client_id << " computation finished." << endl;
        }
        else if (command == '5') break; 
    }

    cout << "Closing connection for " << client_id << endl;
    closesocket(client_socket);
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr = { AF_INET, htons(8080), INADDR_ANY };

    bind(srv, (sockaddr*)&addr, sizeof(addr));
    listen(srv, SOMAXCONN);

    cout << "Server (v2) started on port 8080" << endl;

    while (true) {
        SOCKET cl = accept(srv, nullptr, nullptr);
        if (cl != INVALID_SOCKET) {
            thread(handle_client, cl).detach();
        }
    }

    return 0;
}