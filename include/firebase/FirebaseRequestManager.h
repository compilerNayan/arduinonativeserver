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

/* @Component */
class FirebaseRequestManager : public IFirebaseRequestManager {
    Private FirebaseData fbdo;
    Private FirebaseData fbdoDel;
    Private FirebaseAuth auth;
    Private FirebaseConfig config;
    Private Bool firebaseBegun = false;
    Private Bool streamBegun_ = false;
    Private std::atomic<bool> retrieving_{false};

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

    Private Void EnsureStreamBegin() {
        if (streamBegun_) return;
        if (!Firebase.RTDB.beginStream(&fbdo, kPath()))
            return;
        streamBegun_ = true;
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

    Public Virtual StdList<StdString> RetrieveRequests() override {
        if (retrieving_.exchange(true))
            return StdList<StdString>();
        struct ClearFlag {
            std::atomic<bool>& f;
            ~ClearFlag() { f.store(false); }
        } guard{retrieving_};

        StdList<StdString> result;
        EnsureFirebaseBegin();
        if (!Firebase.ready()) return result;

        EnsureStreamBegin();
        if (!Firebase.RTDB.readStream(&fbdo))
            return result;

        if (!fbdo.streamAvailable())
            return result;

        String raw = fbdo.to<String>();
        StdString payload(raw.c_str());
        StdList<StdString> keysReceived;
        ParseJsonToKeyValuePairs(payload, result, keysReceived);

        Firebase.RTDB.deleteNode(&fbdoDel, kPath());

        return result;
    }
};

#endif // FIREBASEREQUESTMANAGER_H
#endif // ARDUINO
