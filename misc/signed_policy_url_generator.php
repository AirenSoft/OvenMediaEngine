<?php

$policy_query_key_name = "YOUR POLICY QUERY KEY NAME GOES HERE";
$signature_query_key_name = "YOUR SIGNATURE QUERY KEY NAME GOES HERE";
$hmac_private_key = "YOUR HMAC PRIVATE KEY GOES HERE";

// Takes a GET request consisting of the BASE URL and optionally, a stream policy
// e.g. http://myserver.com/signed_policy_url_generator.php?base_url=rtmp://myServer.com:1935/myApp/myStream&policy={"url_expire":12345678}

if ($_SERVER["REQUEST_METHOD"] == "GET") {

    $base_url = trim($_GET["base_url"]);

    if (!empty(trim($_GET["policy"]))) {
        $policy = trim($_GET["policy"]);
    } else {
        $policy = "{\"url_expire\":7245594130000}";
    }

    function b64UrlEnc($str)
    {
        return str_replace(['+', '/', '='], ['-', '_', ''], base64_encode($str));
    }

    $base64_encoded_policy = b64UrlEnc($policy);
    $hmac_string = $base_url . "?" . $policy_query_key_name . "=" . $base64_encoded_policy;
    $signature = pack('H*', hash_hmac("sha1", $hmac_string, $hmac_private_key));
    $base64_encoded_signature = b64UrlEnc($signature);
    $final_url = $base_url . "?" . $policy_query_key_name . "=" . $base64_encoded_policy . "&" . $signature_query_key_name . "=" . $base64_encoded_signature;

    echo $final_url;
}

?>
