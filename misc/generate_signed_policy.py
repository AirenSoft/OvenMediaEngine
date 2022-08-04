import json
import sys
import hashlib
import hmac
import base64
from datetime import datetime, timedelta
from typing import Optional
from base64 import urlsafe_b64encode

class SignedPolicy:
    def __init__(self, base_url: str, secret_key: str):
        self.secret_key = secret_key
        self.base_url = base_url

    def __make_digest(self, message):
        key = bytes(self.secret_key, "UTF-8")
        message = bytes(message, "UTF-8")

        digester = hmac.new(key, message, hashlib.sha1)
        sig = digester.digest()
        
        return str(urlsafe_b64encode(sig).rstrip(b"="), "UTF-8")

    def generate(self, url_expire: datetime, url_activate: Optional[datetime] = None, stream_expire: Optional[datetime] = None, allow_ip: Optional[str] = None) -> dict:
        policy_dict = {
            "url_expire": int(url_expire.timestamp() * 1000)
        }

        if url_activate:
            policy_dict["url_activate"] = url_activate
        if stream_expire:
            policy_dict["stream_expire"] = int(stream_expire.timestamp() * 1000)
        if allow_ip:
            policy_dict["allow_ip"] = allow_ip
        
        policy_base64 = urlsafe_b64encode(json.dumps(policy_dict).encode("utf-8")).rstrip(b"=")
        stream_url = f"{self.base_url}?policy={policy_base64.decode('utf-8')}"
        sig = self.__make_digest(stream_url)

        return f"{stream_url}&signature={sig}"

if __name__ == "__main__":
    HELP = "Usage: signed_policy.py HMAC_KEY BASE_URL EXPIRE_IN (in hours)"
    EXAMPLE = "Example: signed_policy.py 09243c09v0394 wss://edge01.x-net.at/app/stream 3"

    try:
        KEY = sys.argv[1]
        BASE_URL = sys.argv[2]
        EXPIRE_IN = sys.argv[3]
    except IndexError:
        print(HELP)
        print(EXAMPLE)
        exit(0)
    
    if not (KEY and BASE_URL and EXPIRE_IN):
        print(HELP)
        print(EXAMPLE)
        exit(0)
    
    exp = datetime.now() + timedelta(hours=int(EXPIRE_IN))

    manager = SignedPolicy(BASE_URL, KEY)
    print(manager.generate(exp))
