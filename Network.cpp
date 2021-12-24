#include "Network.h"
//#include ClockSyncCommon.h
#include <cstdio>

//Matches enum used in raylib's logging mechanism
typedef enum
{
	LOG_ALL = 0,
	LOG_TRACE,
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR,
	LOG_FATAL,
	LOG_NONE
};

#define NET_LOG(...) printf(__VA_ARGS__);
#define DISABLED_LOG(...)
#define NBN_LogInfo(...) NET_LOG(__VA_ARGS__)
#define NBN_LogError(...) NET_LOG(__VA_ARGS__)
#define NBN_LogDebug(...) NET_LOG(__VA_ARGS__)
#define NBN_LogTrace(...) DISABLED_LOG(__VA_ARGS__)

#include <winsock2.h>
#define NBNET_IMPL
#include "nbnet.h"
#include "net_drivers/udp.h"

#define HOST_PORT 50101
#define PROTOCOL_NAME "clock_sync_test"
static NBN_Connection *client = nullptr;

bool InitalizeHost()
{
	NBN_GameServer_Init(PROTOCOL_NAME, HOST_PORT);

	//Start Server
	if (NBN_GameServer_Start() < 0)
	{
		//NET_LOG("Failed to start the server\n");
		NET_LOG("Failed to start the server\n");

		//Deinit server
		NBN_GameServer_Deinit();

		//Error, quit the server application
		return true;
	}

	NET_LOG("Started Hosting..........\n");
	return false;
}

void InitalizeClient()
{
	NBN_GameClient_Init(PROTOCOL_NAME, "127.0.0.1", HOST_PORT);

	if (NBN_GameClient_Start() < 0)
	{
		NET_LOG("Failed to start client\n");

		//Deinit the client
		NBN_GameClient_Deinit();
		exit(-1);
	}
}

void HandleMessage()
{
	//Get info about the recieved message
	NBN_MessageInfo msg_info = NBN_GameServer_GetMessageInfo();

	//assert checks these conditions
	assert(msg_info.sender == client);
	assert(msg_info.type == NBN_BYTE_ARRAY_MESSAGE_TYPE);

	NBN_ByteArrayMessage *msg = (NBN_ByteArrayMessage*)msg_info.data;
	
	// Everything until Destroy is something to get rid of
	NBN_OutgoingMessage *outgoing_msg = NBN_GameServer_CreateByteArrayMessage(msg->bytes, msg->length);
	assert(outgoing_msg);

	if (NBN_GameServer_SendReliableMessageTo(client, outgoing_msg) < 0)
		exit(-1);

	// Destroy the recieved message
	NBN_ByteArrayMessage_Destroy(msg);

}

NetworkInputPackage ReadInputMessageCommon(const NBN_MessageInfo& msg_info)
{

	//assert checks these conditions
	//assert(msg_info.sender == client);
	assert(msg_info.type == NBN_BYTE_ARRAY_MESSAGE_TYPE);

	NBN_ByteArrayMessage* msg = (NBN_ByteArrayMessage*)msg_info.data;


	NetworkInputPackage InputPackage;
	memcpy(&InputPackage, msg->bytes, sizeof(NetworkInputPackage));

	//NET_LOG("Input Recieved: Input: %d , Frame: %d ", InputPackage.InputCommand, InputPackage.FrameCount);

	// Destroy the recieved message
	NBN_ByteArrayMessage_Destroy(msg);

	return InputPackage;
}

NetworkInputPackage ReadInputMessage()
{
	//Get info about the recieved message
	NBN_MessageInfo msg_info = NBN_GameServer_GetMessageInfo();
	return ReadInputMessageCommon(msg_info);
}

NetworkInputPackage ReadInputMessageClient()
{
	//Get info about the recieved message
	NBN_MessageInfo msg_info = NBN_GameClient_GetMessageInfo();
	return ReadInputMessageCommon(msg_info);

}

//client version
void SendInputMessage(NetworkInputPackage InputPackage)
{
	//create a nbnet outgoing message
	NBN_OutgoingMessage* outgoing_msg = NBN_GameServer_CreateByteArrayMessage((uint8_t*)&InputPackage, sizeof(InputPackage));

	assert(outgoing_msg);

	// Reliably send it to the server
	if (NBN_GameClient_SendUnreliableMessage(outgoing_msg) < 0)
	{
		NET_LOG("Error: Failed to send outgoing message");
		exit(-1);
	}
}

//Host Version
void SendInputMessageHost(NetworkInputPackage InputPackage)
{
	//create a nbnet outgoing message
	NBN_OutgoingMessage *outgoing_msg = NBN_GameServer_CreateByteArrayMessage((uint8_t*)&InputPackage, sizeof(InputPackage));

	assert(outgoing_msg);

	// Reliably send it to the client
	if (NBN_GameServer_SendUnreliableMessageTo(client, outgoing_msg) < 0)
	{
		NET_LOG("Error: Failed to send outgoing message");
		exit(-1);
	}
}

NetworkInputPackage TickNetworkHost(NetworkInputPackage InputPackage, bool& bRecievedInput)
{
	bRecievedInput = false;

	if (client)
	{
		SendInputMessageHost(InputPackage);
	}

	NetworkInputPackage ClientInputPackage{0, 0};

	//Update the server clock
	NBN_GameServer_AddTime(1.0 / 60.0);

	int ev;
	// Poll for client events
	while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT)
	{
		if (ev < 0)
		{
			NET_LOG("Network Error");

			// Error, quit the server appication
			exit(-1);
			break;
		}

		switch (ev)
		{
		//Client is connected to the server
		case NBN_NEW_CONNECTION:

			// Echo server work with one single client at a time
			if (client == nullptr)
			{
				client = NBN_GameServer_GetIncomingConnection();
				NBN_GameServer_AcceptIncomingConnection();
				NET_LOG("Accepting client connection\n");
			}
			break;

		case NBN_CLIENT_MESSAGE_RECEIVED:

			ClientInputPackage = ReadInputMessage();
			bRecievedInput = true;
			break;

		}
	}

	// Pack all enqueued messages as packets and send them
	if (NBN_GameServer_SendPackets() < 0)
	{
		NET_LOG("Failed to send packets\n");

		//Error quit server application
		exit(-1);
	}

	return ClientInputPackage;
}

NetworkInputPackage TickNetworkClient(NetworkInputPackage InputPackage, bool& bRecievedInput)
{
	bRecievedInput = false;
	NetworkInputPackage HostInputPackage = {0, 0};
	SendInputMessage(InputPackage);
	
	//Update the client clock
	NBN_GameClient_AddTime(1.0 / 60.0);

	int ev;
	// Poll for client events
	while ((ev = NBN_GameClient_Poll()) != NBN_NO_EVENT)
	{
		if (ev < 0)
		{
			NET_LOG("An error occured while polling client. Exit\n");
			exit(-1);
		}

		switch (ev)
		{

		//Client is connected to the server
		case NBN_CONNECTED:
				NET_LOG("Connect to host\n");
				break;

		case NBN_MESSAGE_RECEIVED:
			HostInputPackage = ReadInputMessageClient();
			bRecievedInput = true;
			break;
		}
	}

	// Pack all enqueued messages as packets and send them
	if (NBN_GameClient_SendPackets() < 0)
	{
		NET_LOG("Failed to send packets\n");

		//Error quit server application
		exit(-1);
	}

	return HostInputPackage;
}
 
