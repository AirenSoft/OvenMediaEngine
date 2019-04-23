#include "dtls_transport.h"

#include <utility>
#include <algorithm>

#define OV_LOG_TAG              "DTLS"

DtlsTransport::DtlsTransport(uint32_t id, std::shared_ptr<Session> session)
	: SessionNode(id, SessionNodeType::Dtls, std::move(session))
{
	_state = SSL_NONE;
	_peer_cerificate_verified = false;
}

// Set Local Certificate
void DtlsTransport::SetLocalCertificate(const std::shared_ptr<Certificate> &certificate)
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

	ov::TlsCallback callback =
		{
			.create_callback = [](ov::Tls *tls, SSL_CTX *context) -> bool
			{
				tls->SetVerify(SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT);

				// SSL_CTX_set_tlsext_use_srtp() returns 1 on error, 0 on success
				if(SSL_CTX_set_tlsext_use_srtp(context, "SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32"))
				{
					logte("SSL_CTX_set_tlsext_use_srtp failed");
					return false;
				}

				return true;
			},

			.read_callback = std::bind(&DtlsTransport::Read, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			.write_callback = std::bind(&DtlsTransport::Write, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			.destroy_callback = nullptr,
			.ctrl_callback = [](ov::Tls *tls, int cmd, long num, void *ptr) -> long
			{
				switch(cmd)
				{
					case BIO_CTRL_RESET:
					case BIO_CTRL_WPENDING:
					case BIO_CTRL_PENDING:
						return 0;

					case BIO_CTRL_FLUSH:
						return 1;

					default:
						return 0;
				}
			},
			.verify_callback = [](ov::Tls *tls, X509_STORE_CTX *store_context) -> bool
			{
				logtd("SSLVerifyCallback enter");
				return true;
			}
		};

	if(_tls.Initialize(DTLS_server_method(), _local_certificate, nullptr, "DEFAULT:!NULL:!aNULL:!SHA256:!SHA384:!aECDH:!AESGCM+AES256:!aPSK", callback) == false)
	{
		_state = SSL_ERROR;
		return false;
	}

	_state = SSL_CONNECTING;

	// 한번 accept를 시도해본다.
	ContinueSSL();

	return true;
}

bool DtlsTransport::ContinueSSL()
{
	logtd("Continue DTLS...");

	int error = _tls.Accept();

	if(error == SSL_ERROR_NONE)
	{
		_state = SSL_CONNECTED;

		_peer_certificate = _tls.GetPeerCertificate();

		if(_peer_certificate == nullptr)
		{
			return false;
		}

		if(VerifyPeerCertificate() == false)
		{
			// Peer Certificate 검증 실패
		}

		_peer_certificate->Print();

		return MakeSrtpKey();
	}

	return false;
}

bool DtlsTransport::MakeSrtpKey()
{
	if(_peer_cerificate_verified == false)
	{
		return false;
	}

	// https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#exporter-labels
	const ov::String label = "EXTRACTOR-dtls_srtp";

	auto crypto_suite = _tls.GetSelectedSrtpProfileId();

	std::shared_ptr<ov::Data> server_key = std::make_shared<ov::Data>();
	std::shared_ptr<ov::Data> client_key = std::make_shared<ov::Data>();

	_tls.ExportKeyingMaterial(crypto_suite, label, server_key, client_key);

	// Upper Node가 SRTP라면 값을 넘긴다.
	// SRTP를 쓸때는 꼭 SRTP - DTLS 순서로 연결해야 한다.
	auto node = GetUpperNode();

	if(node->GetNodeType() == SessionNodeType::Srtp)
	{
		auto srtp_transport = std::static_pointer_cast<SrtpTransport>(node);

		srtp_transport->SetKeyMeterial(crypto_suite, server_key, client_key);
	}

	return true;
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

				size_t written_bytes;

				int ssl_error = _tls.Write(data->GetData(), data->GetLength(), &written_bytes);

				if(ssl_error == SSL_ERROR_NONE)
				{
					return true;
				}

				logte("SSL_wirte error : error(%d)", ssl_error);
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
					int ssl_error = _tls.Read(buffer, sizeof(buffer), nullptr);

					// 말이 안되는데 여기 들어오는지 보자.
					int pending = _tls.Pending();

					if(pending >= 0)
					{
						logtd("Short DTLS read. Flushing %d bytes", pending);
						_tls.FlushInput();
					}

					// 읽은 패킷을 누군가에게 줘야 하는데... 줄 놈이 없다.
					// TODO: 향후 SCTP 등을 연결하면 준다. 지금은 DTLS로 암호화 된 패킷을 받을 객체가 없다.
					logtd("Unknown dtls packet received (%d)", ssl_error);
				}

				return true;
			}
				// SRTP, SRTCP,
			else
			{
				// 이 서버는 현재 RTP를 받지 않으므로, RTCP가 유일하다.
				// DTLS, RTP(RTCP) 이외의 패킷은 비정상 패킷이다.
				// rtcp에서 검증 처리 있어 주석 처리
				// if(!IsRtpPacket(data))
				// {
				// 	break;
				// }

				// pass to srtp
				auto node = GetUpperNode();

                if(node == nullptr)
                {
                    return false;
                }
                node->OnDataReceived(GetNodeType(), data);

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

ssize_t DtlsTransport::Read(ov::Tls *tls, void *buffer, size_t length)
{
	std::shared_ptr<const ov::Data> data = TakeDtlsPacket();

	if(data == nullptr)
	{
		logtd("SSL read packet block");
		return 0;
	}

	size_t read_len = std::min<size_t>(length, static_cast<size_t>(data->GetLength()));

	logtd("SSL read packet : %zu", read_len);

	::memcpy(buffer, data->GetData(), read_len);

	return read_len;
}

ssize_t DtlsTransport::Write(ov::Tls *tls, const void *data, size_t length)
{
	auto packet = std::make_shared<ov::Data>(data, length);

	auto node = GetLowerNode(SessionNodeType::Ice);

	if(node == nullptr)
	{
		logte("Cannot find lower node (Expected: IcePort)");

		OV_ASSERT2(false);

		return -1;
	}

	logtd("SSL write packet : %zu", packet->GetLength());

	if(node->SendData(GetNodeType(), packet))
	{
		return length;
	}

	// retry
	return 0;
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

