#ifndef ArduinoFirebaseServer_H
#define ArduinoFirebaseServer_H

#include "IServer.h"
#include "IHttpRequest.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>

// Firebase credentials (from exp1). WiFi is assumed already connected by the application.
#define ARDUINO_FIREBASE_HOST "smart-switch-da084-default-rtdb.asia-southeast1.firebasedatabase.app"
#define ARDUINO_FIREBASE_AUTH "Aj54Sf7eKxCaMIgTgEX4YotS8wbVpzmspnvK6X2C"
#define ARDUINO_FIREBASE_PATH "/"

/**
 * Firebase-style server implementation of IServer interface.
 * Uses FirebaseClient (async): get at path -> read data -> remove node.
 * Follows exp1 pattern: state 0=idle, 1=waiting get, 2=waiting remove.
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

    Private WiFiClientSecure ssl_client_;
    Private AsyncClientClass async_client_;
    Private LegacyToken auth_;
    Private FirebaseApp app_;
    Private RealtimeDatabase database_;
    Private AsyncResult dbResult_;
    Private Int state_;           // 0=idle, 1=waiting get, 2=waiting remove
    Private StdString pendingMessage_;
    Private Bool messageReady_;

    Static ArduinoFirebaseServer* s_instance_;

    Static Void OnFirebaseResult(AsyncResult& aResult) {
        if (s_instance_) {
            s_instance_->ProcessFirebaseResult(aResult);
        }
    }

    Void ProcessFirebaseResult(AsyncResult& aResult) {
        if (!aResult.isResult())
            return;
        if (aResult.isError()) {
            Int code = aResult.error().code();
            if (code == -118) {
                aResult.clear();
                state_ = 0;
                return;
            }
            Serial.printf("[ArduinoFirebaseServer] Error: %s code %d\n", aResult.error().message().c_str(), code);
            state_ = 0;
            return;
        }
        if (state_ == 1 && aResult.available()) {
            pendingMessage_ = StdString(aResult.c_str());
            state_ = 2;
            StdString path = GetFirebasePath();
            database_.remove(async_client_, path.c_str(), dbResult_);
            return;
        }
        if (state_ == 2) {
            messageReady_ = true;
            state_ = 0;
        }
    }

    Private StdString GenerateGuid() {
        StdString guid = "";
        Char hexChars[] = "0123456789abcdef";
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

    Private StdString GetFirebasePath() const {
        return StdString(ARDUINO_FIREBASE_PATH);
    }

    Public ArduinoFirebaseServer()
        : port_(DEFAULT_SERVER_PORT), running_(false),
          ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
          receivedMessageCount_(0), sentMessageCount_(0),
          maxMessageSize_(8192), receiveTimeout_(5000),
          auth_(ARDUINO_FIREBASE_AUTH), state_(0), messageReady_(false) {
    }

    Public ArduinoFirebaseServer(CUInt port)
        : port_(port), running_(false),
          ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
          receivedMessageCount_(0), sentMessageCount_(0),
          maxMessageSize_(8192), receiveTimeout_(5000),
          auth_(ARDUINO_FIREBASE_AUTH), state_(0), messageReady_(false) {
    }

    Public Virtual ~ArduinoFirebaseServer() override {
        if (s_instance_ == this)
            s_instance_ = nullptr;
        Stop();
    }

    Public Virtual Bool Start(CUInt port = DEFAULT_SERVER_PORT) override {
        if (running_) {
            return false;
        }
        port_ = port;
        s_instance_ = this;
        ssl_client_.setInsecure();
        ssl_client_.setHandshakeTimeout(5);
        initializeApp(async_client_, app_, auth_.get(), OnFirebaseResult, "auth");
        app_.getApp<RealtimeDatabase>(database_);
        database_.url(("https://" ARDUINO_FIREBASE_HOST));
        running_ = true;
        ipAddress_ = WiFi.status() == WL_CONNECTED ? StdString(WiFi.localIP().toString().c_str()) : StdString("0.0.0.0");
        return true;
    }

    Public Virtual Void Stop() override {
        if (s_instance_ == this)
            s_instance_ = nullptr;
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

    /**
     * ReceiveMessage: follows exp1 async pattern.
     * Calls app.loop() and processData; when a message is ready (get -> read -> remove done), returns it.
     */
    Public Virtual IHttpRequestPtr ReceiveMessage() override {
        if (!running_) {
            return nullptr;
        }
        app_.loop();
        ProcessFirebaseResult(dbResult_);

        if (messageReady_ && !pendingMessage_.empty()) {
            messageReady_ = false;
            StdString msg = pendingMessage_;
            pendingMessage_.clear();
            Serial.println("[ArduinoFirebaseServer] Receive message: ");
            receivedMessageCount_++;
            return IHttpRequest::GetRequest(GenerateGuid(), msg);
        }

        if (app_.ready() && state_ == 0) {
            state_ = 1;
            StdString path = GetFirebasePath();
            database_.get(async_client_, path.c_str(), dbResult_, false);
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

    Public Virtual StdString GetId() const override {
        return StdString("arduinofirebaseserver");
    }
};

inline ArduinoFirebaseServer* ArduinoFirebaseServer::s_instance_ = nullptr;

#endif // ArduinoFirebaseServer_H
