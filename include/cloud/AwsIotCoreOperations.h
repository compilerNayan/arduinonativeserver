#ifndef AWSIOTCOREOPERATIONS_H
#define AWSIOTCOREOPERATIONS_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
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
    Private SemaphoreHandle_t mqttMutex = nullptr;

    Private Static AwsIotCoreOperations* activeInstance;

    Private class MqttLockGuard {
        Private SemaphoreHandle_t mutex_;
        Private Bool locked_;

        Public Explicit MqttLockGuard(SemaphoreHandle_t mutex)
            : mutex_(mutex), locked_(false) {
            if (mutex_ != nullptr) {
                locked_ = (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10000)) == pdTRUE);
            }
        }

        Public ~MqttLockGuard() {
            if (locked_ && mutex_ != nullptr) {
                xSemaphoreGive(mutex_);
            }
        }

        Public Bool IsLocked() const { return locked_; }
    };

    Private Static Void StaticMqttCallback(Char* topic, UInt8* payload, UInt length) {
        if (activeInstance != nullptr) {
            activeInstance->OnMqttMessage(topic, payload, length);
        }
    }

    Private Bool HasEnoughTlsHeadroom(const Char* context) {
        // Keep a guard, but tune for this firmware's steady-state heap profile.
        constexpr UInt kMinFreeHeap = 72 * 1024;
        constexpr UInt kMinLargestBlock = 40 * 1024;
        UInt freeHeap = ESP.getFreeHeap();
        UInt largestBlock = ESP.getMaxAllocHeap();
        if (freeHeap < kMinFreeHeap || largestBlock < kMinLargestBlock) {
            Serial.print("[AwsIotCoreOperations] ");
            Serial.print(context);
            Serial.print(" skipped: low memory freeHeap=");
            Serial.print(freeHeap);
            Serial.print(" largestFreeBlock=");
            Serial.println(largestBlock);
            return false;
        }
        return true;
    }

    Private Void PrintRuntimeStats(const Char* label) {
        (void)label;
    }

    Private Void PrintMqttState(const Char* label) {
        (void)label;
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
        ////Serial.print("[AwsIotCoreOperations] Callback topic: ");
        ////Serial.print(topicName.c_str());
        ////Serial.print(" payload: ");
        ////Serial.println(message.c_str());
        bufferedMessages[topicName].push_back(message);
    }

    Private Bool EnsureConfigured() {
        if (configured) {
            return true;
        }
        if (configProvider == nullptr) {
            ////Serial.println("[AwsIotCoreOperations] EnsureConfigured failed: configProvider is null");
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
        // Publish payloads can exceed 1 KB (e.g. batched logs), so keep MQTT packet buffer larger.
        mqttClient.setBufferSize(4096);
        PrintRuntimeStats("EnsureConfigured configured");
        PrintMqttState("EnsureConfigured state");

        activeInstance = this;
        configured = true;
        //Serial.print("[AwsIotCoreOperations] Configured endpoint=");
        //Serial.print(endpoint.c_str());
        //Serial.print(" thingName=");
        //Serial.print(thingName.c_str());
        //Serial.print(" publishTopic=");
        //Serial.print(publishTopic.c_str());
        //Serial.print(" subscribeTopic=");
        //Serial.println(subscribeTopic.c_str());
        return true;
    }

    Private Bool EnsureMqttConnected() {
//        Serial.println("[AwsIotCoreOperations] EnsureMqttConnected called");
        MqttLockGuard lock(mqttMutex);
        if (!lock.IsLocked()) {
            Serial.println("[AwsIotCoreOperations] EnsureMqttConnected lock timeout");
            return false;
        }
        PrintRuntimeStats("EnsureMqttConnected enter");
        if (!EnsureConfigured()) {
            Serial.println("[AwsIotCoreOperations] EnsureMqttConnected not configured");
            return false;
        }
        PrintMqttState("EnsureMqttConnected after configure");
        if (WiFi.status() != WL_CONNECTED) {
      //      Serial.println("[AwsIotCoreOperations] EnsureMqttConnected: WiFi not connected");
      //      Serial.print("[AwsIotCoreOperations] EnsureMqttConnected WiFi.status=");
        //    Serial.println(static_cast<Int>(WiFi.status()));
            wasConnected = false;
            return false;
        }
        //Serial.print("[AwsIotCoreOperations] EnsureMqttConnected WiFi RSSI=");
        //Serial.println(WiFi.RSSI());
        if (mqttClient.connected()) {
          //  Serial.println("[AwsIotCoreOperations] EnsureMqttConnected mqtt connected");
            PrintMqttState("EnsureMqttConnected already connected");
            wasConnected = true;
            return true;
        }
        //Serial.println("[AwsIotCoreOperations] EnsureMqttConnected mqtt not connected");
        PrintRuntimeStats("EnsureMqttConnected before mqttClient.connect");
        //Serial.print("[AwsIotCoreOperations] EnsureMqttConnected endpoint=");
        //Serial.print(endpoint.c_str());
        //Serial.print(" port=8883 thingNameLen=");
        //Serial.println(static_cast<UInt>(thingName.length()));
        //Serial.print("[AwsIotCoreOperations] EnsureMqttConnected DNS0=");
        //Serial.print(WiFi.dnsIP(0));
        //Serial.print(" DNS1=");
        //Serial.println(WiFi.dnsIP(1));
        IPAddress resolvedIp;
        if (!WiFi.hostByName(endpoint.c_str(), resolvedIp)) {
            Serial.println("[AwsIotCoreOperations] EnsureMqttConnected DNS precheck failed");
            PrintMqttState("EnsureMqttConnected DNS precheck fail state");
            wasConnected = false;
            return false;
        }
        //Serial.print("[AwsIotCoreOperations] EnsureMqttConnected endpoint resolved to=");
        //Serial.println(resolvedIp);
        //Serial.println("[AwsIotCoreOperations] EnsureMqttConnected calling mqttClient.connect NOW");
        if (!HasEnoughTlsHeadroom("EnsureMqttConnected connect")) {
            wasConnected = false;
            return false;
        }
        Bool connected = mqttClient.connect(thingName.c_str());
        //Serial.println("[AwsIotCoreOperations] EnsureMqttConnected mqtt connect result=");
        PrintRuntimeStats("EnsureMqttConnected after mqttClient.connect");
        PrintMqttState("EnsureMqttConnected post connect");
        if (connected) {
            //Serial.println("[AwsIotCoreOperations] EnsureMqttConnected mqtt connect success");
            // MQTT session is new; previous subscriptions are no longer guaranteed.
            if (wasConnected == false) {
                //Serial.println("[AwsIotCoreOperations] EnsureMqttConnected subscribed topics cleared");
                subscribedTopics.clear();
            }
            wasConnected = true;
            //Serial.println("[AwsIotCoreOperations] EnsureMqttConnected subscribed topics cleared");

            // Immediately subscribe to default topic on successful connect.
            if (!subscribeTopic.empty()) {
                //Serial.println("[AwsIotCoreOperations] EnsureMqttConnected subscribing to topic=");
                //Serial.println(subscribeTopic.c_str());
                PrintRuntimeStats("EnsureMqttConnected before default subscribe");
                if (mqttClient.subscribe(subscribeTopic.c_str())) {
                    //Serial.println("[AwsIotCoreOperations] EnsureMqttConnected subscribed to topic=");
                    //Serial.println(subscribeTopic.c_str());
                    subscribedTopics.insert(subscribeTopic);
                    //Serial.print("[AwsIotCoreOperations] Connected + subscribed default topic: ");
                    //Serial.println(subscribeTopic.c_str());
                } else {
                    Serial.println("[AwsIotCoreOperations] EnsureMqttConnected subscribed to topic failed");
                    PrintMqttState("EnsureMqttConnected default subscribe failed");
                    //Serial.print("[AwsIotCoreOperations] Connected but default subscribe failed: ");
                    //Serial.println(subscribeTopic.c_str());
                }
            }
        } else {
            Serial.println("[AwsIotCoreOperations] EnsureMqttConnected mqtt connect failed");
            secureClient.stop();
            mqttClient.disconnect();
            delay(750);
            //Serial.print("[AwsIotCoreOperations] MQTT connect failed, state: ");
            //Serial.println(mqttClient.state());
            PrintMqttState("EnsureMqttConnected connect failed");
            wasConnected = false;
            //Serial.println("[AwsIotCoreOperations] EnsureMqttConnected mqtt connect failed");
        }
        //Serial.println("[AwsIotCoreOperations] EnsureMqttConnected returning connected=");
        //Serial.println(connected ? "true" : "false");
        return connected;
    }

    Private Bool EnsureSubscribed(CStdString topicName) {
        if (!EnsureMqttConnected()) {
            return false;
        }
        MqttLockGuard lock(mqttMutex);
        if (!lock.IsLocked()) {
            Serial.println("[AwsIotCoreOperations] EnsureSubscribed lock timeout");
            return false;
        }
        if (subscribedTopics.find(topicName) != subscribedTopics.end()) {
            return true;
        }
        if (mqttClient.subscribe(topicName.c_str())) {
            //Serial.print("[AwsIotCoreOperations] Subscribed to topic: ");
            //Serial.println(topicName.c_str());
            subscribedTopics.insert(topicName);
            return true;
        }
        //Serial.print("[AwsIotCoreOperations] Subscribe failed for topic: ");
        //Serial.println(topicName.c_str());
        return false;
    }

    Public AwsIotCoreOperations() : mqttClient(secureClient) {
        mqttMutex = xSemaphoreCreateMutex();
    }

    Public Virtual ~AwsIotCoreOperations() override {
        if (mqttMutex != nullptr) {
            vSemaphoreDelete(mqttMutex);
            mqttMutex = nullptr;
        }
    }

    Public Virtual Bool SendMessage(CStdString message) override {
        if (!EnsureConfigured()) {
            Serial.println("[AwsIotCoreOperations] SendMessage failed: not configured");
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
        PrintRuntimeStats("SendMessage enter");
        //Serial.print(" payload=");
        //Serial.println(message.c_str());
        if (!EnsureMqttConnected()) {
            Serial.println("[AwsIotCoreOperations] SendMessage failed: MQTT not connected");
            PrintMqttState("SendMessage not connected");
            return false;
        }
        if (!HasEnoughTlsHeadroom("SendMessage publish")) {
            return false;
        }
        MqttLockGuard lock(mqttMutex);
        if (!lock.IsLocked()) {
            Serial.println("[AwsIotCoreOperations] SendMessage lock timeout");
            return false;
        }
        PrintRuntimeStats("SendMessage before mqttClient.loop");
        mqttClient.loop();
        PrintRuntimeStats("SendMessage before publish");
        Bool ok = mqttClient.publish(topicName.c_str(), message.c_str());
        if (!ok) {
            Serial.println("[AwsIotCoreOperations] mqttClient.publish FAILED");
        }
        PrintMqttState("SendMessage publish result");
        PrintRuntimeStats("SendMessage exit");
        return ok;
    }

    Public Virtual StdVector<StdString> ReceiveMessages(CStdString topicName) override {
        StdVector<StdString> result;
        //Serial.print("[AwsIotCoreOperations] Receive poll for topic: ");
        //Serial.println(topicName.c_str());
        if (!EnsureSubscribed(topicName)) {
            //Serial.println("[AwsIotCoreOperations] Receive poll skipped (not subscribed/connected)");
            return result;
        }
        MqttLockGuard lock(mqttMutex);
        if (!lock.IsLocked()) {
            Serial.println("[AwsIotCoreOperations] ReceiveMessages lock timeout");
            return result;
        }
        mqttClient.loop();

        auto it = bufferedMessages.find(topicName);
        if (it == bufferedMessages.end()) {
            //Serial.println("[AwsIotCoreOperations] Receive poll: no messages buffered");
            return result;
        }

        result = it->second;
        bufferedMessages.erase(it);
        //Serial.print("[AwsIotCoreOperations] Receive poll: returning messages count = ");
        //Serial.println(static_cast<Int>(result.size()));
        return result;
    }
};

AwsIotCoreOperations* AwsIotCoreOperations::activeInstance = nullptr;

#endif /* AWSIOTCOREOPERATIONS_H */
