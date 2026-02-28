#ifndef AWSCLOUDCONFIGPROVIDER_H
#define AWSCLOUDCONFIGPROVIDER_H

#include "IAwsCloudConfigProvider.h"

/** Implementation of IAwsCloudConfigProvider that returns empty strings (stub). */
/* @Component */
class AwsCloudConfigProvider : public IAwsCloudConfigProvider {
    Public AwsCloudConfigProvider() = default;
    Public Virtual ~AwsCloudConfigProvider() override = default;

    Public Virtual StdString GetEndpoint() const override { return {}; }
    Public Virtual StdString GetThingName() const override { return {}; }
    Public Virtual StdString GetDeviceSerial() const override { return {}; }
    Public Virtual StdString GetCaCert() const override { return {}; }
    Public Virtual StdString GetDeviceCert() const override { return {}; }
    Public Virtual StdString GetPrivateKey() const override { return {}; }
};

#endif /* AWSCLOUDCONFIGPROVIDER_H */
