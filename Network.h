#pragma once

constexpr unsigned int NET_PACKET_INPUT_HISTORY_SIZE = 10; 

struct NetworkInputPackage
{
	unsigned int InputHistory[NET_PACKET_INPUT_HISTORY_SIZE];
	int FrameCount{ 0 };
	// difference of frames
	int FrameDelta{ 0 };
};

bool InitalizeHost();

void InitalizeClient();

bool IsClientConnected();

void HandleMessage();

NetworkInputPackage TickNetworkHost(NetworkInputPackage InputPackage, bool& bRecievedInput);

NetworkInputPackage TickNetworkClient(NetworkInputPackage InputPackage, bool& bRecievedInput);

