#define SECURITY_WIN32
#include <fstream>
#include <strstream>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>


#include <iostream>
#include <string>
#include <thread>
#include <sstream>
#include <string>


///This is only for windows. Some of it was ChatGPT but I had to debug on my own as chatgpt does not give the correct solutions alot of the time. Especially with tasks like this.
#pragma comment(lib, "ws2_32.lib")

//#pragma comment(lib, "iphlpapi.lib")
//#pragma comment(lib, "winhttp.lib")

const int PORT = 8080;

std::string Students[10];
int Marks[10][2];

std::string extractValue(const std::string& json, const std::string& key) {
    std::string searchStr = "\"" + key + "\":";
    size_t start = json.find(searchStr);
    if (start == std::string::npos) return ""; // Key not found

    start += searchStr.length();
    size_t end = json.find_first_of(",}", start); // Find end of value
    std::string value = json.substr(start, end - start);

    // Remove quotes if it's a string
    if (value.front() == '"') {
        value = value.substr(1, value.length() - 2);
    }

    return value;
}

void processStudentsData(const std::string& jsonData) {
    std::size_t start = 0;
    std::size_t end = 0;

  
    while ((start = jsonData.find("{", end)) != std::string::npos) {
        end = jsonData.find("}", start);
        if (end == std::string::npos) break; 

        std::string studentObject = jsonData.substr(start, end - start + 1);

      
        std::string name = extractValue(studentObject, "name");


        std::string marksKey = "marks";
        std::string marksArray = extractValue(studentObject, marksKey);

    
        size_t startMark1 = marksArray.find("[");
        size_t endMark1 = marksArray.find(",", startMark1);
        std::string mark1 = marksArray.substr(startMark1 + 1, endMark1 - startMark1 - 1);

        size_t startMark2 = endMark1 + 1; 
        size_t endMark2 = marksArray.find("]", startMark2);
        std::string mark2 = marksArray.substr(startMark2 + 1, endMark2 - startMark2 - 1);

        std::cout << "Student Name: " << name << ", Mark 1: " << mark1 << ", Mark 2: " << mark2 << std::endl;
    }
}



std::string GetClientIPAddress(sockaddr_in& clientAddr) {
    char ipStr[INET_ADDRSTRLEN]; // Buffer to hold the IP string
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr)); // Convert to string
    return std::string(ipStr); // Return the IP as a std::string
}


std::string LoadFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}


void HandleClient(SOCKET clientSocket) {
    try {
        char buffer[1024];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        std::cout << std::endl;
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            //outputting info
            std::string request(buffer);

            std::istringstream requestStream(request);

            std::string requestLine;

            std::string header;

            while (std::getline(requestStream, header) && header != "\r") {
                std::cout << "Header: " << header << std::endl;
            }
            //////

            std::getline(requestStream, requestLine);

            std::cout << "Request Line: " << requestLine << std::endl;
            if (request.find("GET /script.js") != std::string::npos) {
                std::string jsContent = LoadFile("script.js");
                if (!jsContent.empty()) {
                    std::string httpResponse =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/javascript\r\n"
                        "Content-Length: " + std::to_string(jsContent.length()) + "\r\n"
                        "Connection: close\r\n"
                        "\r\n" + jsContent;
                    send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
                }

            }
            else if (request.find("POST /api/students") != std::string::npos) {
                std::size_t pos = request.find("\r\n\r\n");
                if (pos != std::string::npos) {
                    std::string jsonData = request.substr(pos + 4); // Skip the header
                    std::cout << "JSON data:\n" << jsonData << std::endl;

                    processStudentsData(jsonData);

                    // Send a response back to the client
                    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"message\": \"Data received successfully\"}";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
            }
            else if ((request.find("GET /home") != std::string::npos)) {
                std::string htmlContent = LoadFile("home.html");
                if (!htmlContent.empty()) {
                    std::string httpResponse =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: " + std::to_string(htmlContent.length()) + "\r\n"
                        "Connection: close\r\n"
                        "\r\n" + htmlContent;
                    send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
                }
                else {
                    std::string errorResponse =
                        "HTTP/1.1 404 Not Found\r\n"
                        "Content-Type: text/plain\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "404 Not Found\n";
                    send(clientSocket, errorResponse.c_str(), errorResponse.length(), 0);
                }
            }

            else if (request.find("GET /students") != std::string::npos) {
                std::string htmlContent = LoadFile("students.html");
                if (!htmlContent.empty()) {
                    std::string httpResponse =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: " + std::to_string(htmlContent.length()) + "\r\n"
                        "Connection: close\r\n"
                        "\r\n" + htmlContent;
                    send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
                }
            }


            else if (request.find("GET /finalmarks" ) != std::string::npos) {
                std::string htmlContent = LoadFile("finalmarks.html");
                if (!htmlContent.empty()) {
                    std::string httpResponse =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: " + std::to_string(htmlContent.length()) + "\r\n"
                        "Connection: close\r\n"
                        "\r\n" + htmlContent;
                    send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
                }
            }

            else if (request.find("GET /distinctive" ) != std::string::npos) {
                std::string htmlContent = LoadFile("distinctive.html");
                if (!htmlContent.empty()) {
                    std::string httpResponse =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: " + std::to_string(htmlContent.length()) + "\r\n"
                        "Connection: close\r\n"
                        "\r\n" + htmlContent;
                    send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
                }

            }
            else if (request.find("GET /atrisk") != std::string::npos) {
                std::string htmlContent = LoadFile("atrisk.html");
                if (!htmlContent.empty()) {
                    std::string httpResponse =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: " + std::to_string(htmlContent.length()) + "\r\n"
                        "Connection: close\r\n"
                        "\r\n" + htmlContent;
                    send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
                }

            }
            else {
                    std::string errorResponse =
                        "HTTP/1.1 404 Not Found\r\n"
                        "Content-Type: text/plain\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "404 Not Found\n";
                    send(clientSocket, errorResponse.c_str(), errorResponse.length(), 0);

                

            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in HandleClient: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown exception in HandleClient." << std::endl;
    }
    closesocket(clientSocket);
}


int StartHttpServer(bool& isFinished) {
    WSADATA wsaData;
    SOCKET listeningSocket, clientSocket;
    sockaddr_in serverAddr, clientAddr;
    int addrLen = sizeof(clientAddr);
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 0;
    }


    // Load the certificate
   // HCERTSTORE hStore = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, NULL, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"My");
   // PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR, L"localhost", NULL);
   // if (!pCertContext) {
   //     std::cerr << "Could not find certificate\n";
   //     WSACleanup();
   //     return -1;
   // }


    // Create a socket
    listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return 0;
    }
    // Set up the sockaddr_in structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);
    // Bind the socket
    if (bind(listeningSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(listeningSocket);
        WSACleanup();
        return 0;
    }

    listen(listeningSocket, SOMAXCONN);
    std::cout << "Server is listening on port " << PORT << "..." << std::endl;
    while (!isFinished) {
        SOCKET clientSocket = accept(listeningSocket, (sockaddr*)&clientAddr, &addrLen);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed\n";
           // CertFreeCertificateContext(pCertContext);
            //CertCloseStore(hStore, 0);
            WSACleanup();
            return -1;
        }

        std::thread clientThread(HandleClient, clientSocket);
        clientThread.detach(); 
    }



    //CertFreeCertificateContext(pCertContext);
    //CertCloseStore(hStore, 0);
    closesocket(listeningSocket);
    WSACleanup();
    return 1;
}

int main() {
    bool isFinished = false;
    int i = StartHttpServer(isFinished);
    return 0;
}
