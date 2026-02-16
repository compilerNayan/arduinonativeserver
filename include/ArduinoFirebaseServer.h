#ifndef ArduinoFirebaseServer_H
#define ArduinoFirebaseServer_H

#include "IServer.h"
#include "IHttpRequest.h"
#include "firebase/IFirebaseRequestManager.h"
#include "INetworkStatusProvider.h"
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
    /* @Autowired */
    Private INetworkStatusProviderPtr networkStatusProvider_;
    Private Int storedWifiConnectionId_;

    /**
     * Ensures WiFi and internet are up and Firebase is bound to current WiFi id.
     * If WiFi not connected or internet not connected: DismissConnection(), return false.
     * If WiFi id changed: RefreshConnection(), update stored id, return true.
     * Otherwise return true. Call before ReceiveMessage/SendMessage.
     */
    Private Bool EnsureNetworkAndFirebaseMatch() {
        if (!networkStatusProvider_->IsWiFiConnected()) {
            firebaseRequestManager->DismissConnection();
            return false;
        }
        if (!networkStatusProvider_->IsInternetConnected()) {
            firebaseRequestManager->DismissConnection();
            return false;
        }
        Int currentId = networkStatusProvider_->GetWifiConnectionId();
        if (currentId != storedWifiConnectionId_) {
            firebaseRequestManager->RefreshConnection();
            storedWifiConnectionId_ = currentId;
        }
        return true;
    }

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
        if (networkStatusProvider_ != nullptr) {
            storedWifiConnectionId_ = networkStatusProvider_->GetWifiConnectionId();
        }
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
        if (!EnsureNetworkAndFirebaseMatch()) {
            return nullptr;
        }
        if (!firebaseRequestManager) {
            Serial.println("[ArduinoFirebaseServer] ReceiveMessage: firebaseRequestManager is null");
            return nullptr;
        }

        StdString firstPair = firebaseRequestManager->RetrieveRequest();
        if (firstPair.empty()) {
            Serial.println("[ArduinoFirebaseServer] ReceiveMessage: RetrieveRequest returned empty");
            return nullptr;
        }

        Serial.print("[ArduinoFirebaseServer] ReceiveMessage: firstPair=");
        Serial.println(firstPair.c_str());

        Size colonPos = firstPair.find(':');
        StdString value = (colonPos != StdString::npos && colonPos + 1 < firstPair.size())
            ? firstPair.substr(colonPos + 1)
            : firstPair;
        if (value.empty()) {
            Serial.println("[ArduinoFirebaseServer] ReceiveMessage: value empty after parse");
            return nullptr;
        }

        Serial.print("[ArduinoFirebaseServer] ReceiveMessage: requestId=ignore value len=");
        Serial.println(value.size());

        StdString requestId = "ignore";
        receivedMessageCount_++;

        IHttpRequestPtr req = IHttpRequest::GetRequest(requestId, value);
        if (!req) {
            Serial.println("[ArduinoFirebaseServer] ReceiveMessage: IHttpRequest::GetRequest returned null");
            return nullptr;
        }
        Serial.println("[ArduinoFirebaseServer] ReceiveMessage: returning valid IHttpRequest");
        return req;
    }

    Public Virtual Bool SendMessage(CStdString& requestId, CStdString& message) override {
        (void)requestId;
        (void)message;
        if (!EnsureNetworkAndFirebaseMatch()) {
            return false;
        }
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
