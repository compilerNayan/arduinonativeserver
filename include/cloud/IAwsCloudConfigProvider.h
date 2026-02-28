#ifndef IAWSCLOUDCONFIGPROVIDER_H
#define IAWSCLOUDCONFIGPROVIDER_H

#include <StandardDefines.h>

/** Provides AWS IoT connection config as StdString values (endpoint, thing name, device serial, certs). */
DefineStandardPointers(IAwsCloudConfigProvider)
class IAwsCloudConfigProvider {
    Public Virtual ~IAwsCloudConfigProvider() = default;

    Public Virtual StdString GetEndpoint() const = 0;
    Public Virtual StdString GetThingName() const = 0;
    Public Virtual StdString GetDeviceSerial() const = 0;
    Public Virtual StdString GetCaCert() const = 0;
    Public Virtual StdString GetDeviceCert() const = 0;
    Public Virtual StdString GetPrivateKey() const = 0;
};

#endif /* IAWSCLOUDCONFIGPROVIDER_H */
