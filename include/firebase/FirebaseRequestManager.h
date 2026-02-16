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
    Private std::atomic<bool> refreshing_{false};
    Private std::atomic<bool> pendingRefresh_{false};
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
    }

    Private Bool EnsureStreamBegin() {
        if (streamBegun_) return true;
        if (!Firebase.RTDB.beginStream(&fbdo, kPath())) {
            return false;
        }
        streamBegun_ = true;
        return true;
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

    Private Void OnErrorAndScheduleRefresh(const char* msg) {
        Serial.print("[FirebaseRequestManager] RetrieveRequest failed: ");
        Serial.println(msg);
        pendingRefresh_.store(true);
    }

    Public Virtual StdString RetrieveRequest() override {
        StdString fromQueue;
        if (TryDequeue(fromQueue)) {
            return fromQueue;
        }

        if (pendingRefresh_.exchange(false)) {
            RefreshConnection();
        }

        while (refreshing_.load(std::memory_order_relaxed)) {
            delay(10);
        }

        if (retrieving_.exchange(true)) {
            return StdString();
        }
        struct ClearFlag {
            std::atomic<bool>& f;
            ~ClearFlag() { f.store(false); }
        } guard{retrieving_};

        StdList<StdString> result;
        EnsureFirebaseBegin();
        if (!Firebase.ready()) {
            OnErrorAndScheduleRefresh("Firebase not ready");
            return StdString();
        }

        if (!EnsureStreamBegin()) {
            OnErrorAndScheduleRefresh("beginStream failed");
            return StdString();
        }

        if (!Firebase.RTDB.readStream(&fbdo)) {
            OnErrorAndScheduleRefresh(fbdo.errorReason().c_str());
            return StdString();
        }

        if (!fbdo.streamAvailable()) {
            //OnErrorAndScheduleRefresh("stream not available");
            return StdString();
        }

        String raw = fbdo.to<String>();
        StdString payload(raw.c_str());

        StdList<StdString> keysReceived;
        ParseJsonToKeyValuePairs(payload, result, keysReceived);

       // if (!Firebase.RTDB.deleteNode(&fbdoDel, kPath())) {
       //     OnErrorAndScheduleRefresh(fbdoDel.errorReason().c_str());
       // }

        for (const StdString& s : result)
            EnqueueRequest(s);

        if (TryDequeue(fromQueue)) {
            return fromQueue;
        }
        return StdString();
    }

    Public Virtual Void RefreshConnection() override {
        while (retrieving_.load(std::memory_order_relaxed)) {
            delay(10);
        }
        refreshing_.store(true, std::memory_order_release);
        struct ClearRefreshing {
            std::atomic<bool>& f;
            ~ClearRefreshing() { f.store(false); }
        } guard{refreshing_};

        // Always call endStream to clear library state (even if we think stream wasn't started).
        // Stuck internal state after a failure can cause beginStream to fail until reboot otherwise.
        Firebase.RTDB.endStream(&fbdo);
        streamBegun_ = false;
        firebaseBegun = false;
        delay(200);

        EnsureFirebaseBegin();
        if (Firebase.ready()) {
            EnsureStreamBegin();
        }
    }

    Public Virtual Void DismissConnection() override {
        while (retrieving_.load(std::memory_order_relaxed)) {
            delay(10);
        }
        // Always call endStream to clear library state (even if streamBegun_ was false).
        Firebase.RTDB.endStream(&fbdo);
        streamBegun_ = false;
        firebaseBegun = false;
    }
};

#endif // FIREBASEREQUESTMANAGER_H
#endif // ARDUINO
