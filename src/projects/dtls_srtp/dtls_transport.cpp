#include "dtls_transport.h"
#include "openssl_adapter.h"
#include <algorithm>

#define OV_LOG_TAG              "DTLS"

DtlsTransport::DtlsTransport(uint32_t id, std::shared_ptr<Session> session)
	: SessionNode(id, SessionNodeType::Dtls, session)
{
	_state = SSL_NONE;
	_peer_cerificate_verified = false;
}

DtlsTransport::~DtlsTransport()
{
	if(_ssl_ctx != nullptr)
	{
		SSL_CTX_free(_ssl_ctx);
	}

	if(_ssl != nullptr)
	{
		SSL_free(_ssl);
	}

	_state = SSL_CLOSED;
}

// Set Local Certificate
void DtlsTransport::SetLocalCertificate(std::shared_ptr<Certificate> certificate)
{
	_local_certificate = certificate;
}

// Set Peer Fingerprint for verification
void DtlsTransport::SetPeerFingerprint(ov::String algorithm, ov::String fingerprint)
{
	_peer_fingerprint_algorithm = algorithm;
	_peer_fingerprint_value = fingerprint;
}

// Start DTLS
bool DtlsTransport::StartDTLS()
{
	// Ice 상태가 Completed 일 경우에만 시작한다.
	// Polling 방식이 아니므로 밖에서 이벤트를 확실하게 받고 진입해야 한다.
	/*
	if(_ice_port->GetState() != IcePortConnectionState::Completed)
	{
		_state = SSL_WAIT;
		return false;
	}
	*/

	_ssl_ctx = OpenSslAdapter::SetupSslContext(_local_certificate, SSLVerifyCallback);
	if(_ssl_ctx == nullptr)
	{
		_state = SSL_ERROR;
		return false;
	}

	BIO *bio = OpenSslAdapter::CreateBio(this);
	if(bio == nullptr)
	{
		_state = SSL_ERROR;
		return false;
	}

	_ssl = OpenSslAdapter::CreateSslSession(_ssl_ctx, bio, bio, this);
	if(_ssl == nullptr)
	{
		BIO_free(bio);
		SSL_CTX_free(_ssl_ctx);
		_ssl_ctx = nullptr;
		_state = SSL_ERROR;

		return false;
	}

	_state = SSL_CONNECTING;

	// 한번 accept를 시도해본다.
	ContinueSSL();

	return true;
}

int DtlsTransport::ContinueSSL()
{
	logtd("Continue DTLS...");
	// DTLS Accept
	int code = SSL_accept(_ssl);
	int error = SSL_get_error(_ssl, code);

	if(error == SSL_ERROR_NONE)
	{
		logtd("DTLS Accept!");
		_state = SSL_CONNECTED;

		X509 *cert = SSL_get_peer_certificate(_ssl);
		if(!cert)
		{
			logte("Get peer cert failed");
			return 1;
		}
		_peer_certificate = std::make_shared<Certificate>(cert);
		X509_free(cert);

		if(!VerifyPeerCertificate())
		{
			// Peer Certificate 검증 실패
		}

		_peer_certificate->Print();

		MakeSrtpKey();
	}
	else if(error == SSL_ERROR_WANT_READ)
	{
		logtd("SSL_ERROR_WANT_READ");
		// 잠시 후 다시 Continue SSL을 한다.
		// 그런데 이것을 하는 동안 Thread가 잠기므로, 전체적으로 Lock이 걸린다.
		// 해결책이 필요함
	}
	else if(error == SSL_ERROR_WANT_WRITE)
	{
		logtd("SSL_ERROR_WANT_WRITE");
	}
	else
	{
		logtd("SSL_accept error : (%d) err=%d", code, error);
	}

	return 1;
}

//TODO: 향후 SSL 쪽은 모두 Adapter 쪽으로 옮긴다.
uint64_t DtlsTransport::GetDtlsSrtpCryptoSuite()
{
	SRTP_PROTECTION_PROFILE *srtp_profile = SSL_get_selected_srtp_profile(_ssl);
	if(!srtp_profile)
	{
		return 0;
	}

	return srtp_profile->id;
}

