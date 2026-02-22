#ifndef IFIREBASEOPERATIONS_H
#define IFIREBASEOPERATIONS_H

#include <StandardDefines.h>

/** Result of a Firebase / remote-storage operation. */
enum class FirebaseOperationResult {
    OperationSucceeded,
    AnotherOperationInProgress,
    NotReady,
    Failed,
    NoData
};

DefineStandardPointers(IFirebaseOperations)

class IFirebaseOperations {
    Public Virtual ~IFirebaseOperations() = default;

    /** Retrieves commands from Firebase. @param out Filled with list of "key:value" strings. */
    Public Virtual FirebaseOperationResult RetrieveCommands(StdVector<StdString>& out) = 0;

    /** Publish logs to Firebase at /logs. Map key = unique timestamp+seq (ULongLong), value = message. Keys are written as ISO8601. */
    Public Virtual FirebaseOperationResult PublishLogs(const StdMap<ULongLong, StdString>& logs) = 0;

    /** Returns true if RetrieveCommands or PublishLogs is currently running. */
    Public Virtual Bool IsOperationInProgress() const = 0;

    /** Returns true if the instance is dirty (e.g. after an error); public methods will return default/empty until reset. */
    Public Virtual Bool IsDirty() const = 0;
};

#endif /* IFIREBASEOPERATIONS_H */
