#ifndef AWSIOTCOREOPERATIONS_H
#define AWSIOTCOREOPERATIONS_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "IAwsIotCoreOperations.h"
#include "IAwsIotCoreConfigProvider.h"

/* @Component */
class AwsIotCoreOperations : public IAwsIotCoreOperations {
    /* @Autowired */
    Private IAwsIotCoreConfigProviderPtr configProvider;

    Private WiFiClientSecure secureClient;
    Private PubSubClient mqttClient;

    Private StdString endpoint;
    Private StdString thingName;
    Private StdString caCert;
    Private StdString deviceCert;
    Private StdString privateKey;
    Private StdString publishTopic;
    Private StdString subscribeTopic;

    Private Bool configured = false;
    Private Bool wasConnected = false;
    Private StdMap<StdString, StdVector<StdString>> bufferedMessages;
    Private StdUnorderedSet<StdString> subscribedTopics;

    Private Static AwsIotCoreOperations* activeInstance;

    Private Static Void StaticMqttCallback(Char* topic, UInt8* payload, UInt length) {
        if (activeInstance != nullptr) {
            activeInstance->OnMqttMessage(topic, payload, length);
        }
    }

    Private Void OnMqttMessage(Char* topic, UInt8* payload, UInt length) {
        if (topic == nullptr) {
            return;
        }

        StdString topicName(topic);
        StdString message;
        message.reserve(length);
        for (UInt i = 0; i < length; ++i) {
            message += static_cast<Char>(payload[i]);
        }
        Serial.print("[AwsIotCoreOperations] Callback topic: ");
        Serial.print(topicName.c_str());
        Serial.print(" payload: ");
        Serial.println(message.c_str());
        bufferedMessages[topicName].push_back(message);
    }

    Private Bool EnsureConfigured() {
        if (configured) {
            return true;
        }
        if (configProvider == nullptr) {
            return false;
        }

        endpoint = configProvider->GetEndpoint();
        thingName = configProvider->GetThingName();
        caCert = configProvider->GetCaCert();
        deviceCert = configProvider->GetDeviceCert();
        privateKey = configProvider->GetPrivateKey();
        publishTopic = configProvider->GetPublishTopic();
        subscribeTopic = configProvider->GetSubscribeTopic();

        secureClient.setCACert(caCert.c_str());
        secureClient.setCertificate(deviceCert.c_str());
        secureClient.setPrivateKey(privateKey.c_str());

        mqttClient.setServer(endpoint.c_str(), 8883);
        mqttClient.setCallback(StaticMqttCallback);
        mqttClient.setBufferSize(1024);

        activeInstance = this;
        configured = true;
        return true;
    }

    Private Bool EnsureMqttConnected() {
        if (!EnsureConfigured()) {
            return false;
        }
        if (WiFi.status() != WL_CONNECTED) {
            wasConnected = false;
            return false;
        }
        if (mqttClient.connected()) {
            wasConnected = true;
            return true;
        }
        Bool connected = mqttClient.connect(thingName.c_str());
        if (connected) {
            // MQTT session is new; previous subscriptions are no longer guaranteed.
            if (wasConnected == false) {
                subscribedTopics.clear();
            }
            wasConnected = true;

            // Immediately subscribe to default topic on successful connect.
            if (!subscribeTopic.empty()) {
                if (mqttClient.subscribe(subscribeTopic.c_str())) {
                    subscribedTopics.insert(subscribeTopic);
                    Serial.print("[AwsIotCoreOperations] Connected + subscribed default topic: ");
                    Serial.println(subscribeTopic.c_str());
                } else {
                    Serial.print("[AwsIotCoreOperations] Connected but default subscribe failed: ");
                    Serial.println(subscribeTopic.c_str());
                }
            }
        } else {
            Serial.print("[AwsIotCoreOperations] MQTT connect failed, state: ");
            Serial.println(mqttClient.state());
            wasConnected = false;
        }
        return connected;
    }

    Private Bool EnsureSubscribed(CStdString topicName) {
        if (!EnsureMqttConnected()) {
            return false;
        }
        if (subscribedTopics.find(topicName) != subscribedTopics.end()) {
            return true;
        }
        if (mqttClient.subscribe(topicName.c_str())) {
            Serial.print("[AwsIotCoreOperations] Subscribed to topic: ");
            Serial.println(topicName.c_str());
            subscribedTopics.insert(topicName);
            return true;
        }
        Serial.print("[AwsIotCoreOperations] Subscribe failed for topic: ");
        Serial.println(topicName.c_str());
        return false;
    }

    Public AwsIotCoreOperations() : mqttClient(secureClient) {}
    Public Virtual ~AwsIotCoreOperations() override = default;

    Public Virtual Bool SendMessage(CStdString message) override {
        if (!EnsureConfigured()) {
            return false;
        }
        return SendMessage(message, publishTopic);
    }

    Public Virtual StdVector<StdString> ReceiveMessages() override {
        if (!EnsureConfigured()) {
            return StdVector<StdString>();
        }
        return ReceiveMessages(subscribeTopic);
    }

    Public Virtual Bool SendMessage(CStdString message, CStdString topicName) override {
        if (!EnsureMqttConnected()) {
            return false;
        }
        mqttClient.loop();
        return mqttClient.publish(topicName.c_str(), message.c_str());
    }

    Public Virtual StdVector<StdString> ReceiveMessages(CStdString topicName) override {
        StdVector<StdString> result;
        Serial.print("[AwsIotCoreOperations] Receive poll for topic: ");
        Serial.println(topicName.c_str());
        if (!EnsureSubscribed(topicName)) {
            Serial.println("[AwsIotCoreOperations] Receive poll skipped (not subscribed/connected)");
            return result;
        }

        mqttClient.loop();

        auto it = bufferedMessages.find(topicName);
        if (it == bufferedMessages.end()) {
            Serial.println("[AwsIotCoreOperations] Receive poll: no messages buffered");
            return result;
        }

        result = it->second;
        bufferedMessages.erase(it);
        Serial.print("[AwsIotCoreOperations] Receive poll: returning messages count = ");
        Serial.println(static_cast<Int>(result.size()));
        return result;
    }
};

AwsIotCoreOperations* AwsIotCoreOperations::activeInstance = nullptr;

#endif /* AWSIOTCOREOPERATIONS_H */