bool DtlsTransport::GetKeySaltLen(uint64_t crypto_suite, uint64_t &key_len, uint64_t &salt_len)
{
	switch(crypto_suite)
	{
		case SRTP_AES128_CM_SHA1_32:
		case SRTP_AES128_CM_SHA1_80:
			// SRTP_AES128_CM_HMAC_SHA1_32 and SRTP_AES128_CM_HMAC_SHA1_80 are defined
			// in RFC 5764 to use a 128 bits key and 112 bits salt for the cipher.
			key_len = 16;
			salt_len = 14;
			break;
		case SRTP_AEAD_AES_128_GCM:
			// SRTP_AEAD_AES_128_GCM is defined in RFC 7714 to use a 128 bits key and
			// a 96 bits salt for the cipher.
			key_len = 16;
			salt_len = 12;
			break;
		case SRTP_AEAD_AES_256_GCM:
			// SRTP_AEAD_AES_256_GCM is defined in RFC 7714 to use a 256 bits key and
			// a 96 bits salt for the cipher.
			key_len = 32;
			salt_len = 12;
			break;
		default:
			return false;
	}
	return true;
}

bool DtlsTransport::MakeSrtpKey()
{
	if(!_peer_cerificate_verified)
	{
		return false;
	}

	uint64_t key_len;
	uint64_t salt_len;
	uint64_t crypto_suite;

	crypto_suite = GetDtlsSrtpCryptoSuite();
	if(!GetKeySaltLen(crypto_suite, key_len, salt_len))
	{
		return false;
	}

	const ov::String label = "EXTRACTOR-dtls_srtp";
	auto key_data = ov::Data::CreateData((key_len + salt_len) * 2);
	key_data->SetLength((key_len + salt_len) * 2);

	auto key_buffer = key_data->GetWritableDataAs<uint8_t>();

	if(SSL_export_keying_material(_ssl, &key_buffer[0], static_cast<size_t>(key_data->GetLength()),
	                              label.CStr(), static_cast<size_t>(label.GetLength()),
	                              nullptr, 0, false) != 1)
	{
		return false;
	}

	auto client_key_data = ov::Data::CreateData(key_len + salt_len);
	auto server_key_data = ov::Data::CreateData(key_len + salt_len);
	// 사이즈 늘림
	client_key_data->SetLength(key_len + salt_len);
	server_key_data->SetLength(key_len + salt_len);
	// 버퍼 얻어옴
	auto client_key_buffer = client_key_data->GetWritableDataAs<uint8_t>();
	auto server_key_buffer = server_key_data->GetWritableDataAs<uint8_t>();

	size_t offset = 0;
	memcpy(&client_key_buffer[0], &key_buffer[offset], key_len);
	offset += key_len;
	memcpy(&server_key_buffer[0], &key_buffer[offset], key_len);
	offset += key_len;
	memcpy(&client_key_buffer[key_len], &key_buffer[offset], salt_len);
	offset += salt_len;
	memcpy(&server_key_buffer[key_len], &key_buffer[offset], salt_len);

	// Upper Node가 SRTP라면 값을 넘긴다.
	// SRTP를 쓸때는 꼭 SRTP - DTLS 순서로 연결해야 한다.
	auto node = GetUpperNode();
	if(node->GetNodeType() == SessionNodeType::Srtp)
	{
		auto srtp_transport = std::static_pointer_cast<SrtpTransport>(node);
		srtp_transport->SetKeyMeterial(crypto_suite, server_key_data, client_key_data);
	}

	return true;
}

int DtlsTransport::SSLVerifyCallback(X509_STORE_CTX *store, void *arg)
{
	logtd("SSLVerifyCallback enter");
	return 1;

}

