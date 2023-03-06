# Getting Started

## Running with Docker

OvenMediaEngine supports the Docker image from [AirenSoft's Docker Hub](https://hub.docker.com/r/airensoft/ovenmediaengine) (**airensoft/ovenmediaengine)** repository. After installing [Docker](https://www.docker.com), you can simply run the following command:

```
docker run --name ome -d -e OME_HOST_IP=Your.HOST.IP.Address \
-p 1935:1935 -p 9999:9999/udp -p 9000:9000 -p 3333:3333 -p 3478:3478 -p 10000-10009:10000-10009/udp \
airensoft/ovenmediaengine:dev
```

{% hint style="warning" %}
To use TLS, you must set up a certificate. See [TLS Encryption](configuration/tls-encryption.md) for more information.
{% endhint %}

You can set the following environment variables.

### Ports

| Env                                | Default Value   |
| ---------------------------------- | --------------- |
| OME\_HOST\_IP                      | \*              |
| OME\_ORIGIN\_PORT                  | 9000            |
| OME\_RTMP\_PROV\_PORT              | 1935            |
| OME\_SRT\_PROV\_PORT               | 9999/udp        |
| OME\_MPEGTS\_PROV\_PORT            | 4000/udp        |
| OME\_LLHLS\_STREAM\_PORT           | 3333            |
| OME\_LLHLS\_STREAM\_TLS\_PORT      | 3334            |
| OME\_WEBRTC\_SIGNALLING\_PORT      | 3333            |
| OME\_WEBRTC\_SIGNALLING\_TLS\_PORT | 3334            |
| OME\_WEBRTC\_TCP\_RELAY\_PORT      | 3478            |
| OME\_WEBRTC\_CANDIDATE\_PORT       | 10000-10004/udp |

## Installation

### Install dependencies

OvenMediaEngine can work with a variety of open-sources and libraries. First, install them on your clean Linux machine as described below. We think that OME can support most Linux packages, but the tested platforms we use are Ubuntu 18+, Fedora 28+, and CentOS 7+.

```bash
(curl -LOJ https://github.com/AirenSoft/OvenMediaEngine/archive/master.tar.gz && tar xvfz OvenMediaEngine-master.tar.gz)
OvenMediaEngine-master/misc/prerequisites.sh
```

{% hint style="info" %}
If the prerequisites.sh script fails, try to run `sudo apt-get update` and rerun it. If it's not enough proceed with the [manual installation](troubleshooting.md#prerequisites-sh-script-failed).
{% endhint %}

### \*\*\*\*

### **Build & Run**

You can build the OvenMediaEngine source using the following command:

{% tabs %}
{% tab title="Ubuntu 18" %}
```bash
sudo apt-get update
cd OvenMediaEngine-master/src
make release
sudo make install
systemctl start ovenmediaengine
# If you want automatically start on boot
systemctl enable ovenmediaengine.service 
```
{% endtab %}

{% tab title="Fedora 28" %}
```bash
sudo dnf update
cd OvenMediaEngine-master/src
make release
sudo make install
systemctl start ovenmediaengine
# If you want automatically start on boot
systemctl enable ovenmediaengine.service
```
{% endtab %}

{% tab title="CentOS 7" %}
```bash
sudo yum update
source scl_source enable devtoolset-7
cd OvenMediaEngine-master/src
make release
sudo make install
systemctl start ovenmediaengine
# If you want automatically start on boot
systemctl enable ovenmediaengine.service
```

In addition, we recommend that you permanently set environment variables as follows.

```bash
$ echo 'source scl_source enable devtoolset-7' >> ~/.bashrc 
```
{% endtab %}
{% endtabs %}

{% hint style="info" %}
if `systemctl start ovenmediaengine` fails in Fedora, SELinux may be the cause. See [Check SELinux section of Troubleshooting](troubleshooting.md#check-selinux).
{% endhint %}

## Ports used by default

The default configuration uses the following ports, so you need to open it in your firewall settings.

| Port                        | Purpose                                                                                                                                  |
| --------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| 1935/TCP                    | RTMP Input                                                                                                                               |
| 9999/UDP                    | SRT Input                                                                                                                                |
| 4000/UDP                    | MPEG-2 TS Input                                                                                                                          |
| 9000/TCP                    | Origin Server (OVT)                                                                                                                      |
| <p>3333/TCP<br>3334/TLS</p> | <p>LLHLS Streaming<br><mark style="color:red;"><strong>* Streaming over Non-TLS is not allowed with modern browsers.</strong></mark></p> |
| <p>3333/TCP<br>3334/TLS</p> | WebRTC Signaling (both ingest and streaming)                                                                                             |
| 3478/TCP                    | WebRTC TCP relay (TURN Server, both ingest and streaming)                                                                                |
| 10000 - 10009/UDP           | WebRTC Ice candidate (both ingest and streaming)                                                                                         |

{% hint style="warning" %}
To use TLS, you must set up a certificate. See [TLS Encryption](configuration/tls-encryption.md) for more information.
{% endhint %}

You can open firewall ports as in the following example:

```bash
$ sudo firewall-cmd --add-port=3333/tcp
$ sudo firewall-cmd --add-port=3334/tcp
$ sudo firewall-cmd --add-port=1935/tcp
$ sudo firewall-cmd --add-port=9999/udp
$ sudo firewall-cmd --add-port=4000/udp
$ sudo firewall-cmd --add-port=3478/tcp
$ sudo firewall-cmd --add-port=9000/tcp
$ sudo firewall-cmd --add-port=10000-10005/udp
```
