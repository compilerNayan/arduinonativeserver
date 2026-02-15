#ifdef ARDUINO
#ifndef FIREBASEREQUESTMANAGER_H
#define FIREBASEREQUESTMANAGER_H

#define Vector __FirebaseVector
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#undef Vector

#include "IFirebaseRequestManager.h"
#include <atomic>
#include <queue>
#include <mutex>

/* @Component */
class FirebaseRequestManager : public IFirebaseRequestManager {
    Private FirebaseData fbdo;
    Private FirebaseData fbdoDel;
    Private FirebaseAuth auth;
    Private FirebaseConfig config;
    Private Bool firebaseBegun = false;
    Private Bool streamBegun_ = false;
    Private std::atomic<bool> retrieving_{false};
    Private std::queue<StdString> requestQueue_;
    Private std::mutex requestQueueMutex_;

    Private Bool TryDequeue(StdString& out) {
        std::lock_guard<std::mutex> lock(requestQueueMutex_);
        if (requestQueue_.empty()) return false;
        out = requestQueue_.front();
        requestQueue_.pop();
        return true;
    }

    Private Void EnqueueRequest(const StdString& s) {
        std::lock_guard<std::mutex> lock(requestQueueMutex_);
        requestQueue_.push(s);
    }

    Private Static const char* kDatabaseUrl() { return "https://smart-switch-da084-default-rtdb.asia-southeast1.firebasedatabase.app"; }
    Private Static const char* kLegacyToken() { return "Aj54Sf7eKxCaMIgTgEX4YotS8wbVpzmspnvK6X2C"; }
    Private Static const char* kPath() { return "/"; }

    Private Void EnsureFirebaseBegin() {
        if (firebaseBegun) return;
        Serial.println("RetrieveRequest(exp1): EnsureFirebaseBegin first time (Firebase.begin)");
        config.database_url = kDatabaseUrl();
        config.signer.tokens.legacy_token = kLegacyToken();
        fbdo.setBSSLBufferSize(4096, 1024);
        fbdo.setResponseSize(2048);
        fbdoDel.setBSSLBufferSize(4096, 1024);
        fbdoDel.setResponseSize(2048);
        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);
        firebaseBegun = true;
        delay(500);
        Serial.println("RetrieveRequest(exp1): Firebase.begin done");
    }

    Private Void EnsureStreamBegin() {
        if (streamBegun_) return;
        if (!Firebase.RTDB.beginStream(&fbdo, kPath())) {
            Serial.println("RetrieveRequest(exp1): beginStream failed");
            return;
        }
        streamBegun_ = true;
        Serial.println("RetrieveRequest(exp1): beginStream ok");
    }

    Private Static Void ParseJsonToKeyValuePairs(const StdString& jsonStr, StdList<StdString>& out, StdList<StdString>& outKeys) {
        DynamicJsonDocument doc(2048);
        DeserializationError err = deserializeJson(doc, jsonStr);
        if (err) return;
        if (!doc.is<JsonObject>()) return;
        JsonObject root = doc.as<JsonObject>();
        for (JsonPair p : root) {
            StdString key(p.key().c_str());
            outKeys.push_back(key);
            StdString value;
            if (p.value().is<const char*>())
                value = p.value().as<const char*>();
            else if (p.value().is<int>())
                value = std::to_string(p.value().as<int>());
            else if (p.value().is<bool>())
                value = p.value().as<bool>() ? "true" : "false";
            else if (p.value().is<double>())
                value = std::to_string(p.value().as<double>());
            else {
                String s;
                serializeJson(p.value(), s);
                value = StdString(s.c_str());
            }
            out.push_back(key + ":" + value);
        }
    }

    Public FirebaseRequestManager() = default;

    Public Virtual ~FirebaseRequestManager() = default;

    Public Virtual StdString RetrieveRequest() override {
        StdString fromQueue;
        if (TryDequeue(fromQueue)) {
            Serial.print("RetrieveRequest(exp1): from queue, length=");
            Serial.println(fromQueue.size());
            return fromQueue;
        }

        if (retrieving_.exchange(true)) {
            Serial.println("RetrieveRequest(exp1): another retrieve in progress, return empty");
            return StdString();
        }
        struct ClearFlag {
            std::atomic<bool>& f;
            ~ClearFlag() { f.store(false); }
        } guard{retrieving_};

        Serial.println("RetrieveRequest(exp1): fetching from Firebase");
        StdList<StdString> result;
        EnsureFirebaseBegin();
        if (!Firebase.ready()) {
            Serial.println("RetrieveRequest(exp1): Firebase not ready");
            return StdString();
        }

        EnsureStreamBegin();
        if (!Firebase.RTDB.readStream(&fbdo)) {
            Serial.print("RetrieveRequest(exp1): readStream failed ");
            Serial.println(fbdo.errorReason().c_str());
            return StdString();
        }

        if (!fbdo.streamAvailable()) {
            Serial.println("RetrieveRequest(exp1): stream not available");
            return StdString();
        }

        String raw = fbdo.to<String>();
        StdString payload(raw.c_str());
        Serial.print("RetrieveRequest(exp1): payload length=");
        Serial.println(payload.size());

        StdList<StdString> keysReceived;
        ParseJsonToKeyValuePairs(payload, result, keysReceived);
        Serial.print("RetrieveRequest(exp1): parsed ");
        Serial.print(result.size());
        Serial.println(" key:value pair(s)");

        Firebase.RTDB.deleteNode(&fbdoDel, kPath());
        Serial.println("RetrieveRequest(exp1): deleteNode done");

        for (const StdString& s : result)
            EnqueueRequest(s);

        if (TryDequeue(fromQueue)) {
            Serial.print("RetrieveRequest(exp1): return first from new batch, length=");
            Serial.println(fromQueue.size());
            return fromQueue;
        }
        Serial.println("RetrieveRequest(exp1): queue empty after enqueue, return empty");
        return StdString();
    }
};

#endif // FIREBASEREQUESTMANAGER_H
#endif // ARDUINO
