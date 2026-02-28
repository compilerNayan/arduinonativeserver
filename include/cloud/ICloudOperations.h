#ifndef ICLOUDOPERATIONS_H
#define ICLOUDOPERATIONS_H

#include <StandardDefines.h>

/** Cloud operations interface: returns data/boolean instead of result enums. */
DefineStandardPointers(ICloudOperations)
class ICloudOperations {
    Public Virtual ~ICloudOperations() = default;

    /** Returns commands from cloud (e.g. list of "key:value" strings). Returns empty vector on failure or when not ready. */
    Public Virtual StdVector<StdString> RetrieveCommands() = 0;

    /** Publish logs to cloud at /logs. Map key = unique timestamp+seq (ULongLong), value = message. Returns true on success. */
    Public Virtual Bool PublishLogs(const StdMap<ULongLong, StdString>& logs) = 0;

    /** Returns true if RetrieveCommands or PublishLogs is currently running. */
    Public Virtual Bool IsOperationInProgress() const = 0;

    /** Returns true if the instance is dirty (e.g. after an error). */
    Public Virtual Bool IsDirty() const = 0;
};

#endif /* ICLOUDOPERATIONS_H */
