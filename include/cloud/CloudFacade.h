#ifdef ARDUINO
#ifndef CLOUDFACADE_H
#define CLOUDFACADE_H

#include "ICloudFacade.h"
#include "ICloudOperations.h"
#include "CloudOperations.h"
#include <ILogger.h>
#include <INetworkStatusProvider.h>
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
    Private INetworkStatusProviderPtr networkStatusProvider_;

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
        logger->Info(Tag::Untagged, StdString("[CloudFacade] Resetting cloud operations."));
        std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
        cloudOperations_ = std::make_shared<CloudOperations>();
        {
            std::lock_guard<std::mutex> q(requestQueueMutex_);
            while (!requestQueue_.empty()) requestQueue_.pop();
        }
    }

    Public Void StopCloudOperations() override {
        logger->Info(Tag::Untagged, StdString("[CloudFacade] Stopping cloud operations."));
        std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
        cloudOperations_ = nullptr;
    }

    Public Void StartCloudOperations() override {
        logger->Info(Tag::Untagged, StdString("[CloudFacade] Starting cloud operations."));
        std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
        cloudOperations_ = std::make_shared<CloudOperations>();
    }

    Public Bool IsDirty() const override {
        std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
        return cloudOperations_ ? cloudOperations_->IsDirty() : false;
    }

    Public Bool PublishLogs(const StdMap<ULongLong, StdString>& logs) override {
        if (networkStatusProvider_ && !networkStatusProvider_->IsNetworkConnected()) return false;
        ICloudOperationsPtr ops;
        {
            std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
            ops = cloudOperations_;
        }
        if (!ops) return false;
        if (ops->IsDirty()) return false;
        if (ops->IsOperationInProgress()) return false;
        return ops->PublishLogs(logs);
    }

    Public StdString GetCommand() override {
        StdString out;
        if (TryDequeue(out)) return out;
        if (networkStatusProvider_ && !networkStatusProvider_->IsNetworkConnected()) return StdString();
        ICloudOperationsPtr ops;
        {
            std::lock_guard<std::mutex> lock(cloudOperationsMutex_);
            ops = cloudOperations_;
        }
        if (!ops) return StdString();
        if (ops->IsDirty()) return StdString();
        if (ops->IsOperationInProgress()) return StdString();
        StdVector<StdString> commands = ops->RetrieveCommands();
        EnqueueAll(commands);
        if (TryDequeue(out)) return out;
        return StdString();
    }
};

#endif // CLOUDFACADE_H
#endif // ARDUINO