bool DtlsTransport::VerifyPeerCertificate()
{
	// TODO: PEER CERTIFICATE와 SDP에서 받은 Digest를 비교하여 검증한다.
	logtd("Accepted peer certificate");
	_peer_cerificate_verified = true;
	return true;
}

// 데이터를 upper에서 받는다. lower node로 보낸다.
// Session -> Makes SRTP Packet -> DtlsTransport -> Ice로 전송
bool DtlsTransport::SendData(SessionNodeType from_node, const std::shared_ptr<ov::Data> &data)
{
	// Node 시작 전에는 아무것도 하지 않는다.
	if(GetState() != SessionNode::NodeState::Started)
	{
		logtd("SessionNode has not started, so the received data has been canceled.");
		return false;
	}

	switch(_state)
	{
		case SSL_NONE:
		case SSL_WAIT:
		case SSL_CONNECTING:
			// 연결이 완료되기 전에는 아무것도 하지 않는다.
			break;
		case SSL_CONNECTED:
			// SRTP는 이미 암호화가 되었으므로 ICE로 바로 전송한다.
			if(from_node == SessionNodeType::Srtp)
			{
				auto node = GetLowerNode(SessionNodeType::Ice);
				if(node == nullptr)
				{
					return false;
				}
				//logtd("DtlsTransport Send next node : %d", data->GetLength());
				return node->SendData(GetNodeType(), data);
			}
			else
			{
				// 그 이외의 패킷은 암호화 하여 전송한다.
				// TODO: 현재는 사용할 일이 없고 향후 데이터 채널을 붙이면 다시 검증한다. (현재는 검증 전)
				int code = SSL_write(_ssl, data->GetData(), data->GetLength());
				int ssl_error = SSL_get_error(_ssl, code);
				if(ssl_error == SSL_ERROR_NONE)
				{
					return true;
				}

				logte("SSL_wirte error : code(%d) error(%d)", code, ssl_error);
			}
			break;
		case SSL_ERROR:
		case SSL_CLOSED:
			break;
		default:
			break;
	}

	return false;
}

// 데이터를 lower에서 받는다. upper node로 보낸다.
// IcePort -> Publisher ->[queue] Application {thread}-> Session -> DtlsTransport -> SRTP || SCTP
bool DtlsTransport::OnDataReceived(SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	// Node 시작 전에는 아무것도 하지 않는다.
	if(GetState() != SessionNode::NodeState::Started)
	{
		logtd("SessionNode has not started, so the received data has been canceled.");
		return false;
	}

	logtd("OnDataReceived (%d) bytes", data->GetLength());

	switch(_state)
	{
		case SSL_NONE:
		case SSL_WAIT:
			// SSL이 연결되기 전에는 아무것도 하지 않는다.
			break;
		case SSL_CONNECTING:
		case SSL_CONNECTED:
			// DTLS 패킷인지 검사한다.
			if(IsDtlsPacket(data))
			{
				logtd("Receive DTLS packet");
				// Packet을 Queue에 쌓는다.
				SaveDtlsPacket(data);

				// SSL에 읽어가라고 명령을 내린다.
				if(_state == SSL_CONNECTING)
				{
					// 연결중이면 SSL_accept를 해야 한다.
					ContinueSSL();
				}
				else
				{
					// 연결이 완료된 상태면 암호화를 위해 읽어가야 한다.
					char buffer[MAX_DTLS_PACKET_LEN];

					// SSL_read는 다음과 같이 동작한다.
					// SSL -> Read() -> TakeDtlsPacket() -> Decrypt -> buffer
					int code = SSL_read(_ssl, buffer, sizeof(buffer));
					int ssl_error = SSL_get_error(_ssl, code);

					// 말이 안되는데 여기 들어오는지 보자.
					int pending = SSL_pending(_ssl);
					if(pending >= 0)
					{
						logtd("Short DTLS read. Flushing %d bytes", pending);
						FlushInput(pending);
					}

					// 읽은 패킷을 누군가에게 줘야 하는데... 줄 놈이 없다.
					// TODO: 향후 SCTP 등을 연결하면 준다. 지금은 DTLS로 암호화 된 패킷을 받을 객체가 없다.
					logtd("Unknown dtls packet received (%d:%d)", code, ssl_error);
				}

				return true;
			}
				// SRTP, SRTCP,
			else
			{
				// 이 서버는 현재 RTP를 받지 않으므로, RTCP가 유일하다.
				// DTLS, RTP(RTCP) 이외의 패킷은 비정상 패킷이다.
				if(!IsRtpPacket(data))
				{
					break;
				}

				// TODO: SRTP로 보낸다.
				// _srtp_transport->RecvPacket(session_info, data);
				return true;
			}
			break;
		case SSL_ERROR:
		case SSL_CLOSED:
			break;
		default:
			break;
	}

	return false;
}

