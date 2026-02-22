#ifndef IFIREBASEFACADE_H
#define IFIREBASEFACADE_H

#include <StandardDefines.h>
#include "IFirebaseOperations.h"

DefineStandardPointers(IFirebaseFacade)

class IFirebaseFacade {
    Public Virtual ~IFirebaseFacade() = default;

    /** Fills @param out with the next command to execute. Returns operation result. */
    Public Virtual FirebaseOperationResult GetCommand(StdString& out) = 0;

    Public Void ResetFirebaseOperations() = 0;

    Public Void StopFirebaseOperations() = 0;

    Public Void StartFirebaseOperations() = 0;
};

#endif /* IFIREBASEFACADE_H */
