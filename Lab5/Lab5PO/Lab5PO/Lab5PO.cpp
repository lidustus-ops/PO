#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int SERVER_PORT = 8888;
const int RECV_BUFFER = 1024;

string load_file_content(const string& fileName) {
    string folder = "www";
    string fullPath = folder + fileName;
    ifstream targetFile(fullPath, ios::binary);
    if (!targetFile.is_open()) return "";
    stringstream buffer;
    buffer << targetFile.rdbuf();
    return buffer.str();
}

void process_connection(SOCKET clientNode) {
    char incomingData[RECV_BUFFER] = { 0 };
    int result = recv(clientNode, incomingData, RECV_BUFFER, 0);
    if (result > 0) {
        istringstream requestStream(incomingData);
        string action, route, version;
        requestStream >> action >> route >> version;
        if (route == "/") route = "/index.html";

        string fileData = load_file_content(route);
        string finalResponse;
        if (!fileData.empty()) {
            finalResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
            finalResponse += "Content-Length: " + to_string(fileData.size()) + "\r\n";
            finalResponse += "Connection: close\r\n\r\n" + fileData;
        }
        else {
            string errorMsg = "<h1>404 - Storinka ne znaydena</h1>";
            finalResponse = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n";
            finalResponse += "Content-Length: " + to_string(errorMsg.size()) + "\r\n\r\n" + errorMsg;
        }
        send(clientNode, finalResponse.c_str(), (int)finalResponse.size(), 0);
    }
    closesocket(clientNode);
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(SERVER_PORT);
    bind(listener, (struct sockaddr*)&service, sizeof(service));
    listen(listener, SOMAXCONN);

    cout << "Web-server is running on port " << SERVER_PORT << "..." << endl;

    while (true) {
        SOCKET session = accept(listener, NULL, NULL);
        if (session != INVALID_SOCKET) {
            process_connection(session); 
        }
    }
    closesocket(listener);
    WSACleanup();
    return 0;
}