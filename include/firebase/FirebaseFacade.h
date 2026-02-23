#ifdef ARDUINO
#ifndef FIREBASEFACADE_H
#define FIREBASEFACADE_H

#include "IFirebaseFacade.h"
#include "IFirebaseOperations.h"
#include "FirebaseOperations.h"
#include <ILogger.h>
#include <INetworkStatusProvider.h>
#include <Arduino.h>

#include <queue>
#include <mutex>

/* @Component */
/* @Scope("PROTOTYPE") */
class FirebaseFacade : public IFirebaseFacade {

    Private IFirebaseOperationsPtr firebaseOperations;
    Private mutable std::mutex firebaseOperationsMutex_;

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

    Public FirebaseFacade() {
        ResetFirebaseOperations();
    }

    Public Virtual ~FirebaseFacade() override = default;

    /** Thread-safe: replaces firebaseOperations with a new FirebaseOperations instance. Proceeds only if internet is available. */
    Public Void ResetFirebaseOperations() override {
        logger->Info(Tag::Untagged, StdString("[FirebaseFacade] Resetting Firebase operations."));
        std::lock_guard<std::mutex> lock(firebaseOperationsMutex_);
        firebaseOperations = std::make_shared<FirebaseOperations>();
    }

    Public Void StopFirebaseOperations() override {
        logger->Info(Tag::Untagged, StdString("[FirebaseFacade] Stopping Firebase operations."));
        std::lock_guard<std::mutex> lock(firebaseOperationsMutex_);
        firebaseOperations = nullptr;
    }

    Public Void StartFirebaseOperations() override {
        logger->Info(Tag::Untagged, StdString("[FirebaseFacade] Starting Firebase operations."));
        std::lock_guard<std::mutex> lock(firebaseOperationsMutex_);
        firebaseOperations = std::make_shared<FirebaseOperations>();
    }

    Public Bool IsDirty() const override {
        std::lock_guard<std::mutex> lock(firebaseOperationsMutex_);
        return firebaseOperations ? firebaseOperations->IsDirty() : false;
    }

    Public FirebaseOperationResult PublishLogs(const StdMap<ULongLong, StdString>& logs) override {
        IFirebaseOperationsPtr ops;
        {
            std::lock_guard<std::mutex> lock(firebaseOperationsMutex_);
            ops = firebaseOperations;
        }
        if (!ops) return FirebaseOperationResult::NotReady;
        if (ops->IsDirty()) return FirebaseOperationResult::NotReady;
        if (ops->IsOperationInProgress()) return FirebaseOperationResult::AnotherOperationInProgress;
        return ops->PublishLogs(logs);
    }

    Public FirebaseOperationResult GetCommand(StdString& out) override {
        out.clear();
        if (TryDequeue(out)) {
            return FirebaseOperationResult::OperationSucceeded;
        }
        IFirebaseOperationsPtr ops;
        {
            std::lock_guard<std::mutex> lock(firebaseOperationsMutex_);
            ops = firebaseOperations;
        }
        if (!ops) return FirebaseOperationResult::NotReady;
        if (ops->IsDirty()) {
            if (ops->IsOperationInProgress()) return FirebaseOperationResult::AnotherOperationInProgress;
            return FirebaseOperationResult::NotReady;
        }
        StdVector<StdString> commands;
        FirebaseOperationResult res = ops->RetrieveCommands(commands);
        if (res != FirebaseOperationResult::OperationSucceeded) return res;
        EnqueueAll(commands);
        if (TryDequeue(out)) {
            return FirebaseOperationResult::OperationSucceeded;
        }
        return FirebaseOperationResult::OperationSucceeded;
    }
};

#endif // FIREBASEFACADE_H
#endif // ARDUINO
