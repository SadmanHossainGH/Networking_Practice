
#include <cstdio>
#include "Network.h"
//#include ClockSyncCommon.h


//Matches enum used in raylib's logging mechanism
// enum in this case are variables, with no strict datatype
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

// Log Trace marcos
#define NET_LOG(...) printf(__VA_ARGS__);
#define DISABLED_LOG(...)
#define NBN_LogInfo(...) NET_LOG(__VA_ARGS__)
#define NBN_LogError(...) NET_LOG(__VA_ARGS__)
#define NBN_LogDebug(...) NET_LOG(__VA_ARGS__)
#define NBN_LogTrace(...) DISABLED_LOG(__VA_ARGS__)

// networking libraries
#define NBNET_IMPL
#include <winsock2.h>
#include "nbnet.h"
#include "net_drivers/udp.h"

#define HOST_PORT 50101
#define PROTOCOL_NAME "clock_sync_test"

// client connection
static NBN_Connection *client = nullptr;
bool client_connected = false;

// Start instance as Host
bool InitalizeHost()
{
	NBN_GameServer_Init(PROTOCOL_NAME, HOST_PORT);

	//Start Server
	if (NBN_GameServer_Start() < 0)
	{
		NET_LOG("Failed to start the server\n");

		//Deinit server
		NBN_GameServer_Deinit();

		//Error, quit the server application
		return true;
	}

	NET_LOG("? \n");
	NET_LOG("Started Hosting..........\n");
	return false;
}

// Start instance as Client
void InitalizeClient()
{
	NBN_GameClient_Init(PROTOCOL_NAME, "127.0.0.1", HOST_PORT);

	NET_LOG("? \n");
	NET_LOG("FAILED \n");

	if (NBN_GameClient_Start() < 0)
	{
		NET_LOG("Failed to start client\n");

		//Deinit the client
		NBN_GameClient_Deinit();
		exit(-1);
	}
}

bool IsClientConnected()
{
	return client_connected;
}

// decodes and encodes message to send
// might not need to be used, might be used to
// test if both sides can get a message
void HandleMessage()
{
	//Host gets info about the recieved message
	NBN_MessageInfo msg_info = NBN_GameServer_GetMessageInfo();

	//assert checks these conditions
	assert(msg_info.sender == client);
	assert(msg_info.type == NBN_BYTE_ARRAY_MESSAGE_TYPE);

	// Get byte representation of recieved message
	NBN_ByteArrayMessage *msg = (NBN_ByteArrayMessage*)msg_info.data;
	
	// Create message to send based on recieved message
	NBN_OutgoingMessage *outgoing_msg = NBN_GameServer_CreateByteArrayMessage(msg->bytes, msg->length);
	assert(outgoing_msg);

	// Exit if sending fails
	// Send message to client
	if (NBN_GameServer_SendReliableMessageTo(client, outgoing_msg) < 0)
		exit(-1);

	// Destroy the recieved message
	NBN_ByteArrayMessage_Destroy(msg);

}

// copy the message into an inputPacakge regardless of NetworkState
NetworkInputPackage ReadInputMessageCommon(const NBN_MessageInfo& msg_info)
{

	//assert checks these conditions
	//assert(msg_info.sender == client);
	assert(msg_info.type == NBN_BYTE_ARRAY_MESSAGE_TYPE);

	//translate message info into bytes
	NBN_ByteArrayMessage* msg = (NBN_ByteArrayMessage*)msg_info.data;


	// copy the message bytes into inputPackage
	NetworkInputPackage InputPackage;
	memcpy(&InputPackage, msg->bytes, sizeof(NetworkInputPackage));

	//NET_LOG("Input Recieved: Input: %d , Frame: %d  \n", InputPackage.InputHistory[9], InputPackage.FrameCount);

	// Destroy the recieved message
	NBN_ByteArrayMessage_Destroy(msg);

	return InputPackage;
}

// host reads message from client probably
NetworkInputPackage ReadInputMessage()
{
	//Get info about the recieved message
	NBN_MessageInfo msg_info = NBN_GameServer_GetMessageInfo();
	return ReadInputMessageCommon(msg_info);
}

// client reads message from host?
NetworkInputPackage ReadInputMessageClient()
{
	//Get info about the recieved message
	NBN_MessageInfo msg_info = NBN_GameClient_GetMessageInfo();
	return ReadInputMessageCommon(msg_info);

}

//client sends input package to host
// check client to see potitental issues
void SendInputMessage(NetworkInputPackage InputPackage)
{
	//create a nbnet outgoing message
	//NBN_OutgoingMessage* outgoing_msg = NBN_GameServer_CreateByteArrayMessage((uint8_t*)&InputPackage, sizeof(InputPackage));
	NBN_OutgoingMessage* outgoing_msg = NBN_GameClient_CreateByteArrayMessage((uint8_t*)&InputPackage, sizeof(InputPackage));

	assert(outgoing_msg);

	// Reliably send it to the server
	if (NBN_GameClient_SendUnreliableMessage(outgoing_msg) < 0)
	{
		NET_LOG("Error: Failed to send outgoing message");
		exit(-1);
	}
}

//Host send input package to client
void SendInputMessageHost(NetworkInputPackage InputPackage)
{
	//host creates a nbnet outgoing message
	NBN_OutgoingMessage *outgoing_msg = NBN_GameServer_CreateByteArrayMessage((uint8_t *)&InputPackage, sizeof(InputPackage));

	assert(outgoing_msg);

	// Reliably send it to the client
	if (NBN_GameServer_SendUnreliableMessageTo(client, outgoing_msg) < 0)
	{
		NET_LOG("Error: Failed to send outgoing message");
		exit(-1);
	}
}

// maybe the CPU clock ticks the host
NetworkInputPackage TickNetworkHost(NetworkInputPackage InputPackage, bool& bRecievedInput)
{
	bRecievedInput = false;

	// get the host to send message to client
	// CHECK!!!
	if (client)
	{
		for (int i = 0; i < 5; i++)
		{
			//Host sends input package to client
			SendInputMessageHost(InputPackage);
		}
	}

	NetworkInputPackage ClientInputPackage{0, 0};

	//Update the server clock
	NBN_GameServer_AddTime(1.0 / 60.0);

	//event variable
	int ev;

	// Poll inputs for client events
	// while an event occurs
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
		//Client connected to the Host
		case NBN_NEW_CONNECTION:

			// Echo server work with one single client at a time
			// Accept the client if not null
			if (client == nullptr)
			{
				client = NBN_GameServer_GetIncomingConnection();
				NBN_GameServer_AcceptIncomingConnection();
				client_connected = true;
				NET_LOG("Accepting client connection\n");
			}
			break;

		// recived message from client
		case NBN_CLIENT_MESSAGE_RECEIVED:

			// read input from client and indicate it was recivied 
			ClientInputPackage = ReadInputMessage();
			bRecievedInput = true;
			break;

		}
	}

	// Pack all enqueued messages as packets and send them
	// send package to clients
	if (NBN_GameServer_SendPackets() < 0)
	{
		NET_LOG("Failed to send packets \n");

		//Error quit server application
		exit(-1);
	}

	return ClientInputPackage;
}


// this is the network tick that the client uses??
// maybe the CPU clock ticks the client??
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

		// Host is connected to client
		case NBN_CONNECTED:
				NET_LOG("Connect to host\n");
				break;
		
		// read input for client??
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
 
