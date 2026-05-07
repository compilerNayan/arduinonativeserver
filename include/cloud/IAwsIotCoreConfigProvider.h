#ifndef IAWSIOTCORECONFIGPROVIDER_H
#define IAWSIOTCORECONFIGPROVIDER_H

#include <StandardDefines.h>

/** Provides AWS IoT Core connection config as StdString values. */
DefineStandardPointers(IAwsIotCoreConfigProvider)
class IAwsIotCoreConfigProvider {
    Public Virtual ~IAwsIotCoreConfigProvider() = default;

    Public Virtual StdString GetEndpoint() const = 0;
    Public Virtual StdString GetThingName() const = 0;
    Public Virtual StdString GetCaCert() const = 0;
    Public Virtual StdString GetDeviceCert() const = 0;
    Public Virtual StdString GetPrivateKey() const = 0;
    Public Virtual StdString GetPublishTopic() const = 0;
    Public Virtual StdString GetSubscribeTopic() const = 0;
};

#endif /* IAWSIOTCORECONFIGPROVIDER_H */
