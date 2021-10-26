# Test Player

We provide you our test player to make sure that OvenMediaEngine works well. Most browsers prohibit access to the TLS-based HTTPS site through unsecured HTTP or WebSocket (WS) for security reasons. Thus, we have prepared the HTTP or HTTPS based player as follows:

| Site URL                                                   | Description                                                                                            |
| ---------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| [http://demo.ovenplayer.com](http://demo.ovenplayer.com)   | A player without TLS that can test all streaming URLs such as `WS://`, `WSS://`, `HTTP://`, `HTTPS://` |
| [https://demo.ovenplayer.com](https://demo.ovenplayer.com) | A player with TLS that can only test `WSS://` and `HTTPS://` based streaming URLs                      |

![](<.gitbook/assets/image (18).png>)



### Delay option for Low-Latency DASH&#x20;

When playing Low-Latency DASH, you can control the delay time in the player as shown below. Delay time is closely related to the buffering size. The smaller the value, the shorter the latency, but if it is too small, there is no buffer and playback may not be smooth. In a typical network environment, it is most stable to give 2 as the delay value.

![](<.gitbook/assets/image (22).png>)
