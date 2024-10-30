#include <Windows.h>
#include <schannel.h>
#include <iostream>
#include <security.h>
#include <sspi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "schannel.lib") // for Schannel functions

int main() {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return -1;
    }

    // Set up Schannel Security Credentials
    CredHandle hCreds;
    TimeStamp tsExpiry;
    SCHANNEL_CRED schannelCred = {};
    schannelCred.dwVersion = SCHANNEL_CRED_VERSION;
    schannelCred.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER;

    // Load the certificate
    HCERTSTORE hStore = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, NULL, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"My");
    PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR, L"localhost", NULL);
    if (!pCertContext) {
        std::cerr << "Could not find certificate\n";
        CertCloseStore(hStore, 0);
        WSACleanup();
        return -1;
    }

    schannelCred.paCred = &pCertContext;
    schannelCred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS;


    PSECURITY_STRING unisp_name;

    


    SECURITY_STATUS secStatus = AcquireCredentialsHandle(NULL, unisp_name, 2, NULL, &schannelCred, NULL, NULL, &hCreds, &tsExpiry);
    if (secStatus != SEC_E_OK) {
        std::cerr << "Failed to acquire credentials\n";
        CertFreeCertificateContext(pCertContext);
        CertCloseStore(hStore, 0);
        WSACleanup();
        return -1;
    }

    // Create Socket and Bind
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(443);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        FreeCredentialsHandle(&hCreds);
        CertFreeCertificateContext(pCertContext);
        CertCloseStore(hStore, 0);
        WSACleanup();
        return -1;
    }

    listen(listenSocket, SOMAXCONN);

    // Accept a client connection
    SOCKET clientSocket = accept(listenSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed\n";
        FreeCredentialsHandle(&hCreds);
        CertFreeCertificateContext(pCertContext);
        CertCloseStore(hStore, 0);
        WSACleanup();
        return -1;
    }

    // Perform the TLS Handshake
    CtxtHandle hContext;
    SecBufferDesc outBufferDesc;
    SecBuffer outSecBuffer;
    SecBufferDesc inBufferDesc;
    SecBuffer inSecBuffer;
    DWORD outFlags;

    outSecBuffer.cbBuffer = 4096;
    outSecBuffer.pvBuffer = LocalAlloc(LMEM_FIXED, outSecBuffer.cbBuffer);
    outSecBuffer.BufferType = SECBUFFER_TOKEN;

    outBufferDesc.ulVersion = SECBUFFER_VERSION;
    outBufferDesc.cBuffers = 1;
    outBufferDesc.pBuffers = &outSecBuffer;

    // First step of the handshake
    SECURITY_STATUS secStatus = AcceptSecurityContext(
        &hCreds,               // Credential handle

        NULL,                  // No existing context

        NULL,                  // No input buffer on the first call

        ASC_REQ_CONFIDENTIALITY, // Context requirement flags

        SECURITY_NATIVE_DREP,  // Target data representation

        &hContext,             // New context handle

        &outBufferDesc,        // Output buffer

        &outFlags,             // Context attributes

        &tsExpiry);            // Expiration timestamp


    if (secStatus != SEC_I_CONTINUE_NEEDED) {
        std::cerr << "Handshake failed with error: " << secStatus << "\n";
        return -1;
    }
    if (secStatus != SEC_I_CONTINUE_NEEDED) {
        std::cerr << "Handshake failed\n";
        LocalFree(outSecBuffer.pvBuffer);
        closesocket(clientSocket);
        FreeCredentialsHandle(&hCreds);
        CertFreeCertificateContext(pCertContext);
        CertCloseStore(hStore, 0);
        WSACleanup();
        return -1;
    }

    // Send the handshake token to the client
    send(clientSocket, (char*)outSecBuffer.pvBuffer, outSecBuffer.cbBuffer, 0);

    // Read client's response
    char clientResponse[4096];
    int clientResponseLength = recv(clientSocket, clientResponse, sizeof(clientResponse), 0);
    if (clientResponseLength <= 0) {
        std::cerr << "Failed to receive response from client\n";
        LocalFree(outSecBuffer.pvBuffer);
        closesocket(clientSocket);
        FreeCredentialsHandle(&hCreds);
        CertFreeCertificateContext(pCertContext);
        CertCloseStore(hStore, 0);
        WSACleanup();
        return -1;
    }

    // Prepare input buffer for the next AcceptSecurityContext call
    inSecBuffer.pvBuffer = clientResponse;
    inSecBuffer.cbBuffer = clientResponseLength;
    inSecBuffer.BufferType = SECBUFFER_TOKEN;

    inBufferDesc.ulVersion = SECBUFFER_VERSION;
    inBufferDesc.cBuffers = 1;
    inBufferDesc.pBuffers = &inSecBuffer;

    // Second step of the handshake
    secStatus = AcceptSecurityContext(&hCreds, &hContext, &inBufferDesc, 0, NULL, &hContext, &outBufferDesc, &outFlags, NULL);
    if (secStatus != SEC_E_OK) {
        std::cerr << "Handshake failed\n";
        LocalFree(outSecBuffer.pvBuffer);
        closesocket(clientSocket);
        FreeCredentialsHandle(&hCreds);
        CertFreeCertificateContext(pCertContext);
        CertCloseStore(hStore, 0);
        WSACleanup();
        return -1;
    }
    // Send the next handshake token if needed
    if (outSecBuffer.cbBuffer > 0) {
        send(clientSocket, (char*)outSecBuffer.pvBuffer, outSecBuffer.cbBuffer, 0);
    }