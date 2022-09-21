# Usage

```
go run main.go [SECRET_KEY] [BASE_URL] [SIGNATURE_QUERY_KEY_NAME] [POLICY_QUERY_KEY_NAME] [POLICY]
```

# Example

```
go run main.go test 'wss://127.0.0.1:3334/app/test' signature policy '{"url_expire":1890662400000,"stream_expire":1890662400000}'
URL: wss://127.0.0.1:3334/app/test?policy=eyJ1cmxfZXhwaXJlIjoxODkwNjYyNDAwMDAwLCJzdHJlYW1fZXhwaXJlIjoxODkwNjYyNDAwMDAwfQ&signature=AfqJ-KHLfR93e2zzjDCpOiuiXwo
```