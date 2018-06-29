#pragma once

#include <base/publisher/session_node.h>
#include <base/ovcrypto/certificate.h>
#include <base/common_types.h>

#include "ice/ice_port.h"
#include "srtp_transport.h"

#define DTLS_RECORD_HEADER_LEN  		13
#define MAX_DTLS_PACKET_LEN  			2048
#define MIN_RTP_PACKET_LEN 				12

class DtlsTransport : public SessionNode
{
public:
	// Send : Srtp -> this -> Ice
	// Recv : Ice -> {[Queue] -> Application -> Session} -> this -> Srtp
    explicit DtlsTransport(uint32_t id, std::shared_ptr<Session> session);
	virtual ~DtlsTransport();

    // Set Local Certificate
	void 	SetLocalCertificate(std::shared_ptr<Certificate> certificate);

	// Set Peer Fingerprint for verification
	void 	SetPeerFingerprint(ov::String algorithm, ov::String fingerprint);

	// Start DTLS
	bool 	StartDTLS();

	// Implement SessionNode Interface
	// 데이터를 upper에서 받는다. lower node로 보낸다.
	bool 	SendData(SessionNodeType from_node, const std::shared_ptr<ov::Data> &data);
	// 데이터를 lower에서 받는다. upper node로 보낸다.
	bool 	OnDataReceived(SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data);

    // IcePort -> Publisher ->[queue] Application {thread}-> Session -> DtlsTransport -> SRTP -> RTP/RTCP
	// ICE에서는 STUN을 제외한 모든 패킷을 위로 올린다.
	// DTLS에서는 패킷을 받으면 DTLS인 경우 패킷을 버퍼에 쌓고(_dtls_packet_buffer) SSL_read를 호출하여 읽어서
	// 복호화를 한 후 다음 Layer로 전송한다.
	// DTLS가 아니고 RTP/RTCP인 경우는 SRTP로 보낸다.
	// 그 외에는 모르는 패킷이므로 처리하지 않는다.
	bool 	RecvPacket(const std::shared_ptr<ov::Data> &data);

	// SSL에서 암호화 할 패킷을 읽어갈 때 호출한다. _packet_buffer에 쌓인 패킷을 준다.
	int 	Read(void* buffer, size_t buffer_len, size_t* read);
	// SSL에서 복호화한 패킷의 전송을 요청할 때 호출한다. ice로 최종 전송한다.
	int 	Write(const void* data, size_t data_len, size_t* written);

	// SSL에서 accept 하면 호출해주고, 여기서 peer certificate를 읽어서 보관한다.
	static int 	SSLVerifyCallback(X509_STORE_CTX* store, void* arg);

	bool 	VerifyPeerCertificate();

private:
	int 	ContinueSSL();
	void 	FlushInput(int32_t left);
	bool 	IsDtlsPacket(const std::shared_ptr<const ov::Data> data);
	bool	IsRtpPacket(const std::shared_ptr<const ov::Data> data);
	bool 	SaveDtlsPacket(const std::shared_ptr<const ov::Data> data);
	std::shared_ptr<const ov::Data> TakeDtlsPacket();

	bool		MakeSrtpKey();
	uint64_t 	GetDtlsSrtpCryptoSuite();
	bool		GetKeySaltLen(uint64_t crypto_suite, uint64_t &key_len, uint64_t &salt_len);

	enum SSLState
	{
		SSL_NONE,
		SSL_WAIT,
		SSL_CONNECTING,
		SSL_CONNECTED,
		SSL_ERROR,
		SSL_CLOSED
	};

	SSLState 							_state;
	bool								_peer_cerificate_verified;
	std::shared_ptr<SessionInfo> 		_session_info;
	std::shared_ptr<IcePort>			_ice_port;
	std::shared_ptr<SrtpTransport>		_srtp_transport;
	std::shared_ptr<Certificate>		_local_certificate;
	std::shared_ptr<Certificate>		_peer_certificate;
	ov::String							_peer_fingerprint_algorithm;
	ov::String							_peer_fingerprint_value;

	// SSL이 가져갈 패킷을 임시로 보관하는 버퍼, 동시에 1개만 저장한다.
	std::deque<std::shared_ptr<const ov::Data>>	_packet_buffer;

	SSL* 								_ssl;
	SSL_CTX* 							_ssl_ctx;

};
