#ifndef HttpTcpArduinoServer_H
#define HttpTcpArduinoServer_H

#include "IServer.h"
#include "IHttpRequest.h"

#if defined(ESP8266) || defined(ESP32)
    #include <WiFiServer.h>
    #include <WiFiClient.h>
    #define ARDUINO_TCP_SERVER WiFiServer
    #define ARDUINO_TCP_CLIENT WiFiClient
#elif defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_NRF52)
    #include <Ethernet.h>
    #include <EthernetServer.h>
    #include <EthernetClient.h>
    #define ARDUINO_TCP_SERVER EthernetServer
    #define ARDUINO_TCP_CLIENT EthernetClient
#else
    #include <WiFiServer.h>
    #include <WiFiClient.h>
    #define ARDUINO_TCP_SERVER WiFiServer
    #define ARDUINO_TCP_CLIENT WiFiClient
#endif

#include <Arduino.h>
#include <vector>

/**
 * HTTP TCP Server implementation of IServer interface
 * Header-only implementation using Arduino PlatformIO TCP server
 */
ServerImpl("http tcp arduino server")
class HttpTcpArduinoServer : public IServer {
    Private UInt port_;
    Private ARDUINO_TCP_SERVER* server_;
    Private Bool running_;
    Private StdString ipAddress_;
    Private StdString lastClientIp_;
    Private UInt lastClientPort_;
    Private ULong receivedMessageCount_;
    Private ULong sentMessageCount_;
    Private UInt maxMessageSize_;
    Private UInt receiveTimeout_;
    Private ARDUINO_TCP_CLIENT* currentClient_;

    Private Void CloseClient() {
        if (currentClient_ != nullptr) {
            currentClient_->stop();
            delete currentClient_;
            currentClient_ = nullptr;
        }
    }

    Private Void SendHttpResponse(ARDUINO_TCP_CLIENT& client, CStdString& request) {
        // Parse the request to extract method and path
        StdString method, path, version;
        Size firstSpace = request.find(' ');
        if (firstSpace != StdString::npos) {
            method = request.substr(0, firstSpace);
            Size secondSpace = request.find(' ', firstSpace + 1);
            if (secondSpace != StdString::npos) {
                path = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                Size versionStart = request.find("\r\n", secondSpace);
                if (versionStart != StdString::npos) {
                    version = request.substr(secondSpace + 1, versionStart - secondSpace - 1);
                }
            }
        }
        
        // Build HTTP response
        StdString response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";
        response += "Request received successfully!\n";
        response += "Method: " + method + "\n";
        response += "Path: " + path + "\n";
        response += "Full Request:\n" + request;
        
        client.print(response.c_str());
        sentMessageCount_++;
    }

    Private StdString ReadHttpRequest(ARDUINO_TCP_CLIENT& client) {
        StdString request;
        Size bufferSize = maxMessageSize_ > 0 ? maxMessageSize_ : 8192; // Default 8KB for Arduino
        if (bufferSize > 8192) bufferSize = 8192; // Limit to 8KB for Arduino memory constraints
        
        std::vector<Char> buffer(bufferSize, 0);
        Size totalReceived = 0;
        ULong startTime = millis();
        ULong timeoutMs = receiveTimeout_ > 0 ? receiveTimeout_ : 5000; // Default 5 second timeout
        
        // Read until we get the headers (double CRLF)
        while (client.connected() || client.available()) {
            // Check timeout
            if (receiveTimeout_ > 0 && (millis() - startTime) > timeoutMs) {
                break;
            }
            
            if (client.available()) {
                Size remainingSpace = bufferSize - totalReceived - 1;
                if (remainingSpace == 0) {
                    break; // Buffer full
                }
                
                Int bytesAvailable = client.available();
                if (bytesAvailable > 0) {
                    Int bytesToRead = (bytesAvailable < static_cast<Int>(remainingSpace)) ? 
                                     bytesAvailable : static_cast<Int>(remainingSpace);
                    
                    Int bytesRead = client.readBytes(buffer.data() + totalReceived, bytesToRead);
                    if (bytesRead > 0) {
                        totalReceived += bytesRead;
                        buffer[totalReceived] = '\0';
                        
                        // Check if we've received the headers (look for double CRLF)
                        StdString currentData(buffer.data(), totalReceived);
                        Size headerEnd = currentData.find("\r\n\r\n");
                        if (headerEnd == StdString::npos) {
                            headerEnd = currentData.find("\n\n");
                        }
                        
                        if (headerEnd != StdString::npos) {
                            // Headers received, check for Content-Length
                            Size contentLengthPos = currentData.find("Content-Length:");
                            if (contentLengthPos != StdString::npos) {
                                // Extract Content-Length value
                                Size valueStart = currentData.find(':', contentLengthPos) + 1;
                                Size valueEnd = currentData.find("\r\n", valueStart);
                                if (valueEnd == StdString::npos) {
                                    valueEnd = currentData.find('\n', valueStart);
                                }
                                
                                StdString contentLengthStr = currentData.substr(valueStart, valueEnd - valueStart);
                                // Trim whitespace
                                while (!contentLengthStr.empty() && 
                                       (contentLengthStr[0] == ' ' || contentLengthStr[0] == '\t')) {
                                    contentLengthStr.erase(0, 1);
                                }
                                while (!contentLengthStr.empty() && 
                                       (contentLengthStr[contentLengthStr.length() - 1] == ' ' || 
                                        contentLengthStr[contentLengthStr.length() - 1] == '\t')) {
                                    contentLengthStr.erase(contentLengthStr.length() - 1);
                                }
                                
                                ULong contentLength = 0;
                                for (Size i = 0; i < contentLengthStr.length(); i++) {
                                    if (contentLengthStr[i] >= '0' && contentLengthStr[i] <= '9') {
                                        contentLength = contentLength * 10 + (contentLengthStr[i] - '0');
                                    }
                                }
                                
                                Size bodyStart = headerEnd + 4; // Skip double CRLF
                                Size bodyReceived = totalReceived - bodyStart;
                                
                                // Read remaining body if needed
                                while (bodyReceived < contentLength && 
                                       totalReceived < bufferSize - 1 &&
                                       (client.connected() || client.available())) {
                                    if (receiveTimeout_ > 0 && (millis() - startTime) > timeoutMs) {
                                        break;
                                    }
                                    
                                    if (client.available()) {
                                        Size remaining = bufferSize - totalReceived - 1;
                                        if (remaining > 0) {
                                            Int moreBytes = client.readBytes(
                                                buffer.data() + totalReceived, 
                                                (remaining < 1024) ? remaining : 1024
                                            );
                                            if (moreBytes > 0) {
                                                totalReceived += moreBytes;
                                                bodyReceived = totalReceived - bodyStart;
                                            } else {
                                                break;
                                            }
                                        } else {
                                            break;
                                        }
                                    } else {
                                        delay(10); // Small delay to wait for more data
                                    }
                                }
                            }
                            break;
                        }
                    } else {
                        break;
                    }
                } else {
                    delay(10); // Small delay to wait for data
                }
            } else {
                delay(10); // Small delay when no data available
            }
        }
        
        return StdString(buffer.data(), totalReceived);
    }

