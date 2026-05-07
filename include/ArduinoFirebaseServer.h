#ifndef ArduinoFirebaseServer_H
#define ArduinoFirebaseServer_H

#include "IServer.h"
#include "IHttpRequest.h"
#include <ILogger.h>
#include <Arduino.h>

#include "cloud/ICloudFacade.h"

/**
 * Firebase-style server implementation of IServer interface.
 * Header-only. Reads raw HTTP request from IArduinoRemoteStorage::GetCommand().
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
    Private ICloudFacadePtr cloudFacade;
    
    /* @Autowired */
    Private ILoggerPtr logger;

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
        Serial.print("[ArduinoFirebaseServer] Start() port=");
        Serial.println(static_cast<Int>(port_));
        Serial.println("[ArduinoFirebaseServer] Calling cloudFacade->StartCloudOperations()");
        cloudFacade->StartCloudOperations();
        Serial.println("[ArduinoFirebaseServer] Start() complete");
        return true;
    }

    Public Virtual Void Stop() override {
        Serial.println("[ArduinoFirebaseServer] Stop() called");
        running_ = false;
        cloudFacade->StopCloudOperations();
        Serial.println("[ArduinoFirebaseServer] Stop() complete");
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
        Serial.println("[ArduinoFirebaseServer] ReceiveMessage() polling cloudFacade");
        if (!cloudFacade) {
            Serial.println("[ArduinoFirebaseServer] ReceiveMessage() cloudFacade is null");
            return nullptr;
        }
        StdString firstPair = cloudFacade->GetCommand();
        Serial.print("[ArduinoFirebaseServer] cloudFacade->GetCommand() returned: ");
        Serial.println(firstPair.c_str());
        if (firstPair.empty()) {
            return nullptr;
        }

        Size colonPos = firstPair.find(':');
        StdString value = (colonPos != StdString::npos && colonPos + 1 < firstPair.size())
            ? firstPair.substr(colonPos + 1)
            : firstPair;
        if (value.empty()) {
            Serial.println("[ArduinoFirebaseServer] Parsed value empty, returning nullptr");
            return nullptr;
        }

        StdString requestId = GenerateGuid();
        receivedMessageCount_++;

        IHttpRequestPtr req = IHttpRequest::GetRequest(requestId, RequestSource::CloudServer, value);
        Serial.print("[ArduinoFirebaseServer] Built IHttpRequest body: ");
        Serial.println(value.c_str());
        logger->Info(Tag::Untagged, StdString("[ArduinoFirebaseServer] Received message"));
        if (!req) {
            logger->Error(Tag::Untagged, StdString("[ArduinoFirebaseServer] Failed to create request"));
            Serial.println("[ArduinoFirebaseServer] IHttpRequest::GetRequest failed");
            return nullptr;
        }
        return req;
    }

    Public Virtual Bool SendMessage(CStdString& requestId, CStdString& message) override {
        Serial.print("[ArduinoFirebaseServer] SendMessage requestId=");
        Serial.print(requestId.c_str());
        Serial.print(" message=");
        Serial.println(message.c_str());
        if (!running_) {
            logger->Warning(Tag::Untagged, StdString("[ArduinoFirebaseServer] SendMessage: server not running"));
            Serial.println("[ArduinoFirebaseServer] SendMessage failed: server not running");
            return false;
        }
        Serial.println("[ArduinoFirebaseServer] Forwarding to cloudFacade->PublishLogs(...)");
        StdMap<ULongLong, StdString> logs;
        logs[static_cast<ULongLong>(millis())] = message;
        Bool published = (cloudFacade != nullptr) ? cloudFacade->PublishLogs(logs) : false;
        Serial.print("[ArduinoFirebaseServer] cloudFacade->PublishLogs result -> ");
        Serial.println(published ? "OK" : "FAILED");
        sentMessageCount_++;
        return published;
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
