#ifdef ARDUINO
#ifndef CLOUDFACADE_H
#define CLOUDFACADE_H

#include "ICloudFacade.h"
#include "ICloudOperations.h"
#include "CloudOperations.h"
#include <ILogger.h>
#include <IInternetConnectionStatusProvider.h>
#include <Arduino.h>

#include <queue>
#include <mutex>

/* @Component */
/* @Scope("PROTOTYPE") */
class CloudFacade : public ICloudFacade {

    Private ICloudOperationsPtr cloudOperations_;
    Private mutable std::mutex cloudOperationsMutex_;

    /* @Autowired */
    Private ILoggerPtr logger;

    /* @Autowired */
    Private IInternetConnectionStatusProviderPtr internetConnectionStatusProvider_;

    Private std::queue<StdString> requestQueue_;
    Private std::mutex requestQueueMutex_;

    Private Bool TryDequeue(StdString& out) {
        std::lock_guard<std::mutex> lock(requestQueueMutex_);
        if (requestQueue_.empty()) return false;
        out = requestQueue_.front();
        requestQueue_.pop();
        return true;
    }

    Private Void EnqueueAll(const StdVector<StdString>& commands) {
        std::lock_guard<std::mutex> lock(requestQueueMutex_);
        for (const StdString& s : commands) {
            requestQueue_.push(s);
        }
    }

    Public CloudFacade() {
        ResetCloudOperations();
    }

    Public Virtual ~CloudFacade() override = default;

    /** Thread-safe: replaces cloudOperations_ with a new CloudOperations instance and clears command queue. */
    Public Void ResetCloudOperations() override {
        //Serial.println("[CloudFacade] ResetCloudOperations()");
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] Resetting cloud operations."));
        std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
        cloudOperations_ = std::make_shared<CloudOperations>();
        {
            std::lock_guard<std::mutex> q(requestQueueMutex_);
            while (!requestQueue_.empty()) requestQueue_.pop();
        }
    }

    Public Void StopCloudOperations() override {
        //Serial.println("[CloudFacade] StopCloudOperations()");
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] Stopping cloud operations."));
        std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
        cloudOperations_ = nullptr;
    }

    Public Void StartCloudOperations() override {
        //Serial.println("[CloudFacade] StartCloudOperations()");
        if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] Starting cloud operations."));
        std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
        cloudOperations_ = std::make_shared<CloudOperations>();
    }

    Public Bool IsDirty() const override {
        std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
        return cloudOperations_ ? cloudOperations_->IsDirty() : false;
    }

    Public Bool PublishLogs(const StdMap<ULongLong, StdString>& logs) override {
        Serial.print("[CloudFacade] PublishLogs() count=");
        Serial.println(static_cast<Int>(logs.size()));
        /*if (internetConnectionStatusProvider_ && !internetConnectionStatusProvider_->IsInternetConnected()) {
            //Serial.println("[CloudFacade] PublishLogs skip: network not connected");
            //if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] PublishLogs skip: network not connected"));
            return false;
        } */
        ICloudOperationsPtr ops;
        {
            std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
            ops = cloudOperations_;
        }
        if (!ops) {
            //Serial.println("[CloudFacade] PublishLogs skip: no cloud operations");
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] PublishLogs skip: no cloud operations"));
            return false;
        }
        if (ops->IsDirty()) {
            //Serial.println("[CloudFacade] PublishLogs skip: operations dirty");
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] PublishLogs skip: operations dirty"));
            return false;
        }
        if (ops->IsOperationInProgress()) {
            //Serial.println("[CloudFacade] PublishLogs skip: operation already in progress");
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] PublishLogs skip: operation already in progress"));
            return false;
        }
        Bool ok = ops->PublishLogs(logs);
        //Serial.print("[CloudFacade] PublishLogs result -> ");
        //Serial.println(ok ? "OK" : "FAILED");
        if(!ok) {
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] PublishLogs ") + (ok ? "ok" : "failed"));
        }
        return ok;
    }

    Public StdString GetCommand() override {
        //Serial.println("[CloudFacade] GetCommand() called");
        StdString out;
        if (TryDequeue(out)) {
            //Serial.print("[CloudFacade] GetCommand() from queue -> ");
            //Serial.println(out.c_str());
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] GetCommand: from queue: ") + out);
            return out;
        }
        if (internetConnectionStatusProvider_ && !internetConnectionStatusProvider_->IsInternetConnected()) {
            //Serial.println("[CloudFacade] GetCommand skip: network not connected");
            //if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] GetCommand skip: network not connected"));
            return StdString();
        }
        ICloudOperationsPtr ops;
        {
            std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
            ops = cloudOperations_;
        }
        if (!ops) {
            //Serial.println("[CloudFacade] GetCommand skip: no cloud operations");
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] GetCommand skip: no cloud operations"));
            return StdString();
        }
        if (ops->IsDirty()) {
            //Serial.println("[CloudFacade] GetCommand skip: operations dirty");
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] GetCommand skip: operations dirty"));
            return StdString();
        }
        if (ops->IsOperationInProgress()) {
            //Serial.println("[CloudFacade] GetCommand skip: operation already in progress");
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] GetCommand skip: operation already in progress"));
            return StdString();
        }
        StdVector<StdString> commands = ops->RetrieveCommands();
        //Serial.print("[CloudFacade] RetrieveCommands count=");
        //Serial.println(static_cast<Int>(commands.size()));
        //if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] GetCommand: RetrieveCommands returned ") + std::to_string(commands.size()) + " command(s)");
        EnqueueAll(commands);
        if (TryDequeue(out)) {
            //Serial.print("[CloudFacade] GetCommand returning -> ");
            //Serial.println(out.c_str());
            if (logger) logger->Info(Tag::Untagged, StdString("[CloudFacade] GetCommand: returning ") + out);
            return out;
        }
        //Serial.println("[CloudFacade] GetCommand returning empty");
        return StdString();
    }
};

#endif // CLOUDFACADE_H
#endif // ARDUINO
