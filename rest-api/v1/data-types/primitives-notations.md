# Primitives/Notations

## Primitive data types <a id="primitive-data-types"></a>

| Type | Description | Examples |
| :--- | :--- | :--- |
| Short | 16bits integer | `12345` |
| Int | 32bits integer | `1234941932` |
| Long | 64bits integer | `391859818923919232311` |
| Float | 64bits real number | `3.5483` |
| String | A string | `"Hello"` |
| Bool | true/false | `true` |
| Timestamp \(String\) | A timestamp in ISO8601 format | `"2021-01-01T11:00:00.000+09:00"` |
| TimeInterval \(Long\) | A time interval \(unit: milliseconds\) | `349820` |
| IP \(String\) | IP address | `"127.0.0.1"` |
| RangedPort \(String\) | Port numbers with range \(it can contain multiple ports and protocols\) `start_port[-end_port][,start_port[-end_port][,start_port[-end_port]...]][/protocol]` | `"40000-40005/tcp"` `"40000-40005"` `"40000-40005,10000,20000/tcp"` |
| Port \(String\) | A port number `start_port[/protocol]` | `"1935/tcp"` `"1935"` |

## Enum/Container Notations <a id="enumcontainer-notations"></a>

### Enum&lt;`Type`&gt; \(String\) <a id="enumtype-string"></a>

* An enum value named `Type`
* Examples
  * `"value1"`
  * `"value2"`

### List&lt;`Type`&gt; <a id="listtype"></a>

* An ordered list of `Type`
* Examples
  * `[ Type, Type, ... ]`

### Map&lt;`KeyType`, `ValueType`&gt; <a id="mapkeytype-valuetype"></a>

* An object consisting of `Key`-`Value` pairs
* Examples
  * `{ Key1: Value1, Key2: Value2 }`

