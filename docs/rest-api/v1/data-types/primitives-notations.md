# Primitives/Notations

## Primitive data types <a href="primitive-data-types" id="primitive-data-types"></a>

| Type                          | Description                                                                                                                                                                      | Examples                                                                                                          |
| ----------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| Short                         | 16bits integer                                                                                                                                                                   | `12345`                                                                                                           |
| Int                           | 32bits integer                                                                                                                                                                   | `1234941932`                                                                                                      |
| Long                          | 64bits integer                                                                                                                                                                   | `391859818923919232311`                                                                                           |
| Float                         | 64bits real number                                                                                                                                                               | `3.5483`                                                                                                          |
| String                        | A string                                                                                                                                                                         | `"Hello"`                                                                                                         |
| Bool                          | true/false                                                                                                                                                                       | `true`                                                                                                            |
| <p>Timestamp<br>(String)</p>  | A timestamp in ISO8601 format                                                                                                                                                    | `"2021-01-01T11:00:00.000+09:00"`                                                                                 |
| <p>TimeInterval<br>(Long)</p> | A time interval (unit: milliseconds)                                                                                                                                             | `349820`                                                                                                          |
| <p>IP<br>(String)</p>         | IP address                                                                                                                                                                       | `"127.0.0.1"`                                                                                                     |
| <p>RangedPort<br>(String)</p> | <p>Port numbers with range (it can contain multiple ports and protocols)<br><code>start_port[-end_port][,start_port[-end_port][,start_port[-end_port]...]][/protocol]</code></p> | <p><code>"40000-40005/tcp"</code><br><code>"40000-40005"</code><br><code>"40000-40005,10000,20000/tcp"</code></p> |
| <p>Port<br>(String)</p>       | <p>A port number<br><code>start_port[/protocol]</code></p>                                                                                                                       | <p><code>"1935/tcp"</code><br><code>"1935"</code></p>                                                             |

## Enum/Container Notations <a href="enumcontainer-notations" id="enumcontainer-notations"></a>

### Enum<`Type`> (String) <a href="enumtype-string" id="enumtype-string"></a>

* An enum value named `Type`
* Examples
  * `"value1"`
  * `"value2"`

### List<`Type`> <a href="listtype" id="listtype"></a>

* An ordered list of `Type`
* Examples
  * `[ Type, Type, ... ]`

### Map<`KeyType`, `ValueType`> <a href="mapkeytype-valuetype" id="mapkeytype-valuetype"></a>

* An object consisting of `Key`-`Value` pairs
* Examples
  * `{ Key1: Value1, Key2: Value2 }`
