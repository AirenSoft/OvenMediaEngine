---
name: Help
about: Request technical support
title: ''
labels: help
assignees: ''

---

Before asking for help, please read the troubleshooting section in the manual carefully.
https://airensoft.gitbook.io/ovenmediaengine/troubleshooting

Please fill out the form below to create an issue. Failure to follow the form may result in late responses or close issues.

**PROBLEM**

Please describe the problem you are experiencing.

**EXPECTATION**

Please write how you would like the problem to be resolved.

**ENVIRONMENTAL INFORMATION**

- OvenMediaEngine Information

Please check whether it is reproduced in the master branch first. The Docker image for the master branch is airensoft/ovenmediaengine:dev .
If you are using the Master branch, please write Master.
If you are using Release, please write the version (eg 0.12.4).
If you are using Docker by building it yourself, please tell us what version of the image you built.
If you are using a public Docker image, please provide the image path with tags. (e.g. airensoft/ovenmediaengine:0.12.8)

- Encoder information

What encoder are you using?(e.g. OBS 10.22) And how is the codec set on that encoder?
If you are using OvenLiveKit as an encoder, please let me know if it reproduces here https://demo.ovenplayer.com/demo_input.html. And please tell me what error is output in the browser's developer console.
If you are using RTSP Pull, please let me know which RTSP server you are using.

- Player information

Please let me know if it reproduces on http://demo.ovenplayer.com or https://demo.ovenplayer.com. And please tell me what error is output in the browser's developer console.

- URL

Please tell me which address you used to send and play. You can hide IP or Domain.

- Operating system and version

Please tell me the OS and version (e.g. Ubuntu 20.04)

**SETUP INFORMATION AND LOGS**

Please upload Server.xml.
Please upload the entire file /var/log/ovenmediaengine/ovenmediaengine.log. (You may delete your personal information.)
It's hard to find the problem if you post only part of the log. Be sure to upload the entire file.

**OTHER HELPFUL INFORMATION**

Please write any information that anyone viewing this issue can reference.
