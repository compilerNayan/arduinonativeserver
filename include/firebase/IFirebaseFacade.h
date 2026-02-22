#ifndef IFIREBASEFACADE_H
#define IFIREBASEFACADE_H

#include <StandardDefines.h>
#include "IFirebaseOperations.h"

DefineStandardPointers(IFirebaseFacade)

class IFirebaseFacade {
    Public Virtual ~IFirebaseFacade() = default;

    /** Fills @param out with the next command to execute. Returns operation result. */
    Public Virtual FirebaseOperationResult GetCommand(StdString& out) = 0;

    /** Publish logs to Firebase. Returns operation result. */
    Public Virtual FirebaseOperationResult PublishLogs(const StdMap<ULongLong, StdString>& logs) = 0;

    Public Virtual Void ResetFirebaseOperations() = 0;

    Public Virtual Void StopFirebaseOperations() = 0;

    Public Virtual Void StartFirebaseOperations() = 0;

    /** Returns true if the underlying Firebase operations instance is dirty (e.g. after an error). */
    Public Virtual Bool IsDirty() const = 0;
};

#endif /* IFIREBASEFACADE_H */
