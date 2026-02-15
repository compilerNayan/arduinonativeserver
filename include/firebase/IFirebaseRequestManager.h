#ifndef IFIREBASEREQUESTMANAGER_H
#define IFIREBASEREQUESTMANAGER_H

#include <StandardDefines.h>

DefineStandardPointers(IFirebaseRequestManager)
class IFirebaseRequestManager {
    Public Virtual ~IFirebaseRequestManager() = default;

    /**
     * @brief Retrieve requests from Firebase, delete them from the database, and return the list in one go.
     * @return List of strings, each element is "key:value". Empty list if no data or on error.
     */
    Public Virtual StdList<StdString> RetrieveRequests() = 0;
};

#endif // IFIREBASEREQUESTMANAGER_H
