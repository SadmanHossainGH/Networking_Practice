#pragma once

struct NetworkInputPackage
{
	unsigned int InputCommand{ 0 };
	int FrameCount{ 0 };
};

bool InitalizeHost();

void InitalizeClient();

void HandleMessage();

NetworkInputPackage TickNetworkHost(NetworkInputPackage InputPackage, bool& bRecievedInput);

NetworkInputPackage TickNetworkClient(NetworkInputPackage InputPackage, bool& bRecievedInput);

