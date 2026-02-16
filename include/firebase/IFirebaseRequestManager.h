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

    /**
     * @brief Disconnect from Firebase and reconnect. Waits for any in-progress RetrieveRequest to finish before refreshing.
     */
    Public Virtual Void RefreshConnection() = 0;

    /**
     * @brief Reset/tear down the Firebase connection (no reconnect). Safe to call RefreshConnection() afterward.
     */
    Public Virtual Void DismissConnection() = 0;
};

#endif // IFIREBASEREQUESTMANAGER_H
