#ifndef HttpTcpArduinoServer_H
#define HttpTcpArduinoServer_H

#include "IServer.h"
#include "IHttpRequest.h"
#include "INetworkStatusProvider.h"
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <Arduino.h>
#include <map>

/**
 * Sender details structure to store client connection information
 */
struct SenderDetails {
    WiFiClient* client;
    StdString ipAddress;
    UInt port;
    
    SenderDetails() : client(nullptr), ipAddress(""), port(0) {}
    SenderDetails(WiFiClient* cli, CStdString& ip, CUInt p) : client(cli), ipAddress(ip), port(p) {}
};

/**
 * HTTP TCP Server implementation of IServer interface
 * Simple implementation using WiFiServer (tested and working)
 */
/* @ServerImpl("arduinotcpserver") */
class HttpTcpArduinoServer : public IServer {
    Private UInt port_;
    Private WiFiServer* server_;
    Private Bool running_;
    Private StdString ipAddress_;
    Private StdString lastClientIp_;
    Private UInt lastClientPort_;
    Private ULong receivedMessageCount_;
    Private ULong sentMessageCount_;
    Private UInt maxMessageSize_;
    Private UInt receiveTimeout_;
    Private StdMap<StdString, SenderDetails> requestSenderMap_;

    /* @Autowired */
    Private INetworkStatusProviderPtr networkStatusProvider_;
    Private Int storedWifiConnectionId_;

    /**
     * Ensures WiFi is connected and server is bound to the current WiFi connection id.
     * If no provider is set, returns true (proceed). If WiFi is disconnected, stops server and returns false.
     * If WiFi is connected but connection id changed, restarts server and updates stored id, then returns true.
     * Call before ReceiveMessage or SendMessage; if false, do not proceed.
     */
    Private Bool EnsureWifiAndServerMatch() {
        if (networkStatusProvider_ == nullptr) {
            return true;
        }
        if (!networkStatusProvider_->IsWiFiConnected()) {
            Stop();
            return false;
        }
        Int currentId = networkStatusProvider_->GetWifiConnectionId();
        if (currentId != storedWifiConnectionId_) {
            Stop();
            Start(port_);
            storedWifiConnectionId_ = currentId;
        }
        return true;
    }

    Private StdString GenerateGuid() {
        // Simple GUID generation for Arduino using random()
        // Generate UUID v4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
        StdString guid = "";
        Char hexChars[] = "0123456789abcdef";
        
        // First 8 hex digits
        for (Size i = 0; i < 8; i++) {
            guid += hexChars[random(0, 16)];
        }
        guid += "-";
        
        // Next 4 hex digits
        for (Size i = 0; i < 4; i++) {
            guid += hexChars[random(0, 16)];
        }
        guid += "-4";
        
        // Next 3 hex digits (version 4)
        for (Size i = 0; i < 3; i++) {
            guid += hexChars[random(0, 16)];
        }
        guid += "-";
        
        // Variant (8, 9, a, or b)
        guid += hexChars[random(8, 12)];
        
        // Next 3 hex digits
        for (Size i = 0; i < 3; i++) {
            guid += hexChars[random(0, 16)];
        }
        guid += "-";
        
        // Last 12 hex digits
        for (Size i = 0; i < 12; i++) {
            guid += hexChars[random(0, 16)];
        }
        
        return guid;
    }

    Private StdString ReadHttpRequestHeaders(WiFiClient& client) {
        StdString requestHeaders = "";
        ULong timeout = millis();
        ULong timeoutMs = receiveTimeout_ > 0 ? receiveTimeout_ : 5000;
        
        // Read HTTP headers until we get the blank line (\r\n\r\n)
        while (client.connected() && (millis() - timeout < timeoutMs)) {
            if (client.available()) {
                Char c = client.read();
                requestHeaders += c;
                timeout = millis();
                
                // Headers end with \r\n\r\n or \n\n
                if (requestHeaders.length() >= 4) {
                    Size len = requestHeaders.length();
                    if ((len >= 4 && requestHeaders.substr(len - 4) == "\r\n\r\n") ||
                        (len >= 2 && requestHeaders.substr(len - 2) == "\n\n")) {
                        break;
                    }
                }
                
                // Safety: limit header size
                if (requestHeaders.length() > maxMessageSize_) {
                    break;
                }
            }
            delay(1);
        }
        
        return requestHeaders;
    }

