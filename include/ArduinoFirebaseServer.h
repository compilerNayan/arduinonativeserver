#ifndef ArduinoFirebaseServer_H
#define ArduinoFirebaseServer_H

#include "IServer.h"
#include "IHttpRequest.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Firebase credentials (from exp1). WiFi is assumed already connected by the application.
#define ARDUINO_FIREBASE_HOST "smart-switch-da084-default-rtdb.asia-southeast1.firebasedatabase.app"
#define ARDUINO_FIREBASE_AUTH "Aj54Sf7eKxCaMIgTgEX4YotS8wbVpzmspnvK6X2C"
#define ARDUINO_FIREBASE_PATH ""

/**
 * Firebase-style server implementation of IServer interface.
 * Reads message from Firebase at path via Firebase.RTDB.get(), deletes node after read,
 * parses value as HTTP request.
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
    Private FirebaseData fbdo_;
    Private FirebaseAuth auth_;
    Private FirebaseConfig config_;
    Private ULong lastReceiveCallMs_;

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

    /** Returns the Firebase path to read (normalized, e.g. "/" when empty). */
    Private StdString GetFirebasePath() const {
        String path = String(ARDUINO_FIREBASE_PATH);
        if (path.length() == 0) return StdString("/");
        if (path[0] != '/') return StdString("/") + StdString(path.c_str());
        return StdString(path.c_str());
    }

    /**
     * Read message from Firebase at path, then delete the node (consume-on-read).
     * Uses Firebase.RTDB.get() and Firebase.RTDB.deleteNode().
     * @param outMessage The value at path as string (e.g. raw HTTP request); unchanged on failure
     * @return true if get succeeded (outMessage may be empty if node was empty), false on error
     */
    Private Bool ReadMessageFromFirebaseAndDelete(StdString& outMessage) {
        if (WiFi.status() != WL_CONNECTED) {
            return false;
        }
        if (!Firebase.ready()) {
            return false;
        }
        StdString path = GetFirebasePath();
        if (path.empty()) {
            return false;
        }
        if (!Firebase.RTDB.get(&fbdo_, path.c_str())) {
            Serial.printf("[ArduinoFirebaseServer] Firebase get failed: %s\n", fbdo_.errorReason().c_str());
            return false;
        }
        outMessage = StdString(fbdo_.to<String>().c_str());
        if (!Firebase.RTDB.deleteNode(&fbdo_, path.c_str())) {
            Serial.printf("[ArduinoFirebaseServer] Firebase delete failed: %s\n", fbdo_.errorReason().c_str());
        }
        return true;
    }

    Public ArduinoFirebaseServer()
        : port_(DEFAULT_SERVER_PORT), running_(false),
          ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
          receivedMessageCount_(0), sentMessageCount_(0),
          maxMessageSize_(8192), receiveTimeout_(5000), lastReceiveCallMs_(0) {
    }

    Public ArduinoFirebaseServer(CUInt port)
        : port_(port), running_(false),
          ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
          receivedMessageCount_(0), sentMessageCount_(0),
          maxMessageSize_(8192), receiveTimeout_(5000), lastReceiveCallMs_(0) {
    }

    Public Virtual ~ArduinoFirebaseServer() override {
        Stop();
    }

    Public Virtual Bool Start(CUInt port = DEFAULT_SERVER_PORT) override {
        if (running_) {
            return false;
        }
        port_ = port;
        config_.database_url = String("https://") + ARDUINO_FIREBASE_HOST;
        config_.signer.tokens.legacy_token = ARDUINO_FIREBASE_AUTH;
        fbdo_.setBSSLBufferSize(4096, 1024);
        fbdo_.setResponseSize(2048);
        Firebase.begin(&config_, &auth_);
        Firebase.reconnectWiFi(true);
        running_ = true;
        ipAddress_ = WiFi.status() == WL_CONNECTED ? StdString(WiFi.localIP().toString().c_str()) : StdString("0.0.0.0");
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

    /**
     * ReceiveMessage: follows exp1 pattern (get at path -> read data -> delete node).
     * When idle and ready, do get; if data available, use it and delete; then throttle before next get.
     */
    Public Virtual IHttpRequestPtr ReceiveMessage() override {
        if (!running_) {
            return nullptr;
        }
        if (WiFi.status() != WL_CONNECTED) {
            return nullptr;
        }
        if (!Firebase.ready()) {
            return nullptr;
        }
        const ULong throttleMs = 1000;
        if ((ULong)(millis() - lastReceiveCallMs_) < throttleMs) {
            return nullptr;
        }
        StdString path = GetFirebasePath();
        if (path.empty()) {
            return nullptr;
        }
        if (!Firebase.RTDB.get(&fbdo_, path.c_str())) {
            Serial.printf("[ArduinoFirebaseServer] Firebase get failed: %s\n", fbdo_.errorReason().c_str());
            return nullptr;
        }
        StdString outMessage = StdString(fbdo_.to<String>().c_str());
        if (outMessage.empty()) {
            return nullptr;
        }
        if (!Firebase.RTDB.deleteNode(&fbdo_, path.c_str())) {
            Serial.printf("[ArduinoFirebaseServer] Firebase delete failed: %s\n", fbdo_.errorReason().c_str());
        }
        lastReceiveCallMs_ = millis();
        Serial.println("[ArduinoFirebaseServer] Receive message: ");
        receivedMessageCount_++;
        return IHttpRequest::GetRequest(GenerateGuid(), outMessage);
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
