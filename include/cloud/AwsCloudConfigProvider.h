#ifndef AWSCLOUDCONFIGPROVIDER_H
#define AWSCLOUDCONFIGPROVIDER_H

#include "IAwsCloudConfigProvider.h"
#include <StandardDefines.h>
#include <IDeviceDetails.h>

/** Hardcoded AWS IoT config (endpoint, thing name, certs from secrets). Device serial from IDeviceDetails. */
/* @Component */
class AwsCloudConfigProvider : public IAwsCloudConfigProvider {
    Public AwsCloudConfigProvider() = default;
    Public Virtual ~AwsCloudConfigProvider() override = default;

    /* @Autowired */
    Private IDeviceDetailsPtr deviceDetails_;

    Public Virtual StdString GetEndpoint() const override { return "a2hlcpmplecdfa-ats.iot.us-east-1.amazonaws.com"; }
    Public Virtual StdString GetThingName() const override { return "nayanesp32"; }
    Public Virtual StdString GetDeviceSerial() const override { return deviceDetails_->GetSerialNumber(); }
    Public Virtual StdString GetCaCert() const override {
        return R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";
    }
    Public Virtual StdString GetDeviceCert() const override {
        return R"KEY(
-----BEGIN CERTIFICATE-----
MIIDWTCCAkGgAwIBAgIUcY6+6hrzkVIFoB7J8mRZj9xRBjUwDQYJKoZIhvcNAQEL
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTI2MDIyODA4MTE0
MVoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALfELeJlTObH0plowG+u
+UL0SH0TlH6sBONxONvMNPmTFv29XnSUzHcA0JaAmrmjSj+RNnHPugprM6LTpP5q
i+9iUnR6p+hrTvojZSrXSlQp6kgeJ0pcOlnp/HBo4x9B5oncIzOUxELZTH94DFTX
EbGT9txTg1Y5tGbW5qI2mrKmVOr7fVvVdAwDnVp8GdnxWpg6znm5CGFYB86US2wJ
9DlXgep1Y0V/wDuoYd6XSKnaWyas66uPeR8YfMG4hwE6tJsYHw6U1ApsZzrUgJ7C
AfRBRBQP5q3rpmGU27IURgl3V7z6JpYT6i/PhTATfg5nl+M2K4TDdn6as0fevaC0
85kCAwEAAaNgMF4wHwYDVR0jBBgwFoAU74OSzf3dpIOge1jstGz0pJvy5CUwHQYD
VR0OBBYEFLe7q3dy+hTnI5GOZfOXzWrh+a6xMAwGA1UdEwEB/wQCMAAwDgYDVR0P
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQDoONiSJWltibeJhPzwFxcuTTi2
roogQAE+kBjwlq9LY/KFXapgPWIO7oBNyFiguElemiQzUCxW5bPbMry7POEN80uS
P2LSHXmzgnxRf5WtU42XNWpyyn1CRdRANISzABu/LGR+ylV0dYYZdDW2bmbVIxG7
kVMz0JI45eUfrR0mfNEabi/YKtAHPVnQ+QDn9omMWNQN9p8Ny04IaxWQspDPkyuq
uJHMNUtHdEpjX8ehgUq4mMGdvFEhV9BwDU6r2K4Ck3lh+iEct2ANqeGUVqENE5xW
I4fBQ/xEtcjvy+eQLc+RqBp4LaG/xqZaKrT0PgdAwxJ0thvTs8lKlMOvlju4
-----END CERTIFICATE-----
)KEY";
    }
    Public Virtual StdString GetPrivateKey() const override {
        return R"KEY(
-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEAt8Qt4mVM5sfSmWjAb675QvRIfROUfqwE43E428w0+ZMW/b1e
dJTMdwDQloCauaNKP5E2cc+6CmszotOk/mqL72JSdHqn6GtO+iNlKtdKVCnqSB4n
Slw6Wen8cGjjH0HmidwjM5TEQtlMf3gMVNcRsZP23FODVjm0ZtbmojaasqZU6vt9
W9V0DAOdWnwZ2fFamDrOebkIYVgHzpRLbAn0OVeB6nVjRX/AO6hh3pdIqdpbJqzr
q495Hxh8wbiHATq0mxgfDpTUCmxnOtSAnsIB9EFEFA/mreumYZTbshRGCXdXvPom
lhPqL8+FMBN+DmeX4zYrhMN2fpqzR969oLTzmQIDAQABAoIBAH/P9NH3/wqshK+q
id2j5z5IRXqo+ak435WJlDBb0aScR2toIKAZNjS23l0vSW5AQk4AI8v43L6OXTWs
2p8RYA65ZfwZq1sd96pGoACyCMKM/KP4RP+VMgiDk85jRUTiQwQzllAz8mVEWc52
oq0HgQjvjs0jUL/SFsVIRtUgAWyq0AU/jB+BkE0Hdg15XNeF6c+G8dIr8ypSguhN
ZrsvmowvGMvQ3IYyd6ga7ElD+8eVF6yHyNBpgSt6Jk4L6BCDE+EScCIHjt6BZnx5
fhBm3nCjrHTEEOWzp63qU9vFfGYy2RYmVMGoPcHQAQY8LSuCHv4PAb68L5X9hDAj
jCrTOXkCgYEA8XaComxmR0IEX/QDgzYfcmw7gZUev0YjNhb6CmvqTjh1idXMksJ1
O5pQzuPUOd0INm48Gm/wrS+rXf6RxNMa95+aVF4Y9yZfs0kl0hc/WjIkyyD8ciqx
P55q5aBOfwoRyrjo5eZ4DQ1oOUzmoDq0ksyPDw0GHF7lkFwqBje+F7MCgYEAwtRv
DYJkES0dLfvpXGq4jDQJFmi4flXowcvRv3bFS7H/siz6z1OW5DoUE2kyyvsMiL9/
bFpIgsifLGXAf0brPxwv+jkEQ+0c3GSdCy/V30iEyzbCmB72tOMHze9kwABmjfcN
XHPp6RjBkT8siGqwywPzXqmOYxve/7sxuKS8YYMCgYAL/h3Y4VdnHKFI/r6Z6rs/
HJ8L/ZMMXvhqvFNo5xk5D1bPJL5HX78g2bY/wFMPGKpKwcjca0jh4Sc6wOUOiImj
WNr6a74DsHU6W1y0hZEQ8sKfECFZrkMlbMHUa9TaPG7LyclPedXmmA7gIbCmrqO/
UsecMMJn8FKoi9tOecBIPwKBgQCPEq+mzQ0tK+MUxLyfOGPj2caepTJu3Cm8Py/9
mXvTVZi/QJbCr9DMMvZRNtytAOU4euBZMoezOx7g3KwbC3pv8wQsjbhbJYIfOue0
smEtwjR6gvEuAvcK6PnvOhDTirfFIxq1vQ2WNq7XOfw7BZZkz7bl2kAM+get5srh
nGAWfwKBgQDmTZkTKVGLldsf1GhzXXO2DhvgBzf9HmHMQLryQpCuUb8nxDCMdaWK
VTfgGuqWyNiUYIP/CxtrX06ooNftzLigs5dJAVAWuzYK8kmj0hD21T4E9sCvgcN8
dLQmaSKdWdc57233tBTVCknsUyaOKzTlg3y+fnzAYZwCRQMBt81/9A==
-----END RSA PRIVATE KEY-----
)KEY";
    }
};

#endif /* AWSCLOUDCONFIGPROVIDER_H */
