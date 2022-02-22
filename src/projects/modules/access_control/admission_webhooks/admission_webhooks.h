#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/socket_address.h>

class AdmissionWebhooks
{
public:
	enum class ErrCode : uint8_t
	{
		// From control server
		ALLOWED,
		DENIED,
		INVALID_DATA_FORMAT,
		// From http level error
		INVALID_STATUS_CODE, // If HTTP status codes is NOT 200 ok
		// Internal, etc errors
		INTERNAL_ERROR,
	};

	class Status
	{
	public:
		enum class Code : uint8_t
		{
			OPENING,
			CLOSING
		};

		static inline ov::String Description(Code code)
		{
			return _description[static_cast<std::size_t>(code)];
		}

	private:
		static inline const std::vector<ov::String> _description {
			"opening",
			"closing"
		};
	};


	static std::shared_ptr<AdmissionWebhooks> Query(ProviderType provider,
													const std::shared_ptr<ov::Url> &control_server_url, uint32_t timeout_msec,
													const ov::String secret_key,
													const std::shared_ptr<ov::SocketAddress> &client_address,
													const std::shared_ptr<const ov::Url> &request_url,
													const Status::Code status = Status::Code::OPENING);
	static std::shared_ptr<AdmissionWebhooks> Query(PublisherType publisher,
													const std::shared_ptr<ov::Url> &control_server_url, uint32_t timeout_msec,
													const ov::String secret_key,
													const std::shared_ptr<ov::SocketAddress> &client_address,
													const std::shared_ptr<const ov::Url> &request_url,
													const Status::Code status = Status::Code::OPENING);

	ErrCode GetErrCode() const;
	ov::String GetErrReason() const;
	std::shared_ptr<ov::Url> GetNewURL() const;
	uint64_t GetLifetime() const;
	uint64_t GetElpasedTime() const;
	
private:
	void Run();
	ov::String GetMessageBody();
	void SetError(ErrCode code, ov::String reason);

	void ParseResponse(const std::shared_ptr<ov::Data> &data);

	uint64_t _elapsed_ms = 0;

	// Request
	std::shared_ptr<ov::Url> _control_server_url = nullptr;
	uint64_t _timeout_msec = 0;
	ov::String _secret_key;
	ProviderType _provider_type = ProviderType::Unknown;
	PublisherType _publisher_type = PublisherType::Unknown;
	std::shared_ptr<ov::SocketAddress> _client_address;
	std::shared_ptr<const ov::Url> _requested_url;
	Status::Code _status;

	// Response
	bool _allowed = false;
	ErrCode _err_code;
	ov::String _err_reason;
	std::shared_ptr<ov::Url> _new_url = nullptr;
	uint64_t _lifetime = 0;
};