    Public HttpTcpArduinoServer() 
        : port_(DEFAULT_SERVER_PORT), server_(nullptr), running_(false),
          ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
          receivedMessageCount_(0), sentMessageCount_(0),
          maxMessageSize_(8192), receiveTimeout_(5000), currentClient_(nullptr) {
    }

    Public HttpTcpArduinoServer(CUInt port) 
        : port_(port), server_(nullptr), running_(false),
          ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
          receivedMessageCount_(0), sentMessageCount_(0),
          maxMessageSize_(8192), receiveTimeout_(5000), currentClient_(nullptr) {
    }

    Public Virtual ~HttpTcpArduinoServer() {
        Stop();
    }

    Public Virtual Bool Start(CUInt port = DEFAULT_SERVER_PORT) override {
        if (running_) {
            return false;
        }

        port_ = port;
        
        // Create and start the server
        if (server_ != nullptr) {
            delete server_;
        }
        
        server_ = new ARDUINO_TCP_SERVER(static_cast<uint16_t>(port_));
        if (server_ == nullptr) {
            return false;
        }
        
        server_->begin();
        
        running_ = true;
        return true;
    }

    Public Virtual Void Stop() override {
        if (running_) {
            CloseClient();
            if (server_ != nullptr) {
                // Arduino servers don't have explicit stop, but we can delete it
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
        
        // Close previous client if still open
        CloseClient();
        
        // Check for new client connection
        ARDUINO_TCP_CLIENT client = server_->available();
        if (!client || !client.connected()) {
            return nullptr;
        }
        
        // Store client information
        #if defined(ESP8266) || defined(ESP32)
            IPAddress clientIP = client.remoteIP();
            lastClientIp_ = StdString(clientIP.toString().c_str());
            lastClientPort_ = client.remotePort();
        #else
            // For Ethernet, we may not have direct access to client IP
            // Use a placeholder or try to get it if available
            lastClientIp_ = "0.0.0.0";
            lastClientPort_ = 0;
        #endif
        
        // Read HTTP request
        StdString requestString = ReadHttpRequest(client);
        
        if (requestString.empty()) {
            client.stop();
            return nullptr;
        }
        
        // Send HTTP response
        SendHttpResponse(client, requestString);
        
        // Close client connection
        client.stop();
        
        receivedMessageCount_++;
        
        // Parse and return IHttpRequest
        return IHttpRequest::GetRequest(requestString);
    }

    Public Virtual Bool SendMessage(CStdString& message, 
                            CStdString& clientIp = "", 
                            CUInt clientPort = 0) override {
        // For TCP, we typically send responses during ReceiveMessage
        // This method can be used for sending to a specific client if needed
        if (!running_ || server_ == nullptr) {
            return false;
        }
        
        // For Arduino TCP, sending to a specific client requires establishing a connection
        // This is a simplified implementation that attempts to connect to the client
        if (!clientIp.empty() && clientPort > 0) {
            #if defined(ESP8266) || defined(ESP32)
                WiFiClient client;
                IPAddress ip;
                if (ip.fromString(clientIp.c_str())) {
                    if (client.connect(ip, static_cast<uint16_t>(clientPort))) {
                        client.print(message.c_str());
                        client.stop();
                        sentMessageCount_++;
                        return true;
                    }
                }
            #else
                // For Ethernet, we'd need to use EthernetClient similarly
                // This is platform-specific and may not be fully supported
                return false;
            #endif
        }
        
        return false;
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
        // Limit max message size for Arduino memory constraints
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
};

#endif // HttpTcpArduinoServer_H
