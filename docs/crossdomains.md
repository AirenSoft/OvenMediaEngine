# CrossDomains

Most browsers and players prohibit accessing other domain resources in the currently running domain. You can control this situation through Cross-Origin Resource Sharing (CORS) or Cross-Domain (CrossDomain). You can set CORS and Cross-Domain as `<CrossDomains>` element.

CrossDomain settings are available for HTTP-based APIs, HLS, LLHLS, and Thumnail.

```xml
<CrossDomains>
    <Url>*</Url>
    <Url>*.airensoft.com</Url>
    <Url>http://*.ovenplayer.com</Url>
    <Url>https://demo.ovenplayer.com</Url>
    <Header>
        <Key>Access-Control-Expose-Headers</Key>
        <Value>Date, Server, Content-Type, Content-Length</Value>
    </Header>
    <Header>
        <Key>custom-header</Key>
        <Value>airensoft</Value>
    </Header>
</CrossDomains>
```

You can set it using the `<Url>` element as shown above, and you can use the following values:

| Url Value      | Description                                                   |
| -------------- | ------------------------------------------------------------- |
| \*             | Allows requests from all Domains                              |
| domain         | Allows both HTTP and HTTPS requests from the specified Domain |
| http://domain  | Allows HTTP requests from the specified Domain                |
| https://domain | Allows HTTPS requests from the specified Domain               |

You can respond with custom HTTP headers via `<CrossDomains>`. You can use them by adding multiple `<Header><Key>` and `<Header><Value>` as in the example above.



