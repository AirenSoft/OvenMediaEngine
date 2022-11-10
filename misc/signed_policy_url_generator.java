import org.apache.commons.codec.digest.HmacAlgorithms;
import org.apache.commons.codec.digest.HmacUtils;
import org.apache.commons.lang3.StringUtils;

import java.io.UnsupportedEncodingException;
import java.text.MessageFormat;
import java.util.Base64;

public class OvenMediaEngineKeygen {
    private static final Base64.Encoder bEncode = java.util.Base64.getEncoder();

    /**
     *  To use the code below, you need to add a dependency.
     *
     *  <pre>
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
     *     }
     *  </pre>
     *
     *  Arguments must be added when executing the main function.
     *
     *  [HMAC_KEY] [BASE_URL] [SIGNATURE_QUERY_KEY_NAME] [POLICY_QUERY_KEY_NAME] [POLICY]
     *
     *  Usage example
     *  <pre>
     *      {@code
     *      ome_is_the_best ws://localhost:3333/app/stream signature policy "{\"url_expire\":1668234317000}"
     *      }
     *  </pre>
     */
    public static void main(String[] args) throws UnsupportedEncodingException {
        String encodePolicy = encode(args[4]);
        String streamUrl = MessageFormat.format("{0}?{1}={2}", args[1], args[3], encodePolicy);
        String signature = encode(new HmacUtils(HmacAlgorithms.HMAC_SHA_1, args[0]).hmac(streamUrl));
        String signedUrl = MessageFormat.format("{0}&{1}={2}", streamUrl, args[2], signature);
        System.out.println(signedUrl);
    }

    private static String encode(String message) throws UnsupportedEncodingException {
        return encode(message.getBytes("UTF-8"));
    }

    private static String encode(byte[] message) {
        String enc = bEncode.encodeToString(message);
        return StringUtils.stripEnd(enc, "=").replaceAll("\\+", "-").replaceAll("/", "_");
    }

}
