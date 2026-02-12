#ifndef ArduinoFirebaseServer_H
#define ArduinoFirebaseServer_H

#include "IServer.h"
#include "IHttpRequest.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>

// Firebase credentials (from exp1). WiFi is assumed already connected by the application.
#define ARDUINO_FIREBASE_HOST "smart-switch-da084-default-rtdb.asia-southeast1.firebasedatabase.app"
#define ARDUINO_FIREBASE_AUTH "Aj54Sf7eKxCaMIgTgEX4YotS8wbVpzmspnvK6X2C"
#define ARDUINO_FIREBASE_PATH ""

/**
 * Firebase-style server implementation of IServer interface.
 * Fetches latest key-value from Firebase (consume-on-read), parses value as HTTP request.
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

    /** Fetches the latest child via REST, then deletes it from Firebase (consume-on-read). */
    Private Bool GetLatestFromFirebase(StdString& outKey, StdString& outValue) {
        if (WiFi.status() != WL_CONNECTED) {
            return false;
        }
        String path = String(ARDUINO_FIREBASE_PATH);
        if (path.length() && !path.startsWith("/")) path = "/" + path;
        if (path.length() == 0) path = "/";
        String url = "https://" + String(ARDUINO_FIREBASE_HOST) + path + ".json?orderBy=%22%24key%22&limitToLast=1&auth=" + String(ARDUINO_FIREBASE_AUTH);

        const int maxRetries = 4;
        const int retryDelayMs = 400;
        HTTPClient http;
        int code = -1;
        for (int attempt = 0; attempt < maxRetries; attempt++) {
            http.begin(url);
            code = http.GET();
            if (code == 200) {
                break;
            }
            http.end();
            if (attempt < maxRetries - 1) {
                delay(retryDelayMs);
            }
        }
        Bool ok = (code == 200);
        if (ok) {
            String payload = http.getString();
            http.end();

            DynamicJsonDocument doc(1024);
            if (deserializeJson(doc, payload) != DeserializationError::Ok || !doc.is<JsonObject>()) {
                outKey = "";
                outValue = StdString(payload.c_str());
                return true;
            }
            JsonObject obj = doc.as<JsonObject>();
            if (obj.size() == 0) {
                outKey = "";
                outValue = "";
                return true;
            }
            JsonPair p = *obj.begin();
            outKey = StdString(p.key().c_str());
            if (p.value().is<String>())
                outValue = StdString(p.value().as<String>().c_str());
            else {
                String valStr;
                serializeJson(p.value(), valStr);
                outValue = StdString(valStr.c_str());
            }

            if (outKey.length() > 0 && Firebase.ready()) {
                String nodePath = path;
                if (!nodePath.endsWith("/")) nodePath += "/";
                nodePath += outKey.c_str();
                if (!Firebase.RTDB.deleteNode(&fbdo_, nodePath.c_str())) {
                    Serial.printf("[ArduinoFirebaseServer] Firebase delete failed: %s\n", fbdo_.errorReason().c_str());
                }
            }
        } else {
            http.end();
            Serial.printf("[ArduinoFirebaseServer] HTTP %d (after %d attempt(s))\n", code, maxRetries);
        }
        return ok;
    }

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

    Public Virtual IHttpRequestPtr ReceiveMessage() override {
        if (!running_) {
            return nullptr;
        }
        StdString outKey, outValue;
        if (!GetLatestFromFirebase(outKey, outValue)) {
            return nullptr;
        }
        if (outValue.empty()) {
            return nullptr;
        }
        StdString requestId = GenerateGuid();
        receivedMessageCount_++;
        return IHttpRequest::GetRequest(requestId, outValue);
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
