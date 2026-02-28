#ifndef ICLOUDFACADE_H
#define ICLOUDFACADE_H

#include <StandardDefines.h>
#include "ICloudOperations.h"

DefineStandardPointers(ICloudFacade)

class ICloudFacade {
    Public Virtual ~ICloudFacade() = default;

    /** Returns the next command to execute, or empty string if none. */
    Public Virtual StdString GetCommand() = 0;

    /** Publish logs to cloud. Returns true on success, false on failure. */
    Public Virtual Bool PublishLogs(const StdMap<ULongLong, StdString>& logs) = 0;

    Public Virtual Void ResetCloudOperations() = 0;

    Public Virtual Void StopCloudOperations() = 0;

    Public Virtual Void StartCloudOperations() = 0;

    /** Returns true if the underlying cloud operations instance is dirty (e.g. after an error). */
    Public Virtual Bool IsDirty() const = 0;
};

#endif /* ICLOUDFACADE_H */
