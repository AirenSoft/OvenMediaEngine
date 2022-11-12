package kr.hvy;

import com.google.gson.Gson;
import org.apache.commons.codec.digest.HmacAlgorithms;
import org.apache.commons.codec.digest.HmacUtils;
import org.apache.commons.lang3.StringUtils;

import java.io.UnsupportedEncodingException;
import java.sql.Timestamp;
import java.text.MessageFormat;
import java.time.Instant;
import java.util.Base64;
import java.util.HashMap;
import java.util.Map;

public class OvenMediaEngineKeygen {
    private static final String SECRET_KEY = "ome_is_the_best";
    private static final String BASE_RTMP_URL = "rtmp://localhost:1935/app/stream";
    private static final String BASE_WS_URL = "ws://localhost:3333/app/stream";

    private static final String POLICY_NAME = "policy";
    private static final String SIGNATURE_NAME = "signature";

    private static final HmacUtils hmacUtils = new HmacUtils(HmacAlgorithms.HMAC_SHA_1, SECRET_KEY);
    private static final Gson gson = new Gson();
    private static final Base64.Encoder bEncode = java.util.Base64.getEncoder();

    public static void main(String[] args) throws UnsupportedEncodingException {
        // usage sample
        String rtmpUrl = generate(BASE_RTMP_URL,
                Timestamp.from(Instant.now().plusSeconds(100)).getTime(),
                Timestamp.from(Instant.now().plusSeconds(0)).getTime(),
                Timestamp.from(Instant.now().plusSeconds(5 * 60)).getTime(),
                null);
        System.out.println(rtmpUrl);

        String wsUrl = generate(BASE_WS_URL,
                Timestamp.from(Instant.now().plusSeconds(20)).getTime(),
                Timestamp.from(Instant.now().plusSeconds(0)).getTime(),
                null,
                null);
        System.out.println(wsUrl);
    }

    /**
     * To use the code below, you need to add a dependency.
     *
     * <pre>
     *      {@code
     *     <dependency>
     *         <groupId>org.apache.commons</groupId>
     *         <artifactId>commons-lang3</artifactId>
     *         <version>3.12.0</version>
     *     </dependency>
     *     <dependency>
     *         <groupId>commons-codec</groupId>
     *         <artifactId>commons-codec</artifactId>
     *         <version>1.15</version>
     *     </dependency>
     *     <dependency>
     *         <groupId>com.google.code.gson</groupId>
     *         <artifactId>gson</artifactId>
     *         <version>2.10</version>
     *     </dependency>
     *     }
     *  </pre>
     * <p>
     * Usage example main function
     * </pre>
     */
    public static String generate(String rtmpUrl, Long url_expire, Long url_activate, Long stream_expire, String allow_ip) throws UnsupportedEncodingException {

        Map<String, Object> policyMap = new HashMap<String, Object>();
        policyMap.put("url_expire", url_expire);

        if (url_activate != null)
            policyMap.put("url_activate", url_activate);
        if (stream_expire != null)
            policyMap.put("stream_expire", stream_expire);
        if (StringUtils.isNotBlank(allow_ip))
            policyMap.put("allow_ip", allow_ip);

        String policy = gson.toJson(policyMap);
        String encodePolicy = encode(policy);

        String streamUrl = MessageFormat.format("{0}?{1}={2}", rtmpUrl, POLICY_NAME, encodePolicy);
        String signature = makeDigest(streamUrl);

        return MessageFormat.format("{0}&{1}={2}", streamUrl, SIGNATURE_NAME, signature);
    }

    private static String encode(String message) throws UnsupportedEncodingException {
        return encode(message.getBytes("UTF-8"));
    }

    private static String encode(byte[] message) {
        String enc = bEncode.encodeToString(message);
        return StringUtils.stripEnd(enc, "=").replaceAll("\\+", "-").replaceAll("/", "_");
    }

    private static String makeDigest(String message) {
        return encode(hmacUtils.hmac(message));
    }
}
