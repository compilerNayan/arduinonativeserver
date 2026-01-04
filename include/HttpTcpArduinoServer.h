#ifndef HttpTcpArduinoServer_H
#define HttpTcpArduinoServer_H

#include "IServer.h"
#include "IHttpRequest.h"
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <Arduino.h>
#include <sstream>
#ifdef ARDUINO
#include <stdlib.h>
#else
#include <random>
#endif

/**
 * Structure to hold sender details for response routing (Arduino version)
 */
struct ArduinoSenderDetails {
    StdString clientIp;
    UInt clientPort;
    
    ArduinoSenderDetails() : clientPort(0) {}
    ArduinoSenderDetails(CStdString& ip, CUInt port) 
        : clientIp(ip), clientPort(port) {}
};

/**
 * HTTP TCP Server implementation of IServer interface
 * Simple implementation using WiFiServer (tested and working)
 */
/// @ServerImpl("arduinotcpserver")
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
    Private std::unordered_map<StdString, ArduinoSenderDetails> senderDetailsMap_;

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

    Private StdString GenerateGuid() {
        // GUID generation for Arduino and desktop
        std::ostringstream oss;
        oss << std::hex;
        
#ifdef ARDUINO
        // Arduino version using random()
        randomSeed(analogRead(0) + millis());
        for (Size i = 0; i < 8; i++) {
            oss << (random() % 16);
        }
        oss << "-";
        for (Size i = 0; i < 4; i++) {
            oss << (random() % 16);
        }
        oss << "-4"; // Version 4
        for (Size i = 0; i < 3; i++) {
            oss << (random() % 16);
        }
        oss << "-";
        oss << (8 + (random() % 4)); // Variant (8-11)
        for (Size i = 0; i < 3; i++) {
            oss << (random() % 16);
        }
        oss << "-";
        for (Size i = 0; i < 12; i++) {
            oss << (random() % 16);
        }
#else
        // Desktop version using std::random
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);
        
        for (Size i = 0; i < 8; i++) {
            oss << dis(gen);
        }
        oss << "-";
        for (Size i = 0; i < 4; i++) {
            oss << dis(gen);
        }
        oss << "-4"; // Version 4
        for (Size i = 0; i < 3; i++) {
            oss << dis(gen);
        }
        oss << "-";
        oss << dis2(gen); // Variant
        for (Size i = 0; i < 3; i++) {
            oss << dis(gen);
        }
        oss << "-";
        for (Size i = 0; i < 12; i++) {
            oss << dis(gen);
        }
#endif
        return oss.str();
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
          maxMessageSize_(8192), receiveTimeout_(5000) {
    }

    Public HttpTcpArduinoServer(CUInt port) 
        : port_(port), server_(nullptr), running_(false),
          ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
          receivedMessageCount_(0), sentMessageCount_(0),
          maxMessageSize_(8192), receiveTimeout_(5000) {
    }

    Public Virtual ~HttpTcpArduinoServer() {
        Stop();
    }

    Public Virtual Bool Start(CUInt port = DEFAULT_SERVER_PORT) override {
        if (running_) {
            return false;
        }

        port_ = port;
        
        if (server_ != nullptr) {
            delete server_;
        }
        
        server_ = new WiFiServer(static_cast<uint16_t>(port_));
        if (server_ == nullptr) {
            return false;
        }
        
        server_->begin();
        running_ = true;
        
        // Get actual IP address if WiFi is connected
        if (WiFi.status() == WL_CONNECTED) {
            ipAddress_ = StdString(WiFi.localIP().toString().c_str());
        }
        
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
        
        // Store sender details in map for response routing
        ArduinoSenderDetails senderDetails(lastClientIp_, lastClientPort_);
        senderDetailsMap_[requestId] = senderDetails;
        
        // Close client connection (CRITICAL: must close to free socket)
        client.stop();
        
        receivedMessageCount_++;
        
        // Parse and return IHttpRequest with request ID
        return IHttpRequest::GetRequest(fullRequest, requestId);
    }

    Public Virtual Bool SendMessage(CStdString& requestId, CStdString& message) override {
        if (!running_ || server_ == nullptr) {
            return false;
        }
        
        // Look up sender details from the map using request ID
        auto it = senderDetailsMap_.find(StdString(requestId));
        if (it == senderDetailsMap_.end()) {
            return false; // Request ID not found
        }
        
        ArduinoSenderDetails& senderDetails = it->second;
        
        // For Arduino, we need to create a new connection to send the message
        // since the original connection is closed after ReceiveMessage
        WiFiClient client;
        if (!client.connect(senderDetails.clientIp.c_str(), senderDetails.clientPort)) {
            return false; // Failed to connect
        }
        
        // Send the message
        client.print(message.c_str());
        client.stop();
        
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
    
    /**
     * Get sender details for a given request ID
     * @param requestId The request ID (GUID) to look up
     * @return ArduinoSenderDetails* if found, or nullptr if not found
     */
    Public ArduinoSenderDetails* GetSenderDetails(CStdString& requestId) {
        auto it = senderDetailsMap_.find(StdString(requestId));
        if (it != senderDetailsMap_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    /**
     * Remove sender details for a given request ID (cleanup after response sent)
     * @param requestId The request ID (GUID) to remove
     * @return true if found and removed, false otherwise
     */
    Public Bool RemoveSenderDetails(CStdString& requestId) {
        auto it = senderDetailsMap_.find(StdString(requestId));
        if (it != senderDetailsMap_.end()) {
            senderDetailsMap_.erase(it);
            return true;
        }
        return false;
    }
};

#endif // HttpTcpArduinoServer_H
