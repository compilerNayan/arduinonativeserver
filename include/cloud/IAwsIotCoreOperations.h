#ifndef IAWSIOTCOREOPERATIONS_H
#define IAWSIOTCOREOPERATIONS_H

#include <StandardDefines.h>

DefineStandardPointers(IAwsIotCoreOperations)
class IAwsIotCoreOperations {
    Public Virtual ~IAwsIotCoreOperations() = default;

    Public Virtual Bool SendMessage(CStdString message) = 0;
    Public Virtual StdVector<StdString> ReceiveMessages() = 0;

    Public Virtual Bool SendMessage(CStdString message, CStdString topicName) = 0;
    Public Virtual StdVector<StdString> ReceiveMessages(CStdString topicName) = 0;
};

#endif /* IAWSIOTCOREOPERATIONS_H */
