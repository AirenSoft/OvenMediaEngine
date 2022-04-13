---
description: Web Console is a web application that can monitor and operate OvenMediaEngine.
coverY: 0
---

# Web Console

## Overview

The main features of Web Console are:

* Reads and displays config file of OvenMediaEngine.
* Interpret the OvenMediaEngine configuration file and work with the REST API.
* Use the REST API to display a list of Virtual Hosts, Applications, Streams, and configuration information.
* You can reload Virtual Host and Application.
* You can load and unload Virtual Hosts that have been added or removed from the OvenMediaEngine configuration file.
* Provides a Test Player that can be played on the generated stream.

## Installation & Configuration

The Web Console is distributed as part of the OvenMediaEngine Enterprise package. The default installation path and running port for the Web Console are:

| Installation path       | `/usr/share/ovenmediaengine/console`                                   |
| ----------------------- | ---------------------------------------------------------------------- |
| Configuration file path | `/usr/share/ovenmediaengine/console/conf/config.yaml`                  |
| Log file path           | `/var/log/ovenmediaengine/console/ovenmediaengine-console.log`         |
| Running port            | `5000`                                                                 |
| Initial account         | <p>username: <code>admin</code></p><p>password: <code>admin</code></p> |

The default configurations for the Web Console are as follows:

```yaml
# Config about the OvenMediaEngine Console.
ovenmediaengineconsole:
  # Port for serve OvenMediaEngine Console. Default is 5000.
  port: 5000
  # Log dir of OvenMediaEngine Console.
  # When using a different value, be careful that the directory exists.
  logDir: /var/log/ovenmediaengine/console
  # Log level: CRITICAL, FATAL, ERROR, WARN, WARNING, INFO, DEBUG, NOTSET
  logLevel: DEBUG

# Config about the OvenMediaEngine.
ovenmediaengine:
  # Directory where the OvenMediaEngine binary is installed.
  # Default is /usr/share/ovenmediaengine.
  installedDir: /usr/share/ovenmediaengine
```

## Common user interfaces

This section describes the common user interface of Web Console.

### Sign in

![](../.gitbook/assets/1.png)

You can sign in as username: `admin` and password: `admin`, which are the initial account information.

### Common layout

![](../.gitbook/assets/2.png)

#### ① Home link (Server page)&#x20;

This is the link to the first entry screen. You can check the settings of OvenMediaEngine.

#### ② Account information link&#x20;

A link to a page where you can edit your account information.

#### ③ Sign out&#x20;

Sign out and expire the session.

#### ④ Virtual Host, Application, Stream navigation&#x20;

Tree-structured navigation with access to Virtual Hosts, Applications, and Streams running on OvenMediaEngine. The selected resource is highlighted.

{% hint style="info" %}
You can load and unload virtual hosts or applications when they are added or removed from the OvenMediaEngine configuration.
{% endhint %}

#### ⑤ Update Virtual Host, Application, Stream navigation&#x20;

Reload the Virtual Host, Application, and Stream information. If a new stream is added to your application, you can view the new stream.

#### ⑥ Current path

Shows the path of the selected Virtual Host, Application, Stream. You can click the parent path to select that resource.

#### ⑦ Virtual Host, Application, Stream Details&#x20;

Displays statistics, actions, and setting information of the selected resource.

## Features

Describes the main features of Web Console.

### Displaying configuration

The OvenMediaEngine Console displays configuration for each Server, Virtual Host, Application, and Stream.

#### Server configuration

![](../.gitbook/assets/3.png)

**① Display configuration files**

If you select Server, you will see all the configuration files used by OvenMediaEngine.

#### Virtual Host, Application, Stream statistics and information

If you select Virtual Host, Application, or Stream, you can see traffic / connection statistics and configuration.

![Display statistics](../.gitbook/assets/4.png)

![Display of configuration of Virtual Host, Application](../.gitbook/assets/5.png)

![Displaying input/output stream information](../.gitbook/assets/6.png)

**① Statistics display**&#x20;

You can check the cumulative bytes in / out and the number of connected sessions for each protocol.

**② Creation time display**&#x20;

You can check the creation time of the selected resource.

**③ Detailed information display**&#x20;

Displays detailed information about the selected resource. When Virtual Host or Application is selected, the configuration is displayed, and when Stream is selected, the input stream and output stream information is displayed.

### Reload virtual host, application dynamically

If you change the Virtual Host or Application settings directly in the `Server.xml` or include Virtual Host configuration file, you can apply the changes to OvenMediaEngine without restarting OvenMediaEngine. Use Actions → Reload button on the details page of Virtual Host or Application.

![Reload Virtual Host](../.gitbook/assets/7.png)

![Reload Application](../.gitbook/assets/8.png)

{% hint style="info" %}
Disabled features such as New Application and Modify application will be added in the near future.
{% endhint %}

### Add or remove virtual hosts dynamically

If you modify the OvenMediaEngine `Server.xml` directly to add a new Virtual Host or remove an existing Virtual Host, you can reflect the added or removed Virtual Host to the OvenMediaEngine without restarting the OvenMediaEngine. If you add or delete the separated Virtual Host configuration files , it will works in the same way.

![](../.gitbook/assets/9.png)

**① Reload the OvenMediaEngine configuration**

To reload the modified configuration file, refresh each page or use the `Refresh` button in the left navigation.

**② Add a new Virtual Host**&#x20;

If a new Virtual Host is added to the configuration file, the new Virtual Host will be added to the left navigation and you will see a `Load` button that can be immediately reflected to the OvenMediaEngine.

**③ Remove the existing Virtual Host**&#x20;

Similarly, if an existing Virtual Host is removed from the configuration file, you will see an `Unload` button in the left navigation. You can use the Unload button to remove an existing Virtual Host from OvenMediaEngine.

### Add or remove applications dynamically

As with Virtual Host, if you modify the OvenMediaEngine configuration file directly to add or remove an Application, you can add or remove the Application to OvenMediaEngine without restarting OvenMediaEngine.

![](../.gitbook/assets/10.png)

**① Reload OvenMediaEngine configuration**

To reread the modified configuration file, refresh each page or use the `Refresh` button in the left navigation.

**② Add a new application**&#x20;

When a new application is added to the config file, the new application is added to the left navigation with a Load button that can be immediately applied to the OvenMediaEngine. You can use the Load button to apply the new application to OvenMediaEngine.

**③ Remove the existing application**&#x20;

Similarly, if an existing application is removed from the configuration file, an Unload button will appear in the left navigation.

### Test Player

If you select a stream, we provide a player that can play that stream.

![](../.gitbook/assets/11.png)

**① Test Playback link**

If you select a Stream, you can enter the page where you can play the stream with the `Test Playback` link.

**② OvenMediaEngine host setting**&#x20;

Set the IP address or domain of OvenMediaEngine (<mark style="color:red;">required</mark>).

{% hint style="info" %}
Host information is different depending on the operating environment of OvenMediaEngine, so enter it manually.
{% endhint %}

**③ TCP playback settings**&#x20;

Check if you want to play the WebRTC stream with TCP.

**④ Output stream list**&#x20;

It interprets the information set in `②` and `③` and the configurations of OvenMediaEngine and display the list of playable output streams.

**⑤ Output stream playback**&#x20;

If you click the play button, the stream is played with `⑥` Test Player (OvenPlayer).

### Account setting

You can change the account information used to sign in to the Web Console.

![](../.gitbook/assets/12.png)

**① Account setting link**

You can enter the account information page through the `Account` link.

The default account is set to username: `admin`, password: `admin`. If you change your account information, your session will be expired and you will need to sign in again.

