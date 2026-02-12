#ifndef ArduinoFirebaseServer_H
#define ArduinoFirebaseServer_H

#include "IServer.h"
#include "IHttpRequest.h"
#include <Arduino.h>

/**
 * Firebase-style server implementation of IServer interface.
 * Stub implementation that returns dummy strings/values for now.
 */
/* @ServerImpl("arduinofirebaseserver") */
class ArduinoFirebaseServer : public IServer {
    Private UInt port_;
    Private Bool running_;
    Private StdString ipAddress_;
    Private StdString lastClientIp_;
    Private UInt lastClientPort_;
    Private ULong receivedMessageCount_;
    Private ULong sentMessageCount_;
    Private UInt maxMessageSize_;
    Private UInt receiveTimeout_;

    Public ArduinoFirebaseServer()
        : port_(DEFAULT_SERVER_PORT), running_(false),
          ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
          receivedMessageCount_(0), sentMessageCount_(0),
          maxMessageSize_(8192), receiveTimeout_(5000) {
    }

    Public ArduinoFirebaseServer(CUInt port)
        : port_(port), running_(false),
          ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
          receivedMessageCount_(0), sentMessageCount_(0),
          maxMessageSize_(8192), receiveTimeout_(5000) {
    }

    Public Virtual ~ArduinoFirebaseServer() override {
        Stop();
    }

    Public Virtual Bool Start(CUInt port = DEFAULT_SERVER_PORT) override {
        if (running_) {
            return false;
        }
        port_ = port;
        running_ = true;
        ipAddress_ = "0.0.0.0";
        return true;
    }

    Public Virtual Void Stop() override {
        running_ = false;
    }

    Public Virtual Bool IsRunning() const override {
        return running_;
    }

    Public Virtual UInt GetPort() const override {
        return port_;
    }

    Public Virtual StdString GetIpAddress() const override {
        return ipAddress_.empty() ? StdString("0.0.0.0") : ipAddress_;
    }

    Public Virtual Bool SetIpAddress(CStdString& ip) override {
        if (running_) {
            return false;
        }
        ipAddress_ = ip;
        return true;
    }

    Public Virtual IHttpRequestPtr ReceiveMessage() override {
        if (running_) {
            Serial.println("[ArduinoFirebaseServer] Running - no message (dummy)");
        }
        return nullptr;
    }

    Public Virtual Bool SendMessage(CStdString& requestId, CStdString& message) override {
        (void)requestId;
        (void)message;
        if (!running_) {
            return false;
        }
        sentMessageCount_++;
        return true;
    }

    Public Virtual StdString GetLastClientIp() const override {
        return lastClientIp_.empty() ? StdString("") : lastClientIp_;
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
        maxMessageSize_ = (size > 8192) ? 8192u : static_cast<UInt>(size);
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
        return ServerType::Unknown;
    }
};

#endif // ArduinoFirebaseServer_H
