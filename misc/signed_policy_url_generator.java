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
    private static final String SECRET_KEY = "motolies.video#@#";
    private static final String BASE_RTMP_URL = "rtmp://localhost:1935/app/motolies";
    private static final String BASE_WS_URL = "ws://localhost:3333/app/motolies";

    private static final HmacUtils hmacUtils = new HmacUtils(HmacAlgorithms.HMAC_SHA_1, SECRET_KEY);
    private static final Gson gson = new Gson();
    private static final Base64.Encoder bEncode = java.util.Base64.getEncoder();

    public static void main(String[] args) throws UnsupportedEncodingException {
        // 방송 시작에 대한 시간도 넣어주어야 한다.
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

    public static String generate(String rtmpUrl, Long url_expire, Long url_activate, Long stream_expire, String allow_ip) throws UnsupportedEncodingException {

        // 파라미터 생성
        Map<String, Object> policyMap = new HashMap<String, Object>();

        if (url_expire != null)
            policyMap.put("url_expire", url_expire);
        if (url_activate != null)
            policyMap.put("url_activate", url_activate);
        if (stream_expire != null)
            policyMap.put("stream_expire", stream_expire);
        if (StringUtils.isNotBlank(allow_ip))
            policyMap.put("allow_ip", allow_ip);

        if (policyMap.size() < 1)
            throw new RuntimeException("At least one parameter is required.");


        String policy = gson.toJson(policyMap);
        String encodePolicy = encode(policy);

        String streamUrl = MessageFormat.format("{0}?policy={1}", rtmpUrl, encodePolicy);
        String signature = makeDigest(streamUrl);

        return streamUrl + "&signature=" + signature;
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