    Private Int ParseContentLength(CStdString& headers) {
        Size contentLengthIndex = headers.find("Content-Length:");
        if (contentLengthIndex == StdString::npos) {
            return 0;
        }
        
        Size colonIndex = headers.find(":", contentLengthIndex);
        Size newlineIndex = headers.find("\r\n", colonIndex);
        if (newlineIndex == StdString::npos) {
            newlineIndex = headers.find("\n", colonIndex);
        }
        
        if (newlineIndex > colonIndex) {
            StdString lengthStr = headers.substr(colonIndex + 1, newlineIndex - colonIndex - 1);
            // Trim whitespace
            while (!lengthStr.empty() && (lengthStr[0] == ' ' || lengthStr[0] == '\t')) {
                lengthStr.erase(0, 1);
            }
            while (!lengthStr.empty() && 
                   (lengthStr[lengthStr.length() - 1] == ' ' || 
                    lengthStr[lengthStr.length() - 1] == '\t')) {
                lengthStr.erase(lengthStr.length() - 1);
            }
            
            // Convert to integer
            Int contentLength = 0;
            for (Size i = 0; i < lengthStr.length(); i++) {
                if (lengthStr[i] >= '0' && lengthStr[i] <= '9') {
                    contentLength = contentLength * 10 + (lengthStr[i] - '0');
                }
            }
            return contentLength;
        }
        
        return 0;
    }

    Private StdString ReadHttpRequestBody(WiFiClient& client, Int contentLength) {
        StdString body = "";
        if (contentLength <= 0) {
            return body;
        }
        
        ULong timeout = millis();
        ULong timeoutMs = receiveTimeout_ > 0 ? receiveTimeout_ : 5000;
        
        // Read body if Content-Length is present
        while (client.connected() && 
               static_cast<Int>(body.length()) < contentLength && 
               (millis() - timeout < timeoutMs)) {
            if (client.available()) {
                Char c = client.read();
                body += c;
                timeout = millis();
            }
            delay(1);
        }
        
        return body;
    }

    Public HttpTcpArduinoServer() 
        : port_(DEFAULT_SERVER_PORT), server_(nullptr), running_(false),
          ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
          receivedMessageCount_(0), sentMessageCount_(0),
          maxMessageSize_(8192), receiveTimeout_(5000),
          storedWifiConnectionId_(0) {
    }

    Public HttpTcpArduinoServer(CUInt port) 
        : port_(port), server_(nullptr), running_(false),
          ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
          receivedMessageCount_(0), sentMessageCount_(0),
          maxMessageSize_(8192), receiveTimeout_(5000),
          storedWifiConnectionId_(0) {
    }

    Public Virtual ~HttpTcpArduinoServer() {
        Stop();
    }

    Public Virtual Bool Start(CUInt port = DEFAULT_SERVER_PORT) override {
        Serial.print("[HttpTcpArduinoServer] Start() called with port: ");
        Serial.println(port);

        if (networkStatusProvider_ != nullptr && !networkStatusProvider_->IsWiFiConnected()) {
            Serial.println("[HttpTcpArduinoServer] ERROR: WiFi is not connected. Start aborted.");
            return false;
        }
        if (networkStatusProvider_ != nullptr) {
            storedWifiConnectionId_ = networkStatusProvider_->GetWifiConnectionId();
        }
        Stop();

        if (running_) {
            Serial.println("[HttpTcpArduinoServer] ERROR: Server is already running!");
            return false;
        }

        port_ = port;
        Serial.print("[HttpTcpArduinoServer] Port set to: ");
        Serial.println(port_);
        
        Serial.println("[HttpTcpArduinoServer] Step 1: Cleaning up existing server instance...");
        if (server_ != nullptr) {
            delete server_;
            server_ = nullptr;
            Serial.println("[HttpTcpArduinoServer] Old server instance deleted");
        }
        
        Serial.println("[HttpTcpArduinoServer] Step 2: Creating new WiFiServer instance...");
        server_ = new WiFiServer(static_cast<uint16_t>(port_));
        if (server_ == nullptr) {
            Serial.println("[HttpTcpArduinoServer] ERROR: Failed to allocate memory for WiFiServer!");
            return false;
        }
        Serial.println("[HttpTcpArduinoServer] WiFiServer instance created successfully");
        
        Serial.println("[HttpTcpArduinoServer] Step 3: Checking WiFi status...");
        wl_status_t wifiStatus = WiFi.status();
        Serial.print("[HttpTcpArduinoServer] WiFi status: ");
        Serial.println(wifiStatus);
        if (wifiStatus != WL_CONNECTED) {
            Serial.print("[HttpTcpArduinoServer] WARNING: WiFi is not connected (status: ");
            Serial.print(wifiStatus);
            Serial.println("). Server may not work properly.");
        } else {
            Serial.print("[HttpTcpArduinoServer] WiFi is connected. Local IP: ");
            Serial.println(WiFi.localIP());
        }
        
        Serial.println("[HttpTcpArduinoServer] Step 4: Starting server with server_->begin()...");
        server_->begin();
        running_ = true;
        Serial.print("[HttpTcpArduinoServer] Server started. Running: ");
        Serial.println(running_ ? "true" : "false");
        
        // Get actual IP address if WiFi is connected
        if (WiFi.status() == WL_CONNECTED) {
            ipAddress_ = StdString(WiFi.localIP().toString().c_str());
            Serial.print("[HttpTcpArduinoServer] Server IP address: ");
            Serial.println(ipAddress_.c_str());
        } else {
            Serial.println("[HttpTcpArduinoServer] WiFi not connected, IP address not set");
        }

        Serial.println("[HttpTcpArduinoServer] Start() completed successfully");
        return true;
    }

