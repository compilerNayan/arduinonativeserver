#ifndef IFIREBASEREQUESTMANAGER_H
#define IFIREBASEREQUESTMANAGER_H

#include <StandardDefines.h>

DefineStandardPointers(IFirebaseRequestManager)
class IFirebaseRequestManager {
    Public Virtual ~IFirebaseRequestManager() = default;

    /**
     * @brief Retrieve one request from Firebase (or from internal queue). Deletes from DB when fetching from Firebase.
     * @return One "key:value" string, or empty string if no data or on error.
     */
    Public Virtual StdString RetrieveRequest() = 0;
};

#endif // IFIREBASEREQUESTMANAGER_H
