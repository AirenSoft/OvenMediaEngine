# Enable GPU Acceleration

OvenMediaEngine supports GPU-based hardware decoding and encoding. Currently supported GPU acceleration devices are Intel's QuickSync and NVIDIA's NVDECODE/NVENCODE. This document describes how to install the video driver for OvenMediaEngine to use the GPU and how to set the Config file.\
Please check what graphics card you have and refer to the NVIDIA or Intel driver installation guide.

##

## 1. Install GPU Driver

### Install Intel QuickSync Driver

If you are using an Intel CPU that supports QuickSync, please refer to the following guide to install the driver. The OSes that support installation using the provided scripts are **CentOS 7/8** and **Ubuntu 18/20** versions. If you want to install the driver on a different OS, please refer to the Manual Installation Guide document.

When the Intel QuickSync driver installation is complete, the OS must be rebooted for normal operation.

{% code overflow="wrap" lineNumbers="true" %}
```bash
(curl -LOJ https://github.com/AirenSoft/OvenMediaEngine/archive/master.tar.gz && tar xvfz OvenMediaEngine-master.tar.gz) 
OvenMediaEngine-master/misc/install_intel_driver.sh
```
{% endcode %}

#### How to check driver installation

After the driver installation is complete, check whether the driver operates normally with the Matrix Monitor program.

```bash
# Use the samples provided in the Intel Media SDK
# Check the list of codecs supported by iGPU
/MediaSDK-intel-mediasdk-21.1.2/build/__bin/release/simple_7_codec
```

### Install NVIDIA GPU Driver

If you are using an NVIDIA graphics card, please refer to the following guide to install the driver. The OS that supports installation with the provided script are **CentOS 7/8** and **Ubuntu 18/20** versions. If you want to install the driver in another OS, please refer to the manual installation guide document.

CentOS environment requires the process of uninstalling the nouveau driver. After uninstalling the driver, the first reboot is required, and a new NVIDIA driver must be installed and rebooted. Therefore, two install scripts must be executed.

{% code overflow="wrap" lineNumbers="true" %}
```bash
(curl -LOJ https://github.com/AirenSoft/OvenMediaEngine/archive/master.tar.gz && tar xvfz OvenMediaEngine-master.tar.gz)
OvenMediaEngine-master/misc/install_nvidia_driver.sh
```
{% endcode %}

#### How to check driver installation

After the driver installation is complete, check whether the driver is operating normally with the nvidia-smi command.

```bash
$ nvidia-smi

Thu Jun 17 10:20:23 2021
+-----------------------------------------------------------------------------+
| NVIDIA-SMI 465.19.01    Driver Version: 465.19.01    CUDA Version: 11.3     |
|-------------------------------+----------------------+----------------------+
| GPU  Name        Persistence-M| Bus-Id        Disp.A | Volatile Uncorr. ECC |
| Fan  Temp  Perf  Pwr:Usage/Cap|         Memory-Usage | GPU-Util  Compute M. |
|                               |                      |               MIG M. |
|===============================+======================+======================|
|   0  NVIDIA GeForce ...  Off  | 00000000:01:00.0 Off |                  N/A |
| 20%   35C    P8    N/A /  75W |    156MiB /  1997MiB |      0%      Default |
|                               |                      |                  N/A |
+-------------------------------+----------------------+----------------------+

+-----------------------------------------------------------------------------+
| Processes:                                                                  |
|  GPU   GI   CI        PID   Type   Process name                  GPU Memory |
|        ID   ID                                                   Usage      |
|=============================================================================|
|    0   N/A  N/A      1589      G   /usr/libexec/Xorg                  38MiB |
|    0   N/A  N/A      1730      G   /usr/bin/gnome-shell              115MiB |
+-----------------------------------------------------------------------------+
```

### Manual Installation

If the provided installation script fails, please refer to the manual installation guide.

{% content-ref url="manual-installation.md" %}
[manual-installation.md](manual-installation.md)
{% endcontent-ref %}



## 2. Prerequisites&#x20;

If you have finished installing the driver to use the GPU, you need to reinstall the open source library using Prerequisites.sh . The purpose is to allow external libraries to use the installed graphics driver.

### Using Intel QuickSync GPU

```bash
OvenMediaEngine-master/misc/prerequisites.sh --enable-qsv
```

### Using NVIDIA GPU

```bash
OvenMediaEngine-master/misc/prerequisites.sh --enable-nvc
```

### Using Docker with NVIDIA GPU

Describes how to enable GPU acceleration for users running OvenMediaEngine in the Docker runtime environment. To use GPU acceleration in Docker, the NVIDIA Driver must be installed on the host OS and the NVIDIA Container Toolkit must be installed. This toolkit includes container runtime libraries and utilities to use NVIDIA GPUs in Docker containers.

![Reference : https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/overview.html](<../../.gitbook/assets/image (30) (1).png>)

```bash
OvenMediaEngine-master/misc/install_nvidia_docker_container.sh
```

{% hint style="info" %}
The NVIDIA Driver must have been previously installed
{% endhint %}



## 3. Configuration

To use hardware acceleration, set the **HardwareAcceleration** option to **true** under OutputProfiles. If this option is enabled, a hardware codec is automatically used when creating a stream, and if it is unavailable due to insufficient hardware resources, it is replaced with a software codec.

