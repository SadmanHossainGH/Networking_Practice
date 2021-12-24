#include "raylib.h"
#include <cmath>
#include <cstdio>
#include <iostream>
#include "Source.h"
#include "Network.h"

enum class NetworkState
{
    None,
    Host,
    Client
};

//Boot or Bool?
enum class BootNetworkSetting
{
    NoConnection,
    Host,
    Client
};

// Stores the entire input state for a given frame and player
enum class InputCommand : unsigned int
{
    // powers of 2 to do bitwise OR operation
    None = 0,
    Up = 1,
    Down = 2,
    Left = 4,
    Right = 8,

};

struct Vector2i
{
    int X {0};
    int Y {0};

    void operator+=(const Vector2i& Other)
    {
        this->X += Other.X;
        this->Y += Other.Y;
    }

    void operator-=(const Vector2i& Other)
    {
        this->X -= Other.X;
        this->Y -= Other.Y;
    }
};

Vector2i operator+(const Vector2i& A, const Vector2i& B)
{
    return Vector2i(A.X + B.X, A.Y + B.Y);
}

Vector2i operator-(const Vector2i& A, const Vector2i& B)
{
    return Vector2i(A.X + B.X, A.Y + B.Y);
}

/** complete game state for an entity in the game
    use deterministic lockstep to synchronize our gamestate
    across the network**/
struct EntityState
{
    Vector2i Position;
    Vector2i Velocity;
};

/** Complete Game State that represent our players**/
struct SimulationState
{
    EntityState Entities[2];
    unsigned int Inputs[2];
    int FrameCount = { 0 };
};



int PingToTicks(int PinginMS)
{
    return PinginMS/16.667f;
}

const int SCREEN_WIDTH = 1080;
const int SCREEN_HEIGHT = 768;

//Configurable properties for entity
namespace EntitySettings
{
    const int CircleRadius { 50 };
    const int StartDistance { 400 };
    const int StartHeight { 100 };
    const int MoveSpeed { 4 };
}

void DrawEntity(const EntityState& State, Color color)
{
    DrawCircle(/* X */ State.Position.X + (SCREEN_WIDTH/2) , /* Y */SCREEN_HEIGHT - State.Position.Y,/*Radius*/ 100, color);
}

// 1:34:20-ish should give a good look at most of network.cpp
// 1:37 - ish should give an idea of Network.h
// 1:48:57 should give an glimpse of the entirety of Network.cpp

// Update Movement
void UpdateEntity(unsigned int Input, EntityState& Entity)
{
    // Reset Velocity Each Frame so Entity doesn't keep moving 
    Entity.Velocity.X = 0;
    Entity.Velocity.Y = 0;

    // Input : If input was made, maybe from player?
    // Entity: Player Entity that pushed input
    {
        // Horizontal Control
        if (Input & static_cast<unsigned int>(InputCommand::Left))
        {
            Entity.Velocity.X = -EntitySettings::MoveSpeed;
        }
        else if (Input & static_cast<unsigned int>(InputCommand::Right))
        {
            Entity.Velocity.X = EntitySettings::MoveSpeed;
        }

        // Vertical Control
        if (Input & static_cast<unsigned int>(InputCommand::Up))
        {
            Entity.Velocity.Y = EntitySettings::MoveSpeed;
        }
        else if (Input & static_cast<unsigned int>(InputCommand::Down))
        {
            Entity.Velocity.Y = -EntitySettings::MoveSpeed;
        }
    }

    // Handle Physics
    Entity.Position += Entity.Velocity;

}

void UpdateSimulation(SimulationState& GameState)
{
    UpdateEntity(GameState.Inputs[0], GameState.Entities[0]);
    UpdateEntity(GameState.Inputs[1], GameState.Entities[1]);

}

