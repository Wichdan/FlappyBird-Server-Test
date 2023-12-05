#include <iostream>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

struct GameObject {
    int id;
    int x;
    int y;
};

struct ClientInfo {
    SOCKET socket;
    GameObject object;
};

std::vector<ClientInfo> clients;

void SendData(SOCKET clientSocket, const char* data, int dataSize) {
    send(clientSocket, data, dataSize, 0);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return -1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create server socket." << std::endl;
        WSACleanup();
        return -1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }

    std::cout << "Server listening on port 8888..." << std::endl;

    // Loop untuk menerima koneksi dari klien
    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed." << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            return -1;
        }

        std::cout << "Client connected." << std::endl;

        // Buat ClientInfo untuk menyimpan informasi klien
        ClientInfo newClient;
        newClient.socket = clientSocket;
        newClient.object.id = static_cast<int>(clients.size()) + 1;
        newClient.object.x = 0;
        newClient.object.y = 0;

        // Tambahkan klien ke vektor clients
        clients.push_back(newClient);

        // Kirim ID klien ke klien yang baru terkoneksi
        SendData(clientSocket, reinterpret_cast<const char*>(&newClient.object), sizeof(GameObject));

        // Event handling
        WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
        events[0] = WSACreateEvent(); // Event untuk koneksi masuk
        events[1] = WSACreateEvent(); // Event untuk data yang dapat dibaca

        WSAEventSelect(clientSocket, events[1], FD_READ | FD_CLOSE);
        WSAEventSelect(serverSocket, events[0], FD_ACCEPT);


        while (true) {
            DWORD index = WSAWaitForMultipleEvents(2, events, FALSE, WSA_INFINITE, FALSE);

            if (index == WSA_WAIT_FAILED) {
                std::cerr << "WSAWaitForMultipleEvents failed." << std::endl;
                break;
            }

            index -= WSA_WAIT_EVENT_0;

            if (index == 0) {
                // Koneksi masuk
                SOCKET newClientSocket = accept(serverSocket, NULL, NULL);
                if (newClientSocket != INVALID_SOCKET) {
                    std::cout << "Client connected." << std::endl;

                    ClientInfo newClient;
                    newClient.socket = newClientSocket;
                    newClient.object.id = static_cast<int>(clients.size()) + 1;
                    newClient.object.x = 0;
                    newClient.object.y = 0;

                    clients.push_back(newClient);

                    // Kirim ID klien ke klien yang baru terkoneksi
                    SendData(newClientSocket, reinterpret_cast<const char*>(&newClient.object), sizeof(GameObject));

                    WSAEventSelect(newClientSocket, events[1], FD_READ | FD_CLOSE);
                }
            }
            else if (index == 1) {
                // Data yang dapat dibaca atau koneksi tertutup
                WSANETWORKEVENTS netEvents;
                WSAEnumNetworkEvents(clientSocket, NULL, &netEvents);

                if (netEvents.lNetworkEvents & FD_READ) {
                    // Terima data dari klien
                    char buffer[sizeof(GameObject)];
                    int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
                    if (bytesRead > 0) {
                        // Data diterima, lakukan replikasi objek
                        GameObject* receivedObj = reinterpret_cast<GameObject*>(buffer);

                        for (auto& client : clients) {
                            // Replikasi objek ke semua klien
                            if (client.socket != clientSocket) {
                                SendData(client.socket, reinterpret_cast<const char*>(receivedObj), sizeof(GameObject));
                                cout << "Replikasi Object? " << endl;
                            }
                        }
                        cout << clientSocket << ": " << bytesRead << endl;
                    }
                }

                if (netEvents.lNetworkEvents & FD_CLOSE) {
                    // Koneksi tertutup
                    std::cout << "Client disconnected." << std::endl;

                    closesocket(clientSocket);

                    // Hapus klien dari vektor clients
                    clients.erase(std::remove_if(clients.begin(), clients.end(),
                        [clientSocket](const ClientInfo& client) {
                            return client.socket == clientSocket;
                        }), clients.end());

                    break;
                }
            }

            WSAResetEvent(events[index]);
        }
    }

    // Clean up
    for (const auto& client : clients) {
        closesocket(client.socket);
    }
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}
