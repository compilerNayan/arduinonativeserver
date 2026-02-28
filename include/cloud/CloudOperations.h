#ifndef CLOUDOPERATIONS_H
#define CLOUDOPERATIONS_H

#include "ICloudOperations.h"
#include "IAwsCloudConfigProvider.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ILogger.h>
#include <atomic>

class CloudOperations : public ICloudOperations {
    Public CloudOperations() : client_(net_) {}

    Public StdVector<StdString> RetrieveCommands() override {
        if (dirty_.load(std::memory_order_relaxed)) {
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] RetrieveCommands skip: dirty"));
            return {};
        }
        if (operationInProgress_.exchange(true)) {
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] RetrieveCommands skip: operation already in progress"));
            return {};
        }
        struct Guard { std::atomic<bool>& f; ~Guard() { f.store(false); } } g{operationInProgress_};
        if (!EnsureConnected()) {
            if (logger) logger->Error(Tag::Untagged, StdString("[CloudOperations] RetrieveCommands: EnsureConnected failed"));
            return {};
        }
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] RetrieveCommands: waiting for commands on ") + GetCmdTopic());
        pendingCommands_.clear();
        commandsDone_.store(false);
        const unsigned long timeoutMs = 15000;
        unsigned long start = millis();
        while (!commandsDone_.load(std::memory_order_relaxed) && (millis() - start < timeoutMs)) {
            client_.loop();
            delay(50);
        }
        StdVector<StdString> out(pendingCommands_.begin(), pendingCommands_.end());
        pendingCommands_.clear();
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] RetrieveCommands: got ") + std::to_string(out.size()) + " command(s)");
        return out;
    }

    Public Bool PublishLogs(const StdMap<ULongLong, StdString>& logs) override {
        if (dirty_.load(std::memory_order_relaxed)) {
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] PublishLogs skip: dirty"));
            return false;
        }
        if (operationInProgress_.exchange(true)) {
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] PublishLogs skip: operation already in progress"));
            return false;
        }
        struct Guard { std::atomic<bool>& f; ~Guard() { f.store(false); } } g{operationInProgress_};
        if (!EnsureConnected()) {
            if (logger) logger->Error(Tag::Untagged, StdString("[CloudOperations] PublishLogs: EnsureConnected failed"));
            return false;
        }
        if (logs.empty()) return true;
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] PublishLogs: publishing ") + std::to_string(logs.size()) + " log(s) to " + GetLogsTopic());
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        for (const auto& p : logs) {
            root[std::to_string(p.first).c_str()] = p.second.c_str();
        }
        char buf[1024];
        size_t n = serializeJson(doc, buf, sizeof(buf));
        if (n >= sizeof(buf)) {
            if (logger) logger->Error(Tag::Untagged, StdString("[CloudOperations] PublishLogs: serialized payload too large"));
            return false;
        }
        Bool ok = client_.publish(GetLogsTopic().c_str(), buf);
        client_.loop();
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
    Private IAwsCloudConfigProviderPtr configProvider_;
    /* @Autowired */
    Private ILoggerPtr logger;
    Private WiFiClientSecure net_;
    Private PubSubClient client_;
    Private std::atomic<bool> connected_{false};
    Private std::atomic<bool> operationInProgress_{false};
    Private std::atomic<bool> dirty_{false};

    Private StdVector<StdString> pendingCommands_;
    Private std::atomic<bool> commandsDone_{false};

    Private static CloudOperations* s_instance;

    Private static void MqttCallback(char* topic, byte* payload, unsigned int length) {
        if (s_instance) s_instance->OnMqttMessage(topic, payload, length);
    }

    Private void OnMqttMessage(char* topic, byte* payload, unsigned int length) {
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] MQTT msg topic=") + topic + " len=" + std::to_string(length));
        if (IsDonePayload(reinterpret_cast<const char*>(payload), length)) {
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] MQTT done payload received"));
            commandsDone_.store(true);
            return;
        }
        StdString cmd = ParseCommandPayload(reinterpret_cast<const char*>(payload), length);
        if (!cmd.empty()) {
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] MQTT command: ") + cmd);
            pendingCommands_.push_back(cmd);
        }
    }

    Private Bool IsDonePayload(const char* payload, unsigned int length) {
        if (length == 0) return false;
        StdString s(payload, length);
        if (s == "done") return true;
        JsonDocument doc;
        if (deserializeJson(doc, payload, length)) return false;
        if (doc["done"].is<bool>() && doc["done"].as<bool>()) return true;
        return false;
    }

    Private StdString ParseCommandPayload(const char* payload, unsigned int length) {
        if (length == 0) return {};
        JsonDocument doc;
        if (deserializeJson(doc, payload, length)) {
            return StdString(payload, length);
        }
        if (doc.containsKey("key") && doc.containsKey("value")) {
            const char* k = doc["key"].as<const char*>();
            const char* v = doc["value"].as<const char*>();
            if (k && v) return StdString(k) + ":" + StdString(v);
        }
        return StdString(payload, length);
    }

    Private StdString GetCmdTopic() const {
        return configProvider_->GetDeviceSerial() + "/cmd";
    }

    Private StdString GetLogsTopic() const {
        return configProvider_->GetDeviceSerial() + "/logs";
    }

    Private Bool EnsureConnected() {
        if (dirty_.load(std::memory_order_relaxed)) return false;
        if (connected_.load(std::memory_order_relaxed) && client_.connected()) return true;
        if (!connected_.load(std::memory_order_relaxed)) {
            client_.setBufferSize(1024);
            client_.setSocketTimeout(10);
            client_.setServer(configProvider_->GetEndpoint().c_str(), 8883);
            client_.setCallback(MqttCallback);
        }
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] Connecting to ") + configProvider_->GetEndpoint() + " as " + configProvider_->GetThingName());
        net_.setCACert(configProvider_->GetCaCert().c_str());
        net_.setCertificate(configProvider_->GetDeviceCert().c_str());
        net_.setPrivateKey(configProvider_->GetPrivateKey().c_str());
        s_instance = this;
        const unsigned long timeoutMs = 20000;
        unsigned long start = millis();
        while (!client_.connect(configProvider_->GetThingName().c_str())) {
            if (millis() - start > timeoutMs) {
                if (logger) logger->Error(Tag::Untagged, StdString("[CloudOperations] MQTT connect timeout"));
                dirty_.store(true);
                s_instance = nullptr;
                return false;
            }
            delay(500);
        }
        StdString cmdTopic = GetCmdTopic();
        client_.subscribe(cmdTopic.c_str());
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudOperations] MQTT connected, subscribed to ") + cmdTopic);
        connected_.store(true);
        return true;
    }
};

CloudOperations* CloudOperations::s_instance = nullptr;

#endif /* CLOUDOPERATIONS_H */
