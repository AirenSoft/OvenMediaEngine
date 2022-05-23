# Clustering

OvenMediaEngine supports clustering and ensures High Availability (HA) and scalability.

![](.gitbook/assets/image.png)

OvenMediaEngine supports the Origin-Edge structure for cluster configuration and provides scalability. Also, you can set Origin as `Primary` and `Secondary` in OvenMediaEngine for HA.

## Origin-Edge Configuration

The OvenMediaEngine running as edge pulls a stream from an external server when a user requests it. The external server could be another OvenMediaEngine with OVT enabled or another stream server that supports RTSP.&#x20;

The OVT is a protocol defined by OvenMediaEngine to relay stream between Origin-Edge and OVT can be run over SRT and TCP. For more information on the SRT Protocol, please visit the [SRT Alliance](https://www.srtalliance.org/) site.

### Origin&#x20;

OvenMediaEngine provides OVT protocol for passing streams from the origin to the edge. To run OvenMediaEngine as Origin, OVT port, and OVT Publisher must be enabled as follows :

```xml
<Server version="5">
	<Bind>
		<Publishers>
			<OVT>
				<Port>9000</Port>
			</OVT>
		</Publishers>
	</Bind>
	<VirtualHosts>
    		<VirtualHost>
		    	<Applications>
				<Application>
					...
					<Publishers>
						<OVT />
					</Publishers>
				</Application>		
			</Applications>
		</VirtualHost>
	</VirtualHosts>
</Server>
```

### Edge

The role of the edge is to receive and distribute streams from an origin. You can configure hundreds of Edge to distribute traffic to your players. As a result of testing, a single edge can stream 4-5Gbps traffic by WebRTC based on AWS C5.2XLarge. If you need to stream to thousands of people, you can configure and use multiple edges.

The edge supports OVT and RTSP to pull stream from an origin. In the near future, we will support more protocols. The stream pulled through OVT or RTSP is bypassed without being encoded.&#x20;

{% hint style="warning" %}
In order to re-encode the stream created by OVT and RTSP, the function to put into an existing application will be supported in the future.
{% endhint %}

To run OvenMediaEngine as Edge, you need to add Origins elements to the configuration file as follows:

```markup
<VirtualHosts>
	<VirtualHost>
		<Origins>
			<Properties>
				<NoInputFailoverTimeout>3000</NoInputFailoverTimeout>
				<UnusedStreamDeletionTimeout>60000</UnusedStreamDeletionTimeout>
			</Properties>
			<Origin>
				<Location>/app/stream</Location>
				<Pass>
					<Scheme>ovt</Scheme>
					<Urls><Url>origin.com:9000/app/stream_720p</Url></Urls>
				</Pass>
				<ForwardQueryParams>true</ForwardQueryParams>
			</Origin>
			<Origin>
				<Location>/app/</Location>
				<Pass>
					<Scheme>OVT</Scheme>
					<Urls><Url>origin.com:9000/app/</Url></Urls>
				</Pass>
			</Origin>
			<Origin>
				<Location>/</Location>
				<Pass>
					<Scheme>RTSP</Scheme>
					<Urls><Url>origin2.com:9000/</Url></Urls>
				</Pass>
			</Origin>
		</Origins>
	</VirtualHost>
</VirtualHosts>
```

The `<Origin>`is a rule about where to pull a stream from for what request.&#x20;

The `<Origin>`has the ability to automatically create an application with that name if the application you set in `<Location>` doesn't exist on the server.  If an application exists in the system, a stream will be created in the application.

The automatically created application by `<Origin>` enables all providers but if you create an application yourself, you must enable the provider that matches the setting as follows.

```markup
<VirtualHosts>
	<VirtualHost>
		<Origins>
			<Properties>
				<NoInputFailoverTimeout>3000</NoInputFailoverTimeout>
				<UnusedStreamDeletionTimeout>60000</UnusedStreamDeletionTimeout>
			</Properties>
			<Origin>
				<Location>/this_application/stream</Location>
				<Pass>
					<Scheme>OVT</Scheme>
					<Urls><Url>origin.com:9000/app/stream_720p</Url></Urls>
				</Pass>
				<ForwardQueryParams>true</ForwardQueryParams>
			</Origin>
			<Origin>
				<Location>/this_application/rtsp_stream</Location>
				<Pass>
					<Scheme>RTSP</Scheme>
					<Urls><Url>rtsp.origin.com/145</Url></Urls>
				</Pass>
			</Origin>
		</Origins>
		<Applications>
			<Application>
				<Name>this_application</Name>
				<Type>live</Type>
				<Providers>
					<!-- You have to enable the OVT provider 
					because you used the ovt scheme for configuring Origin. -->
					<OVT />
					<!-- If you set RTSP into Scheme, 
					you have to enable RTSPPull provider -->
					<RTSPPull />
				</Providers>
		
```

#### \<Properties>

<mark style="color:purple;"><mark style="color:blue;">**NoInputFailoverTimeout**<mark style="color:blue;"></mark><mark style="color:purple;">** **</mark><mark style="color:purple;">****</mark>** (default 3000)**

NoInputFailoverTimeout is the time (in milliseconds) to switch to the next URL if there is no input for the set time.

<mark style="color:blue;">**UnusedStreamDeletionTimeout**</mark>** (default 60000)**

UnusedStreamDeletionTimeout is a function that deletes a stream created with OriginMap if there is no viewer for a set amount of time (milliseconds). This helps to save network traffic and system resources for Origin and Edge.

#### \<Origin>

For a detailed description of Origin's elements, see:

<mark style="color:blue;">**Location**</mark>

Origin is already filtered by domain because it belongs to VirtualHost. Therefore, in Location, set App, Stream, and File to match except domain area. If a request matches multiple Origins, the top of them runs.

<mark style="color:blue;">**Pass**</mark>

Pass consists of Scheme and Url.&#x20;

`<Scheme>` is the protocol that will use to pull from the Origin Stream. It currently can be configured as `OVT`or `RTSP`.&#x20;

If the origin server is OvenMediaEngine, you have to set `OVT`into the `<Scheme>`.&#x20;

You can pull the stream from the RTSP server by setting `RTSP`into the`<Scheme>`. In this case, the `<RTSPPull>` provider must be enabled. The application automatically generated by Origin doesn't need to worry because all providers are enabled.

`Urls` is the address of origin stream and can consist of multiple URLs.

`ForwardQueryParams` is an option to determine whether to pass the query string part to the server at the URL you requested to play.(**Default : true**) Some RTSP servers classify streams according to query strings, so you may want this option to be set to false. For example, if a user requests `ws://host:port/app/stream?transport=tcp` to play WebRTC, the `?transport=tcp` may also be forwarded to the RTSP server, so the stream may not be found on the RTSP server. On the other hand, OVT does not affect anything, so you can use it as the default setting.



### Rules for generating Origin URL

The final address to be requested by OvenMediaEngine is generated by combining the configured Url and user's request except for Location. For example, if the following is set

```markup
<Location>/edge_app/</Location>
<Pass>
    <Scheme>ovt</Scheme>
    <Urls><Url>origin.com:9000/origin_app/</Url></Urls>
</Pass>
```

If a user requests [http://edge.com/edge\_app/stream](http://edge.com/edge\_app/stream), OvenMediaEngine makes an address to ovt: //origin.com: 9000/origin\_app/stream.

## Load Balancer

When you are configuring Load Balancer, you need to use third-party solutions such as L4 Switch, LVS, or GSLB, but we recommend using DNS Round Robin. Also, services such as cloud-based [AWS Route53](https://aws.amazon.com/route53/?nc1=h\_ls), [Azure DNS](https://azure.microsoft.com/en-us/services/dns/), or [Google Cloud DNS](https://cloud.google.com/dns/) can be a good alternative.
