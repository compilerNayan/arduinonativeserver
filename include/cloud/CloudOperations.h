#ifndef CLOUDOPERATIONS_H
#define CLOUDOPERATIONS_H

#include "ICloudOperations.h"
#include "IAwsIotCoreOperations.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ILogger.h>
#include <atomic>

class CloudOperations : public ICloudOperations {
    Public CloudOperations() = default;

    Public StdVector<StdString> RetrieveCommands() override {
        Serial.println("[CloudOperations] RetrieveCommands() begin");
        if (dirty_.load(std::memory_order_relaxed)) {
            Serial.println("[CloudOperations] RetrieveCommands skip: dirty");
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] RetrieveCommands skip: dirty"));
            return {};
        }
        if (operationInProgress_.exchange(true)) {
            Serial.println("[CloudOperations] RetrieveCommands skip: operation in progress");
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] RetrieveCommands skip: operation already in progress"));
            return {};
        }
        struct Guard { std::atomic<bool>& f; ~Guard() { f.store(false); } } g{operationInProgress_};
        if (awsIotCoreOperations_ == nullptr) {
            Serial.println("[CloudOperations] RetrieveCommands failed: awsIotCoreOperations null");
            if (logger) logger->Error(Tag::Untagged, StdString("[CloudOperations] RetrieveCommands: awsIotCoreOperations not available"));
            return {};
        }
        Serial.println("[CloudOperations] Calling awsIotCoreOperations_->ReceiveMessages()");
        StdVector<StdString> incoming = awsIotCoreOperations_->ReceiveMessages();
        Serial.print("[CloudOperations] Raw incoming count=");
        Serial.println(static_cast<Int>(incoming.size()));
        StdVector<StdString> out;
        out.reserve(incoming.size());
        for (const auto& msg : incoming) {
            Serial.print("[CloudOperations] Raw incoming payload: ");
            Serial.println(msg.c_str());
            if (IsDonePayload(msg)) {
                Serial.println("[CloudOperations] Payload treated as done marker");
                if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] RetrieveCommands: done payload received"));
                continue;
            }
            StdString cmd = ParseCommandPayload(msg);
            if (!cmd.empty()) {
                Serial.print("[CloudOperations] Parsed command: ");
                Serial.println(cmd.c_str());
                out.push_back(cmd);
            }
        }
        Serial.print("[CloudOperations] Returning command count=");
        Serial.println(static_cast<Int>(out.size()));
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] RetrieveCommands: got ") + std::to_string(out.size()) + " command(s)");
        return out;
    }

    Public Bool PublishLogs(const StdMap<ULongLong, StdString>& logs) override {
        Serial.print("[CloudOperations] PublishLogs() count=");
        Serial.println(static_cast<Int>(logs.size()));
        if (dirty_.load(std::memory_order_relaxed)) {
            Serial.println("[CloudOperations] PublishLogs skip: dirty");
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] PublishLogs skip: dirty"));
            return false;
        }
        if (operationInProgress_.exchange(true)) {
            Serial.println("[CloudOperations] PublishLogs skip: operation in progress");
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] PublishLogs skip: operation already in progress"));
            return false;
        }
        struct Guard { std::atomic<bool>& f; ~Guard() { f.store(false); } } g{operationInProgress_};
        if (awsIotCoreOperations_ == nullptr) {
            Serial.println("[CloudOperations] PublishLogs failed: awsIotCoreOperations null");
            if (logger) logger->Error(Tag::Untagged, StdString("[CloudOperations] PublishLogs: awsIotCoreOperations not available"));
            return false;
        }
        if (logs.empty()) return true;
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] PublishLogs: publishing ") + std::to_string(logs.size()) + " log(s)");
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        for (const auto& p : logs) {
            root[std::to_string(p.first).c_str()] = p.second.c_str();
        }
        char buf[1024];
        size_t n = serializeJson(doc, buf, sizeof(buf));
        Serial.print("[CloudOperations] Serialized log payload size=");
        Serial.println(static_cast<Int>(n));
        if (n >= sizeof(buf)) {
            Serial.println("[CloudOperations] PublishLogs failed: payload too large");
            if (logger) logger->Error(Tag::Untagged, StdString("[CloudOperations] PublishLogs: serialized payload too large"));
            return false;
        }
        Serial.print("[CloudOperations] Sending payload: ");
        Serial.println(buf);
        Bool ok = awsIotCoreOperations_->SendMessage(StdString(buf, n));
        Serial.print("[CloudOperations] awsIotCoreOperations_->SendMessage -> ");
        Serial.println(ok ? "OK" : "FAILED");
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] PublishLogs: ") + (ok ? "ok" : "publish failed"));
        return ok;
    }

    Public Bool IsOperationInProgress() const override {
        return operationInProgress_.load(std::memory_order_relaxed);
    }

    Public Bool IsDirty() const override {
        return dirty_.load(std::memory_order_relaxed);
    }

    /* @Autowired */
    Private IAwsIotCoreOperationsPtr awsIotCoreOperations_;
    /* @Autowired */
    Private ILoggerPtr logger;
    Private std::atomic<bool> operationInProgress_{false};
    Private std::atomic<bool> dirty_{false};

    Private Bool IsDonePayload(CStdString payload) {
        if (payload.empty()) return false;
        StdString s(payload);
        if (s == "done") return true;
        JsonDocument doc;
        if (deserializeJson(doc, payload.c_str(), payload.length())) return false;
        if (doc["done"].is<bool>() && doc["done"].as<bool>()) return true;
        return false;
    }

    Private StdString ParseCommandPayload(CStdString payload) {
        if (payload.empty()) return {};
        JsonDocument doc;
        if (deserializeJson(doc, payload.c_str(), payload.length())) {
            return StdString(payload);
        }
        if (doc.containsKey("key") && doc.containsKey("value")) {
            const char* k = doc["key"].as<const char*>();
            const char* v = doc["value"].as<const char*>();
            if (k && v) return StdString(k) + ":" + StdString(v);
        }
        return StdString(payload);
    }
};

#endif /* CLOUDOPERATIONS_H */