int main(/*void*/ int argc, char** argv)
{
    //Default Window Title
    const char* WindowTitle = "Clock Sync Test";
    BootNetworkSetting NetworkModeSetting = BootNetworkSetting::NoConnection;

    if (argc >= 2)
    {
        if (argv[1][0] == 'c')
        {
            NetworkModeSetting = BootNetworkSetting::Client;
        }
        else if (argv[1][0] == 's')
        {
            NetworkModeSetting = BootNetworkSetting::Client;
        }
    }

    // Network initally off
    NetworkState NetState = NetworkState::None;

    // Assuming Left Side
    int OpponentSide = 1;

    // latest frame counter recieved from other player
    int NetworkFrame = 0;

    if (NetworkModeSetting == BootNetworkSetting::Host)
    {
        InitalizeHost();
        OpponentSide = 1;
        NetState = NetworkState::Host;
        WindowTitle = "Clock Sync Test P1 : Host";
    }
    else if (NetworkModeSetting == BootNetworkSetting::Client)
    {
        InitalizeClient();
        NetState = NetworkState::Client;
        OpponentSide = 0;
        WindowTitle = "Clock Sync Test P2 : Client";
    }


    //Look to see what needs to be added for this
    //SetTraceLogCallback(LogCustom);

    InitWindow(1080, 758, WindowTitle);
    SetTargetFPS(60);

    SimulationState GameState;

    // Initial Positions for both players to stay in sync between clients
    GameState.Entities[0].Position.X = EntitySettings::StartDistance / 2;
    GameState.Entities[0].Position.Y = EntitySettings::StartHeight;

    GameState.Entities[1].Position.X = EntitySettings :: StartDistance/2;
    GameState.Entities[1].Position.Y = EntitySettings::StartHeight;

    GameState.Inputs[0] = static_cast<unsigned int>(InputCommand::None);
    GameState.Inputs[1] = static_cast<unsigned int>(InputCommand::None);

    // Network Input Buffer
    unsigned int NetInputBuffer = 0;

    // Recived Input Histroy
    const int INPUT_HISTORY_COUNT = 10;
    NetworkInputPackage NetworkInputHistory[INPUT_HISTORY_COUNT];
    int InputHistoryIndex = 0;

    // Main Game Loop
    while (!WindowShouldClose())
    {

        GameState.Inputs[0] = 0;
        GameState.Inputs[1] = 0;

        // Clear Game Input Each Frame
        GameState.Inputs[0] = 0;

        // Only check for starting host/client when not initialized already
        if (NetState == NetworkState::None)
        {
            if (IsKeyDown(KEY_H))
            {
                //Initalize Host
                InitalizeHost();
                NetState = NetworkState::Host;
                OpponentSide = 1;

            }
             else if (IsKeyDown(KEY_C))
            {
                //Initalize Client
                InitalizeClient();
                NetState = NetworkState::Client;
                OpponentSide = 0;
            }
        }

        // Use the Input recorded during the last frame
        // 1 frame input buffer (1 frame daily, has to happen)
        // Have to record/poll at least one input before updating
        GameState.Inputs[!OpponentSide] = NetInputBuffer;

        NetInputBuffer = 0;
        // Update Input
        if (IsWindowFocused())
        {

            // Storing Inputs

            if (IsKeyDown(KEY_UP))
            {
                // GameState.Entities[0].Position.Y += 2;
                NetInputBuffer |= static_cast<unsigned int>(InputCommand::Up);
            }

            if (IsKeyDown(KEY_DOWN))
            {
                NetInputBuffer |= static_cast<unsigned int>(InputCommand::Down);
            }

            if (IsKeyDown(KEY_LEFT))
            {
                NetInputBuffer |= static_cast<unsigned int>(InputCommand::Left);
            }

            if (IsKeyDown(KEY_RIGHT))
            {
                NetInputBuffer |= static_cast<unsigned int>(InputCommand::Right);
            }
        }
 

        // need to not be 1 so we know its updated?
        NetworkInputPackage LatestInputPackage{ 0, -1 };

        bool bRecievedNetworkInput = false;

        //Update Network Based on Input Polling
        if (NetState == NetworkState::Host)
        {
            // stores in player 2's state
            LatestInputPackage = TickNetworkHost(NetworkInputPackage{GameState.Inputs[0], GameState.FrameCount + 1}, bRecievedNetworkInput);
        }
        else if (NetState == NetworkState::Client)
        {
            LatestInputPackage = TickNetworkClient(NetworkInputPackage{GameState.Inputs[1], GameState.FrameCount + 1}, bRecievedNetworkInput);
        }

        for (const auto& InputPackage : NetworkInputHistory)
        {
            if (InputPackage.FrameCount == LatestInputPackage.FrameCount)
            {
                bRecievedNetworkInput = false;
                break;
            }
        }
        // 2:01 for a explantion on later stuff maybe?
        if (bRecievedNetworkInput)
        {
            //NET_LOG ADDED HERE
            
            // Update input buffer and increment index
            NetworkInputHistory[InputHistoryIndex] = LatestInputPackage;
            InputHistoryIndex = (InputHistoryIndex + 1) % INPUT_HISTORY_COUNT;
        }

        // 2:06:28 is a very fast movement to top of Source.cpp
        bool bUpdateNextFrame = (GameState.FrameCount == 0);
        int ToUseOpponentInputCommand = 0;

        // find input for next game/simulation frame
        for (const auto& InputPackage : NetworkInputHistory)
        {
            if (InputPackage.FrameCount == GameState.FrameCount)
            {
                ToUseOpponentInputCommand = InputPackage.InputCommand;
                bUpdateNextFrame = true;
                break;
            }
        }

        if (NetState != NetworkState::None)
        {
            bUpdateNextFrame = false;
        }

        if ( bUpdateNextFrame)
        {
            //NET LOG ADDED HERE

            //Assign our opponents input from network
            GameState.Inputs[OpponentSide] = ToUseOpponentInputCommand;

            // Input
            UpdateSimulation(GameState);

            // Update frame count
            GameState.FrameCount++;
        }

        //1:28 - explanation of input buffer
        // Drawing Section
        {
            BeginDrawing();
            ClearBackground(DARKGRAY);
            DrawEntity(GameState.Entities[0], BLUE);
            DrawEntity(GameState.Entities[1], RED);

            // Debug Test
            DrawText(TextFormat("P1: [%d, %d]", GameState.Entities[0].Position.X, GameState.Entities[0].Position.Y), 10, 40, 10, GREEN);
            DrawText(TextFormat("P1: [%d, %d]", GameState.Entities[1].Position.X, GameState.Entities[2].Position.Y), 10, 60, 10, GREEN);

            DrawText(TextFormat("Frame Count [%d]", GameState.FrameCount), 10, 80, 10, WHITE);
            DrawText(TextFormat("Net Frame   [%d]", LatestInputPackage.FrameCount), 10, 100, 10, WHITE);

            EndDrawing();
        }


    }

    CloseWindow();
    return 0;
}