    Public Virtual Void Stop() override {
        if (running_) {
            if (server_ != nullptr) {
                delete server_;
                server_ = nullptr;
            }
            running_ = false;
        }
    }

    Public Virtual Bool IsRunning() const override {
        return running_;
    }

    Public Virtual UInt GetPort() const override {
        return port_;
    }

    Public Virtual StdString GetIpAddress() const override {
        return ipAddress_;
    }

    Public Virtual Bool SetIpAddress(CStdString& ip) override {
        if (running_) {
            return false;
        }
        ipAddress_ = ip;
        return true;
    }

    Public Virtual IHttpRequestPtr ReceiveMessage() override {
        if (!EnsureWifiAndServerMatch()) {
            return nullptr;
        }
        if (!running_ || server_ == nullptr) {
            return nullptr;
        }
        
        // Check for new client connection
        WiFiClient client = server_->available();
        if (!client || !client.connected()) {
            return nullptr;
        }

        // Store client information
        IPAddress clientIP = client.remoteIP();
        lastClientIp_ = StdString(clientIP.toString().c_str());
        lastClientPort_ = client.remotePort();
        
        // Read HTTP headers
        StdString requestHeaders = ReadHttpRequestHeaders(client);
        if (requestHeaders.empty()) {
            client.stop();
            return nullptr;
        }
        
        // Parse Content-Length to determine if there's a body
        Int contentLength = ParseContentLength(requestHeaders);
        
        // Read body if Content-Length is present
        StdString body = ReadHttpRequestBody(client, contentLength);
        
        // Build full request string (headers + body)
        StdString fullRequest = requestHeaders + body;
        
        // Generate GUID for this request
        StdString requestId = GenerateGuid();
        
        // Allocate WiFiClient on heap to keep it alive
        // Use move semantics if available, otherwise copy
        WiFiClient* clientPtr = new WiFiClient(std::move(client));
        if (clientPtr == nullptr) {
            client.stop();
            return nullptr;
        }
        
        // Store sender details against the GUID
        SenderDetails senderDetails(clientPtr, lastClientIp_, lastClientPort_);
        requestSenderMap_[requestId] = senderDetails;
        
        receivedMessageCount_++;
        
        // Parse and return IHttpRequest with request ID
        // NOTE: Do NOT close client here - it's stored in the map for SendMessage()
        return IHttpRequest::GetRequest(requestId, fullRequest);
    }

    Public Virtual Bool SendMessage(CStdString& requestId, CStdString& message) override {
        if (!EnsureWifiAndServerMatch()) {
            return false;
        }
        if (!running_ || server_ == nullptr) {
            return false;
        }
        
        // Look up sender details from the map using requestId
        auto it = requestSenderMap_.find(StdString(requestId));
        if (it == requestSenderMap_.end()) {
            return false; // Request ID not found
        }
        
        SenderDetails& senderDetails = it->second;
        
        // Check if client is valid
        if (senderDetails.client == nullptr || !senderDetails.client->connected()) {
            // Clean up if client is invalid
            if (senderDetails.client != nullptr) {
                senderDetails.client->stop();
                delete senderDetails.client;
            }
            requestSenderMap_.erase(it);
            return false;
        }
        
        // Send the message using the stored client
        WiFiClient* client = senderDetails.client;
        size_t bytesSent = client->print(message.c_str());
        if (bytesSent == 0) {
            // Failed to send, clean up
            client->stop();
            delete client;
            requestSenderMap_.erase(it);
            return false;
        }
        
        // Close the client after sending
        client->stop();
        delete client;
        
        // Remove the entry from the map after sending
        requestSenderMap_.erase(it);
        
        sentMessageCount_++;
        return true;
    }

    Public Virtual StdString GetLastClientIp() const override {
        return lastClientIp_;
    }

    Public Virtual UInt GetLastClientPort() const override {
        return lastClientPort_;
    }

    Public Virtual ULong GetReceivedMessageCount() const override {
        return receivedMessageCount_;
    }

    Public Virtual ULong GetSentMessageCount() const override {
        return sentMessageCount_;
    }

    Public Virtual Void ResetStatistics() override {
        receivedMessageCount_ = 0;
        sentMessageCount_ = 0;
    }

    Public Virtual UInt GetMaxMessageSize() const override {
        return maxMessageSize_;
    }

    Public Virtual Bool SetMaxMessageSize(Size size) override {
        if (running_) {
            return false;
        }
        if (size > 8192) {
            maxMessageSize_ = 8192;
        } else {
            maxMessageSize_ = static_cast<UInt>(size);
        }
        return true;
    }

    Public Virtual UInt GetReceiveTimeout() const override {
        return receiveTimeout_;
    }

    Public Virtual Bool SetReceiveTimeout(CUInt timeoutMs) override {
        receiveTimeout_ = timeoutMs;
        return true;
    }

    Public Virtual ServerType GetServerType() const override {
        return ServerType::TCP;
    }

    Public Virtual StdString GetId() const override {
        return StdString("arduinotcpserver");
    }
};

#endif // HttpTcpArduinoServer_H
