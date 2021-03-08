#include "stun_client.h"
#include "stun/stun_message.h"
#include "stun/attributes/stun_xor_mapped_address_attribute.h"

#define OV_LOG_TAG "StunClient"

bool StunClient::GetMappedAddress(const ov::SocketAddress &stun_server, ov::SocketAddress &mapped_address)
{
	StunMessage message;

	message.SetClass(StunClass::Request);
	message.SetMethod(StunMethod::Binding);

	uint8_t transaction_id[OV_STUN_TRANSACTION_ID_LENGTH];
	uint8_t charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	// generate transaction id ramdomly
	for (int index = 0; index < OV_STUN_TRANSACTION_ID_LENGTH; index++)
	{
		transaction_id[index] = charset[rand() % OV_COUNTOF(charset)];
	}
	message.SetTransactionId(&(transaction_id[0]));

	auto send_data = message.Serialize();
	
	auto client = ov::SocketPool::GetUdpPool()->AllocSocket();
	timeval tv = {1, 0};

	client->SetRecvTimeout(tv);
	if(client->SendTo(stun_server, send_data) == false)
	{
		client->Close();
		return false;
	}

	auto recv_data = std::make_shared<ov::Data>(1500);
	auto result = client->RecvFrom(recv_data, nullptr);
	client->Close();

	if(recv_data->GetLength() <= 0)
	{
		return false;
	}

	ov::ByteStream recv_stream(recv_data.get());
	StunMessage recv_message;

	if (recv_message.Parse(recv_stream) == false)
	{
		logte("Could not parse STUN packet");
		return false;
	}

	auto xor_mapped_address_attr = recv_message.GetAttribute<StunXorMappedAddressAttribute>(StunAttributeType::XorMappedAddress);
	if(xor_mapped_address_attr == nullptr)
	{
		logte("Couldn't get XorMappedAdress attribute");
		return false;
	}

	mapped_address = xor_mapped_address_attr->GetAddress();

	return true;
}