```markup
<VirtualHosts>
   <VirtualHost>
      <Name>default</Name>

      <!-- Settings for multi domain and TLS -->
      <Host>
         ...
      </Host>

      <!-- Settings for applications -->
      <Applications>
         <Application>
            <Name>app</Name>
            <Type>live</Type>
            <OutputProfiles>
               <!-- Settings to use hardware codecs -->
               <HardwareAcceleration>true</HardwareAcceleration>
               <OutputProfile>
                  <Name>bypass_stream</Name>
                  <OutputStreamName>${OriginStreamName}_o</OutputStreamName>
                  <Encodes>
                     <Video>
                        <Codec>h264</Codec>
                        <Width>1920</Width>
                        <Height>1080</Height>
                        <Bitrate>5000000</Bitrate>
                        <Framerate>30</Framerate>
                     </Video>
                     
                     <Video>
                        <Codec>h265</Codec>
                        <Width>1280</Width>
                        <Height>720</Height>
                        <Bitrate>5000000</Bitrate>
                        <Framerate>30</Framerate>
                     </Video> 
                  </Encodes>
               </OutputProfile>

            </OutputProfiles>
            <Providers>
               ...
            </Providers>
            <Publishers>
               ...
            </Publishers>
         </Application>
      </Applications>
   </VirtualHost>
</VirtualHosts>
```

{% content-ref url="../../configuration/" %}
[configuration](../../configuration/)
{% endcontent-ref %}

## 4. Build & Run

You can build the OvenMediaEngine source using the following command. Same as the contents of **Getting Started**.

{% tabs %}
{% tab title="Ubuntu  18+" %}
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

{% tab title="Fedora 28+ " %}
```
sudo dnf update
cd OvenMediaEngine-master/src
make release
sudo make install
systemctl start ovenmediaengine
# If you want automatically start on boot
systemctl enable ovenmediaengine.service
```
{% endtab %}

{% tab title="CentOS 7+" %}
```
sudo yum update
source scl_source enable devtoolset-7
cd OvenMediaEngine-master/src
make release
sudo make install
systemctl start ovenmediaengine
# If you want automatically start on boot
systemctl enable ovenmediaengine.service
```
{% endtab %}
{% endtabs %}

{% content-ref url="../../getting-started/" %}
[getting-started](../../getting-started/)
{% endcontent-ref %}

### How to build and run docker image

To use Docker, you need to build a new Docker image. To build an OvenMediaEngine image with GPU support, you need to set some parameters. First, check the major version of the NVIDIA driver installed on the host OS

<pre class="language-bash"><code class="lang-bash">$ nvidia-smi --query-gpu=driver_version --format=csv
driver_version
<strong><a data-footnote-ref href="#user-content-fn-1">470.182.03</a>
</strong></code></pre>

Set the major version as the value of the **NVIDIA\_DRIVER** argument when building docker image.

{% code overflow="wrap" %}
```
docker build --file Dockerfile -t airensoft/ovenmediaengine:dev --build-arg GPU=TRUE --build-arg NVIDIA_DRIVER=470
```
{% endcode %}

After the build is complete, you must include the **--gpus all** option when running Docker

{% code overflow="wrap" %}
```
docker run -d -p 1935:1935 -p 4000-4005:4000-4005/udp -p 3333:3333 -p 3478:3478 -p 8080:8080 -p 9000:9000 -p 9999:9999/udp -p 10006-10010:10006-10010/udp --gpus all airensoft/ovenmediaengine:dev
```
{% endcode %}

## Support Format

The codecs available using hardware accelerators in OvenMediaEngine are as shown in the table below. Different GPUs support different codecs. If the hardware codec is not available, you should check if your GPU device supports the codec.

<table><thead><tr><th>Device</th><th width="199" align="center">H264</th><th align="center">H265</th><th align="center">VP8</th><th align="center">VP9</th></tr></thead><tbody><tr><td>QuickSync</td><td align="center">D / E</td><td align="center">D / E</td><td align="center">-</td><td align="center">-</td></tr><tr><td>NVIDIA</td><td align="center">D / E</td><td align="center">D / E</td><td align="center">-</td><td align="center">-</td></tr><tr><td>Docker on NVIDIA Container Toolkit</td><td align="center">D / E</td><td align="center">D / E</td><td align="center">-</td><td align="center">-</td></tr></tbody></table>

D : Decoding, E : Encoding

## Reference

* Quick Sync Video Format : [https://en.wikipedia.org/wiki/Intel\_Quick\_Sync\_Video](https://en.wikipedia.org/wiki/Intel\_Quick\_Sync\_Video)
* NVIDIA NVDEC Video Format : [https://en.wikipedia.org/wiki/Nvidia\_NVDEC](https://en.wikipedia.org/wiki/Nvidia\_NVDEC)
* NVIDIA NVENV Video Format : [https://en.wikipedia.org/wiki/Nvidia\_NVENC](https://en.wikipedia.org/wiki/Nvidia\_NVENC)
* CUDA Toolkit Installation Guide : [https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html#introduction](https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html#introduction)
* NVIDIA Container Toolkit : [https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/arch-overview.html#arch-overview](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/arch-overview.html#arch-overview)
* Quick Sync Video format support: [https://en.wikipedia.org/wiki/Intel\_Quick\_Sync\_Video](https://en.wikipedia.org/wiki/Intel\_Quick\_Sync\_Video#AMD)

##

[^1]: 