// SSL에서 암호화 할 패킷을 읽어갈 때 호출한다.
int DtlsTransport::Read(void *buffer, size_t buffer_len, size_t *read)
{
	std::shared_ptr<const ov::Data> data = TakeDtlsPacket();
	if(data == nullptr)
	{
		logtd("SSL read packet block");
		return 2;
	}

	size_t read_len = std::min<size_t>(buffer_len, static_cast<size_t>(data->GetLength()));
	logtd("SSL read packet : %d", read_len);
	memcpy(buffer, data->GetData(), read_len);
	*read = read_len;

	return 1;
}

// SSL에서 패킷을 전송하기 위해 호출한다. ice로 최종 전송한다.
int DtlsTransport::Write(const void *data, size_t data_len, size_t *written)
{
	auto packet = ov::Data::CreateData(data, data_len);
	auto node = GetLowerNode(SessionNodeType::Ice);
	if(node == nullptr)
	{
		logte("SSL write packet block");
		return 2;
	}

	logtd("SSL write packet : %d", packet->GetLength());
	node->SendData(GetNodeType(), packet);
	*written = data_len;
	return 1;
}

void DtlsTransport::FlushInput(int32_t left)
{
	unsigned char buf[MAX_DTLS_PACKET_LEN];

	while(left)
	{
		// This should always succeed
		int toread = (sizeof(buf) < left) ? sizeof(buf) : left;
		int code = SSL_read(_ssl, buf, toread);

		int ssl_error = SSL_get_error(_ssl, code);

		if(ssl_error != SSL_ERROR_NONE)
		{
			return;
		}

		logtd("Flushed %d bytes", code);
		left -= code;
	}
}


bool DtlsTransport::IsDtlsPacket(const std::shared_ptr<const ov::Data> data)
{
	//TODO: DTLS처럼 보이는 쓰레기가 있는지 더 엄격하게 검사한다.
	uint8_t *buffer = (uint8_t *)data->GetData();
	return (data->GetLength() >= DTLS_RECORD_HEADER_LEN && (buffer[0] > 19 && buffer[0] < 64));
}

bool DtlsTransport::IsRtpPacket(const std::shared_ptr<const ov::Data> data)
{
	uint8_t *buffer = (uint8_t *)data->GetData();
	return (data->GetLength() >= MIN_RTP_PACKET_LEN && (buffer[0] & 0xC0) == 0x80);
}

bool DtlsTransport::SaveDtlsPacket(const std::shared_ptr<const ov::Data> data)
{
	// 이미 하나가 저장되어 있는데 또 저장하려는 것은 잘못된 것이다.
	// 구조상 저장하자마자 사용해야 한다.
	if(_packet_buffer.size() == 1)
	{
		logte("Ssl buffer is full");
		return false;
	}

	_packet_buffer.push_back(data);

	return true;
}

std::shared_ptr<const ov::Data> DtlsTransport::TakeDtlsPacket()
{
	if(_packet_buffer.size() <= 0)
	{
		logtd("Ssl buffer is empty");
		return nullptr;
	}

	std::shared_ptr<const ov::Data> data = _packet_buffer.front();
	_packet_buffer.pop_front();
	return data;
}