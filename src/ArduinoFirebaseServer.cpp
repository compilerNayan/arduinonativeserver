// Include FirebaseClient first so this TU never sees StandardDefines (List) before FirebaseClient.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>

#include "IServer.h"
#include "IHttpRequest.h"
#include "ArduinoFirebaseServer.h"

// ---------------------------------------------------------------------------
// Opaque implementation (FirebaseClient types only in this TU)
// ---------------------------------------------------------------------------
struct ArduinoFirebaseServer::Impl {
    WiFiClientSecure ssl_client_;
    AsyncClientClass async_client_;
    LegacyToken auth_;
    FirebaseApp app_;
    RealtimeDatabase database_;
    AsyncResult dbResult_;
    Int state_;
    StdString pendingMessage_;
    Bool messageReady_;

    Impl() : auth_(ARDUINO_FIREBASE_AUTH), state_(0), messageReady_(false) {}

    void ProcessFirebaseResult(AsyncResult& aResult) {
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

    StdString GetFirebasePath() const {
        return StdString(ARDUINO_FIREBASE_PATH);
    }
};

static StdString GenerateGuid() {
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

void OnFirebaseResult(firebase_ns::AsyncResult& aResult) {
    if (ArduinoFirebaseServer::s_instance_)
        ArduinoFirebaseServer::s_instance_->ProcessFirebaseResult(&aResult);
}

// ---------------------------------------------------------------------------
// ArduinoFirebaseServer implementation
// ---------------------------------------------------------------------------
ArduinoFirebaseServer* ArduinoFirebaseServer::s_instance_ = nullptr;

ArduinoFirebaseServer::ArduinoFirebaseServer()
    : port_(DEFAULT_SERVER_PORT), running_(false),
      ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
      receivedMessageCount_(0), sentMessageCount_(0),
      maxMessageSize_(8192), receiveTimeout_(5000),
      pImpl_(new Impl()) {
}

ArduinoFirebaseServer::ArduinoFirebaseServer(CUInt port)
    : port_(port), running_(false),
      ipAddress_("0.0.0.0"), lastClientIp_(""), lastClientPort_(0),
      receivedMessageCount_(0), sentMessageCount_(0),
      maxMessageSize_(8192), receiveTimeout_(5000),
      pImpl_(new Impl()) {
}

ArduinoFirebaseServer::~ArduinoFirebaseServer() {
    if (s_instance_ == this)
        s_instance_ = nullptr;
    Stop();
    delete pImpl_;
    pImpl_ = nullptr;
}

void ArduinoFirebaseServer::ProcessFirebaseResult(Void* rawResult) {
    if (pImpl_ && rawResult)
        pImpl_->ProcessFirebaseResult(*reinterpret_cast<AsyncResult*>(rawResult));
}

Bool ArduinoFirebaseServer::Start(CUInt port) {
    if (running_)
        return false;
    port_ = port;
    s_instance_ = this;
    Impl& i = *pImpl_;
    i.ssl_client_.setInsecure();
    i.ssl_client_.setHandshakeTimeout(5);
    initializeApp(i.async_client_, i.app_, i.auth_.get(), OnFirebaseResult, "auth");
    i.app_.getApp<RealtimeDatabase>(i.database_);
    i.database_.url(("https://" ARDUINO_FIREBASE_HOST));
    running_ = true;
    ipAddress_ = WiFi.status() == WL_CONNECTED ? StdString(WiFi.localIP().toString().c_str()) : StdString("0.0.0.0");
    return true;
}

Void ArduinoFirebaseServer::Stop() {
    if (s_instance_ == this)
        s_instance_ = nullptr;
    running_ = false;
}

IHttpRequestPtr ArduinoFirebaseServer::ReceiveMessage() {
    if (!running_)
        return nullptr;
    Impl& i = *pImpl_;
    i.app_.loop();
    ProcessFirebaseResult(&i.dbResult_);

    if (i.messageReady_ && !i.pendingMessage_.empty()) {
        i.messageReady_ = false;
        StdString msg = i.pendingMessage_;
        i.pendingMessage_.clear();
        Serial.println("[ArduinoFirebaseServer] Receive message: ");
        receivedMessageCount_++;
        return IHttpRequest::GetRequest(GenerateGuid(), msg);
    }

    if (i.app_.ready() && i.state_ == 0) {
        i.state_ = 1;
        StdString path = i.GetFirebasePath();
        i.database_.get(i.async_client_, path.c_str(), i.dbResult_, false);
    }
    return nullptr;
}
