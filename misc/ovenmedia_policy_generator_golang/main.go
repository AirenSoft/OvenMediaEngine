package main

import (
	"crypto/hmac"
	"crypto/sha1"
	"encoding/base64"
	"fmt"
	"net/url"
	"os"
	"strings"
)

func main() {
	if len(os.Args) < 5 {
		fmt.Print("Ovenmedia Policy URL Generator - Golang Edition \n\n")
		fmt.Println("Usage: go run main.go [SECRET_KEY] [BASE_URL] [SIGNATURE_QUERY_KEY_NAME] [POLICY_QUERY_KEY_NAME] [POLICY]")
		fmt.Println("Example: go run main.go ome_secret ws://127.0.0.1:3333/app/stream signature policy '{\"url_expire\":1857477520178}'")
		return
	}

	secretKey := os.Args[1]
	baseURL := os.Args[2]
	signatureQueryKeyName := os.Args[3]
	policyQueryKeyName := os.Args[4]
	policy := os.Args[5]

	u, err := url.Parse(baseURL)
	if err != nil {
		fmt.Println("Failed to parse base url")
		return
	}

	encodedPolicy := removeEncodePadding(base64.StdEncoding.EncodeToString([]byte(policy)))
	query := u.Query()
	query.Add(policyQueryKeyName, encodedPolicy)
	u.RawQuery = query.Encode()

	// remove percent encode
	decoded, err := url.QueryUnescape(query.Encode())
	if err != nil {
		fmt.Printf("Failed to decode url")
		return
	}
	u.RawQuery = decoded
	signedSignature := removeEncodePadding(signURL(u.String(), secretKey))
	query.Add(signatureQueryKeyName, signedSignature)
	u.RawQuery = query.Encode()

	// remove percent encode
	decoded, err = url.QueryUnescape(query.Encode())
	if err != nil {
		fmt.Printf("Failed to decode url")
		return
	}
	u.RawQuery = decoded
	fmt.Println(fmt.Sprintf("URL: %s", u.String()))
}

// signs url with secret key
func signURL(url, secretKey string) string {
	h := hmac.New(sha1.New, []byte(secretKey))
	h.Write([]byte(url))
	return base64.RawURLEncoding.EncodeToString(h.Sum(nil))
}

// removes padding
func removeEncodePadding(s string) string {
	return strings.TrimRight(s, "=")
}
