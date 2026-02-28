#ifdef ARDUINO
#ifndef FIREBASEOPERATIONS_H
#define FIREBASEOPERATIONS_H

#include "IFirebaseOperations.h"
#include <ILogger.h>
#include <IDeviceDetails.h>

#define Vector __FirebaseVector
#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#undef Vector

#include <atomic>
#include <ctime>
#include <set>

class FirebaseOperations : public IFirebaseOperations {
    /* @Autowired */
    Private ILoggerPtr logger;
    /* @Autowired */
    Private IDeviceDetailsPtr deviceDetails_;
    Private Int storedWifiConnectionId_{0};

    Private FirebaseData fbdo;
    Private FirebaseData fbdoDel;
    Private FirebaseData fbdoLog;
    Private FirebaseAuth auth;
    Private FirebaseConfig config;
    Private Bool firebaseBegun = false;
    Private Bool streamBegun_ = false;
    /** Only one of RetrieveCommands or PublishLogs may run at a time. */
    Private std::atomic<bool> operationInProgress_{false};
    /** When true, all public methods return default/empty without doing work. Set on any error; clear via ResetConnection(). */
    Private std::atomic<bool> dirty_{false};

    Private Static const char* kDatabaseUrl() { return "https://smart-switch-da084-default-rtdb.asia-southeast1.firebasedatabase.app"; }
    Private Static const char* kLegacyToken() { return "Aj54Sf7eKxCaMIgTgEX4YotS8wbVpzmspnvK6X2C"; }
    Private Static const unsigned long kDeleteIntervalMs = 60000;
    Private unsigned long lastDeleteMillis_ = 0;
    /** Keys we have already processed; duplicate keys are ignored. Cleared when we delete the commands node. */
    Private std::set<StdString> seenCommandKeys_;

    Private StdString GetCommandsPath() const {
        return "/" + deviceDetails_->GetSerialNumber() + "/commands";
    }
    Private StdString GetLogsPath() const {
        return "/" + deviceDetails_->GetSerialNumber() + "/logs";
    }
    /** UTC ms when millis() was 0; set when converting pre-NTP keys (key = millis*1000+seq). ULongLong to avoid 32-bit truncation. */
    Private ULongLong epochOffsetMs_{0};

