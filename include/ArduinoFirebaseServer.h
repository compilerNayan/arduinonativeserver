#ifndef ArduinoFirebaseServer_H
#define ArduinoFirebaseServer_H

#include "IServer.h"
#include "IHttpRequest.h"
#include "firebase/IFirebaseRequestManager.h"
#include <Arduino.h>

/**
 * Firebase-style server implementation of IServer interface.
 * Header-only. Uses IFirebaseRequestManager: RetrieveRequests() -> first "key:value" -> value as raw HTTP request.
 * Project using this must have build_flags: -DENABLE_DATABASE -DENABLE_LEGACY_TOKEN (and optionally -DFIREBASE_SSE_TIMEOUT_MS=40000).
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

    /* @Autowired */
    Private IFirebaseRequestManagerPtr firebaseRequestManager;

    Private Static StdString GenerateGuid() {
        StdString guid;
        const char hexChars[] = "0123456789abcdef";
        for (Size i = 0; i < 8; i++) guid += hexChars[random(0, 16)];
        guid += "-";
        for (Size i = 0; i < 4; i++) guid += hexChars[random(0, 16)];
        guid += "-4";
        for (Size i = 0; i < 3; i++) guid += hexChars[random(0, 16)];
        guid += "-";
        guid += hexChars[random(8, 12)];
        for (Size i = 0; i < 3; i++) guid += hexChars[random(0, 16)];
        guid += "-";
        for (Size i = 0; i < 12; i++) guid += hexChars[random(0, 16)];
        return guid;
    }

    Public Virtual Bool IsRunning() const override {
        return running_;
    }

    Public Virtual UInt GetPort() const override {
        return port_;
    }

    Public Virtual Bool Start(CUInt port = DEFAULT_SERVER_PORT) override {
        port_ = port;
        running_ = true;
        return true;
    }

    Public Virtual Void Stop() override {
        running_ = false;
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
        if (!firebaseRequestManager) return nullptr;

        StdList<StdString> pairs = firebaseRequestManager->RetrieveRequests();
        if (pairs.empty()) return nullptr;

        const StdString& firstPair = pairs.front();
        Size colonPos = firstPair.find(':');
        StdString value = (colonPos != StdString::npos && colonPos + 1 < firstPair.size())
            ? firstPair.substr(colonPos + 1)
            : firstPair;
        if (value.empty()) return nullptr;

        StdString requestId = "ignore";
        receivedMessageCount_++;

        return IHttpRequest::GetRequest(requestId, value);
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

    Public Virtual StdString GetId() const override {
        return StdString("arduinofirebaseserver");
    }
};

#endif // ArduinoFirebaseServer_H
