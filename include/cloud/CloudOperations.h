#ifndef CLOUDOPERATIONS_H
#define CLOUDOPERATIONS_H

#include "ICloudOperations.h"
#include "IAwsCloudConfigProvider.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <atomic>

/* @Component */
class CloudOperations : public ICloudOperations {
    Public CloudOperations() : client_(net_) {}

    Public StdVector<StdString> RetrieveCommands() override {
        if (dirty_.load(std::memory_order_relaxed)) return {};
        if (operationInProgress_.exchange(true)) return {};
        struct Guard { std::atomic<bool>& f; ~Guard() { f.store(false); } } g{operationInProgress_};
        if (!EnsureConnected()) return {};
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
        return out;
    }

    Public Bool PublishLogs(const StdMap<ULongLong, StdString>& logs) override {
        if (dirty_.load(std::memory_order_relaxed)) return false;
        if (operationInProgress_.exchange(true)) return false;
        struct Guard { std::atomic<bool>& f; ~Guard() { f.store(false); } } g{operationInProgress_};
        if (!EnsureConnected()) return false;
        if (logs.empty()) return true;
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        for (const auto& p : logs) {
            root[std::to_string(p.first).c_str()] = p.second.c_str();
        }
        char buf[1024];
        size_t n = serializeJson(doc, buf, sizeof(buf));
        if (n >= sizeof(buf)) return false;
        Bool ok = client_.publish(GetLogsTopic().c_str(), buf);
        client_.loop();
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
        (void)topic;
        if (IsDonePayload(reinterpret_cast<const char*>(payload), length)) {
            commandsDone_.store(true);
            return;
        }
        StdString cmd = ParseCommandPayload(reinterpret_cast<const char*>(payload), length);
        if (!cmd.empty()) pendingCommands_.push_back(cmd);
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
        net_.setCACert(configProvider_->GetCaCert().c_str());
        net_.setCertificate(configProvider_->GetDeviceCert().c_str());
        net_.setPrivateKey(configProvider_->GetPrivateKey().c_str());
        s_instance = this;
        const unsigned long timeoutMs = 20000;
        unsigned long start = millis();
        while (!client_.connect(configProvider_->GetThingName().c_str())) {
            if (millis() - start > timeoutMs) {
                dirty_.store(true);
                s_instance = nullptr;
                return false;
            }
            delay(500);
        }
        client_.subscribe(GetCmdTopic().c_str());
        connected_.store(true);
        return true;
    }
};

CloudOperations* CloudOperations::s_instance = nullptr;

#endif /* CLOUDOPERATIONS_H */