    Private Void EnsureFirebaseBegin() {
        if (firebaseBegun) return;
        config.database_url = kDatabaseUrl();
        config.signer.tokens.legacy_token = kLegacyToken();
        fbdo.setBSSLBufferSize(4096, 1024);
        fbdo.setResponseSize(2048);
        fbdoDel.setBSSLBufferSize(4096, 1024);
        fbdoDel.setResponseSize(2048);
        fbdoLog.setBSSLBufferSize(4096, 1024);
        fbdoLog.setResponseSize(2048);
        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);
        firebaseBegun = true;
        delay(500);
    }

    Private Bool EnsureStreamBegin() {
        if (streamBegun_) return true;
        StdString cmdPath = GetCommandsPath();
        if (!Firebase.RTDB.beginStream(&fbdo, cmdPath.c_str())) {
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

    Private Void OnErrorAndScheduleRefresh(const char* msg) {
        logger->Error(Tag::Untagged, StdString(std::string("[FirebaseOperations] RetrieveCommands failed: ") + msg));
        dirty_.store(true);
    }


    /** Returns false if dirty, network/Firebase mismatch, or Firebase not ready. Otherwise ensures Firebase begun and returns true. */
    Private Bool EnsureReady() {
        if (dirty_.load(std::memory_order_relaxed)) return false;
        EnsureFirebaseBegin();
        if (!Firebase.ready()) {
            dirty_.store(true);
            return false;
        }
        return true;
    }

    /** Convert key (UTC ms or sec*1000+seq) to Firebase-safe key in local time "2026-02-17T18-30-00_123" (no Z; TZ set by DeviceTimeSyncNtp). When NTP not synced, format as "1970-01-01T00-29-12_041" using value as sec since epoch. */
    Private StdString MillisToIso8601(ULongLong timestampMs) {
        auto FormatAsTime = [](ULongLong ms) -> StdString {
            time_t sec = static_cast<time_t>(ms / 1000);
            unsigned int subMs = static_cast<unsigned int>(ms % 1000);
            struct tm* t = localtime(&sec);
            if (!t) return "millis_" + std::to_string((unsigned long long)ms);
            char buf[32];
            if (strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", t) == 0) return "millis_" + std::to_string((unsigned long long)ms);
            char out[40];
            snprintf(out, sizeof(out), "%s_%03u", buf, subMs);
            return StdString(out);
        };
        ULongLong utcMs;
        if (timestampMs >= 1000000000000ULL) {
            utcMs = timestampMs;
        } else {
            if (epochOffsetMs_ == 0) {
                time_t now = time(nullptr);
                if (now >= 978307200) {
                    epochOffsetMs_ = (ULongLong)now * 1000ULL - (ULongLong)millis();
                } else {
                    return FormatAsTime(timestampMs);
                }
            }
            utcMs = epochOffsetMs_ + timestampMs;
        }
        time_t sec = static_cast<time_t>(utcMs / 1000);
        unsigned int ms = static_cast<unsigned int>(utcMs % 1000);
        struct tm* t = localtime(&sec);
        if (!t) return FormatAsTime(utcMs);
        char buf[32];
        size_t n = strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", t);
        if (n == 0) return FormatAsTime(utcMs);
        char out[40];
        snprintf(out, sizeof(out), "%s_%03u", buf, ms);
        return StdString(out);
    }

    /** Returns all key:value pairs from one read. Uses GET (not stream) so the device
     *  sees commands regardless of who wrote them. Call only while operationInProgress_ is held. */
    Private StdVector<StdString> RetrieveCommandsFromFirebase() {
        StdVector<StdString> emptyResult;
        EnsureFirebaseBegin();
        StdString cmdPath = GetCommandsPath();

        if (!Firebase.RTDB.get(&fbdo, cmdPath.c_str())) {
            OnErrorAndScheduleRefresh(fbdo.errorReason().c_str());
            return emptyResult;
        }

        if (fbdo.dataType() == "null" || !fbdo.dataAvailable()) {
            unsigned long now = millis();
            if (now - lastDeleteMillis_ >= kDeleteIntervalMs) {
                if (!Firebase.RTDB.deleteNode(&fbdoDel, cmdPath.c_str())) {
                    OnErrorAndScheduleRefresh(fbdoDel.errorReason().c_str());
                } else {
                    lastDeleteMillis_ = now;
                    seenCommandKeys_.clear();
                }
            }
            return emptyResult;
        }

        String raw = fbdo.to<String>();
        StdString payload(raw.c_str());

        StdList<StdString> result;
        StdList<StdString> keysReceived;
        ParseJsonToKeyValuePairs(payload, result, keysReceived);

        StdVector<StdString> out;
        auto keyIt = keysReceived.begin();
        auto resultIt = result.begin();
        for (; keyIt != keysReceived.end() && resultIt != result.end(); ++keyIt, ++resultIt) {
            const StdString& key = *keyIt;
            if (seenCommandKeys_.find(key) != seenCommandKeys_.end())
                continue;
            seenCommandKeys_.insert(key);
            out.push_back(*resultIt);
        }

        /* unsigned long now = millis();
        if (now - lastDeleteMillis_ >= kDeleteIntervalMs) {
            if (!Firebase.RTDB.deleteNode(&fbdoDel, cmdPath.c_str())) {
                OnErrorAndScheduleRefresh(fbdoDel.errorReason().c_str());
            } else {
                lastDeleteMillis_ = now;
                seenCommandKeys_.clear();
            }
        } */  

        return out;
    }

    Public FirebaseOperations() = default;

    Public Virtual ~FirebaseOperations() override {
        dirty_.store(true);
        const ULong kShutdownWaitMs = 3000u;
        ULong deadline = (ULong)millis() + kShutdownWaitMs;
        while (operationInProgress_.load(std::memory_order_relaxed) && (ULong)millis() < deadline) {
            delay(10);
        }
        if (streamBegun_ || firebaseBegun) {
            Firebase.RTDB.endStream(&fbdo);
            streamBegun_ = false;
            firebaseBegun = false;
            delay(100);
        }
    }

    Public FirebaseOperationResult RetrieveCommands(StdVector<StdString>& out) override {
        out.clear();
        if (operationInProgress_.exchange(true)) {
            return FirebaseOperationResult::AnotherOperationInProgress;
        }
        struct ClearOp {
            std::atomic<bool>& f;
            ~ClearOp() { f.store(false); }
        } guard{operationInProgress_};
        if (!EnsureReady()) return FirebaseOperationResult::NotReady;
        out = RetrieveCommandsFromFirebase();
        return FirebaseOperationResult::OperationSucceeded;
    }

    Public FirebaseOperationResult PublishLogs(const StdMap<ULongLong, StdString>& logs) override {
        if (operationInProgress_.exchange(true)) {
            return FirebaseOperationResult::AnotherOperationInProgress;
        }
        struct ClearOp {
            std::atomic<bool>& f;
            ~ClearOp() { f.store(false); }
        } guard{operationInProgress_};
        if (!EnsureReady()) return FirebaseOperationResult::NotReady;
        DynamicJsonDocument doc(16384);
        JsonObject root = doc.to<JsonObject>();
        for (const auto& pair : logs) {
            const StdString& message = pair.second;
            if (message.empty()) continue;
            root[MillisToIso8601(pair.first).c_str()] = message.c_str();
        }
        if (root.size() == 0) {
            return FirebaseOperationResult::OperationSucceeded;
        }
        String jsonStr;
        serializeJson(doc, jsonStr);
        FirebaseJson fbJson;
        fbJson.setJsonData(jsonStr);
        time_t nowSec = time(nullptr);
        ULong nowMs = (nowSec != (time_t)-1) ? (ULong)nowSec * 1000ULL : (ULong)millis();
        StdString uniqueKey = MillisToIso8601(nowMs);
        StdString path = GetLogsPath() + "/" + uniqueKey;
        if (!Firebase.RTDB.setJSON(&fbdoLog, path.c_str(), &fbJson)) {
            const char* reason = fbdoLog.errorReason().c_str();
            StdString msg = StdString("[FirebaseOperations] PublishLogs failed: ") + (reason && *reason ? StdString(reason) : StdString("(no reason)"));
            logger->Error(Tag::Untagged, msg);
            dirty_.store(true);
            return FirebaseOperationResult::Failed;
        }
        return FirebaseOperationResult::OperationSucceeded;
    }

    /** Returns true if RetrieveCommands or PublishLogs is currently running. */
    Public Virtual Bool IsOperationInProgress() const override {
        return operationInProgress_.load(std::memory_order_relaxed);
    }

    /** Returns true if the instance is dirty (e.g. after an error); public methods will return default/empty until reset. */
    Public Virtual Bool IsDirty() const override {
        return dirty_.load(std::memory_order_relaxed);
    }
};

#endif // FIREBASEOPERATIONS_H
#endif // ARDUINO
