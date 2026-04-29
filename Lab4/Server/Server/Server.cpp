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
    bool is_ready = false;
    bool is_busy = false;
    double last_duration = 0.0;
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

    cout << "New connection. Socket ID: " << client_id << endl;

    while (recv(client_socket, &command, 1, 0) > 0) {
        if (command == '1') {
            DataHeader h;
            if (receive_all(client_socket, (char*)&h, sizeof(h)) < 0) break;

            int n = ntohl(h.matrix_size);
            session.threads_to_use = ntohl(h.thread_count);
            int bytes = ntohl(h.payload_size);

            cout << "Client " << client_id << " sent matrix " << n << "x" << n
                << " (Threads: " << session.threads_to_use << ")" << endl;

            vector<int> flat(n * n);
            if (receive_all(client_socket, (char*)flat.data(), bytes) < 0) break;

            session.matrix.assign(n, vector<int>(n));
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    session.matrix[i][j] = ntohl(flat[i * n + j]);
                }
            }

            session.is_ready = false;
            send(client_socket, "A", 1, 0);
        }
        else if (command == '2') {
            if (session.matrix.empty()) {
                cout << "Client " << client_id << " tried to compute without data!" << endl;
                send(client_socket, "E", 1, 0);
                continue;
            }

            cout << "Starting calculation for Client " << client_id << "..." << endl;
            session.is_busy = true;

            auto start_time = high_resolution_clock::now();

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

            auto end_time = high_resolution_clock::now();
            session.last_duration = duration_cast<microseconds>(end_time - start_time).count() / 1000.0;

            session.is_busy = false;
            session.is_ready = true;
            cout << "Computation done in " << session.last_duration << " ms." << endl;
            send(client_socket, "A", 1, 0);
        }
        else if (command == '3') {
            char status = session.is_busy ? 'P' : (session.is_ready ? 'R' : 'I');
            send(client_socket, &status, 1, 0);
        }
        else if (command == '4') {
            if (!session.is_ready) {
                cout << "Client " << client_id << " requested result before computation!" << endl;
                send(client_socket, "E", 1, 0);
                continue;
            }

            cout << "Sending result matrix to Client " << client_id << endl;
            send(client_socket, "G", 1, 0);

            send(client_socket, (char*)&session.last_duration, sizeof(double), 0);

            uint32_t n = (uint32_t)session.matrix.size();
            uint32_t net_len = htonl(n * n * sizeof(int));
            send(client_socket, (char*)&net_len, 4, 0);

            for (auto& row : session.matrix) {
                for (int val : row) {
                    int net_v = htonl(val);
                    send(client_socket, (char*)&net_v, 4, 0);
                }
            }
        }
        else if (command == '5') {
            cout << "Client " << client_id << " requested disconnect." << endl;
            break;
        }
    }

    cout << "Connection closed for Client " << client_id << endl;
    closesocket(client_socket);
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr = { AF_INET, htons(8080), INADDR_ANY };

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cout << "Bind failed!" << endl;
        return 1;
    }

    listen(srv, SOMAXCONN);

    cout << "       Server started on port: 8080" << endl;
    cout << "       Waiting for connections..." << endl;

    while (true) {
        SOCKET cl = accept(srv, nullptr, nullptr);
        if (cl != INVALID_SOCKET) {
            thread(handle_client, cl).detach();
        }
    }

    return 0;
}