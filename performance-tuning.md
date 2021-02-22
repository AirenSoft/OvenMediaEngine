# Performance Tuning

OvenMediaEngine provides a way to control the server's performance by service type through configuration files as shown below.

```text
<Application>
<Publishers>
  <StreamLoadBalancingThreadCount>1</StreamLoadBalancingThreadCount>
  <SessionLoadBalancingThreadCount>8</SessionLoadBalancingThreadCount>
</Publishers>
</Application>
```

### StreamLoadBalancingThreadCount

| Type | Value |
| :--- | :--- |
| Default | 1 |
| Minimum | 1 |
| Maximum | 72 |

With `StreamLoadBalancingThreadCount`, you can set the number of threads for distributed processing of streams when hundreds of streams are created in one applicaiton. When an application is requested to create a stream, the stream is evenly attached to one of created threads. The main role of Stream is to packetize raw media packets into the media format of the protocol to be transmitted. When there are thousands of streams, it is difficult to process them in one thread. Also, if SessionLoadBalancingThreadCount is set to 0, StreamLoadBalancingThread is responsible for sending media packets to the session. 

It is recommended that this value does not exceed the number of CPU cores.

### 

### SessionLoadBalancingThreadCount

| Type | Value |
| :--- | :--- |
| Default | 8 |
| Minimum | 0 |
| Maximum | 72 |

It may be impossible to send data to thousands of viewers in one thread. SessionLoadBalancingThreadCount allows sessions to be distributed across multiple threads and transmitted simultaneously. This means that resources required for SRTP encryption of WebRTC or TLS encryption of HLS/DASH can be distributed and processed by multiple threads. It is recommended that this value not exceed the number of CPU cores.



### Use-Case

If a large number of streams are created and very few viewers connect to each stream, increase StreamLoadBalancingThreadCount and lower SessionLoadBalancingThreadCount as follows.

```text
<Publishers>
  <StreamLoadBalancingThreadCount>32</StreamLoadBalancingThreadCount>
  <SessionLoadBalancingThreadCount>0</SessionLoadBalancingThreadCount>
</Publishers>
```

If a small number of streams are created and a very large number of viewers are connected to each stream, lower StreamLoadBalancingThreadCount and increase SessionLoadBalancingThreadCount as follows.

```text
<Publishers>
  <StreamLoadBalancingThreadCount>1</StreamLoadBalancingThreadCount>
  <SessionLoadBalancingThreadCount>32</SessionLoadBalancingThreadCount>
</Publishers>
```

