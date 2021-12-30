
//internal packages
#include <cmath>
#include <cstdio>
#include <iostream>

// headers
#include "raylib.h"
#include "Source.h"
#include "Network.h"
#include "ClockSyncCommon.h"

// Network State of Gamestate/Entity
enum class NetworkState
{
    None,
    Host,
    Client
};

// State that we can boot an instance to
// used if we want to make instances
// clients and hosts with arguments
enum class BootNetworkSetting
{
    NoConnection,
    Host,
    Client
};

// Network State of Gamestate/Entity
enum class NetworkSyncType
{
    LockStep,
    Rollback
};

// Stores the input state for a given frame 
// and player in bitwise
enum class InputCommand : unsigned int
{
    // Powers of 2 to do bitwise OR operation
    None = 0,
    Up = 1,
    Down = 2,
    Left = 4,
    Right = 8,

};

// X and Y vector for entity positions
struct Vector2i
{
    int X {0};
    int Y {0};

    // Operators to subtract X and Ys
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

// Operator to add and subtract two vectors
Vector2i operator+(const Vector2i& A, const Vector2i& B)
{
    return Vector2i(A.X + B.X, A.Y + B.Y);
}

Vector2i operator-(const Vector2i& A, const Vector2i& B)
{
    return Vector2i(A.X + B.X, A.Y + B.Y);
}

// COMPLETE GAME STATE for an entity in the game
// use deterministic lockstep to synchronize our gamestate
// across the network
struct EntityState
{
    Vector2i Position;
    Vector2i Velocity;
};

//Complete Game State that represent our players
// Includes the  1) Entity (or Players) itself 
// 2) Inputs for the Enitity
// 3) How many frames occured on the Entitiy's instance
struct SimulationState
{
    EntityState Entities[2];
    unsigned int Inputs[2];
    int FrameCount = { 0 };
};


// Tranlates Ping to number of ticks that has occured
int PingToTicks(int PinginMS)
{
    return PinginMS/16.667f;
}

// Screen width and height
const int SCREEN_WIDTH = 1080;
const int SCREEN_HEIGHT = 768;
const int TARGET_FRAME_RATE = 60;

// Configurable properties for entity
namespace EntitySettings
{
    const int CircleRadius { 50 };
    const int StartDistance { 400 };
    const int StartHeight { 100 };
    const int MoveSpeed { 10 };
}

// Draws Entities on screen
// 1:03 on frame limiting video  for look at source.cpp up to that point
void DrawEntity(const EntityState& State, Color color)
{
    DrawCircle(/* X */ State.Position.X + (SCREEN_WIDTH/2) , /* Y */SCREEN_HEIGHT - State.Position.Y,EntitySettings::CircleRadius, color);
}


// Update Entity Movement
void UpdateEntity(unsigned int Input, EntityState& Entity)
{
    // Reset Velocity Each Frame so Entity doesn't keep moving 
    Entity.Velocity.X = 0;
    Entity.Velocity.Y = 0;

    // Applies Velocity based on input, changing
    // entity position
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

        // Change position using velocity
        Entity.Position += Entity.Velocity;
    }

}

// Apply player/entity position updates 
// using current inputs on each Entity 
void UpdateSimulation(SimulationState& GameState)
{
    UpdateEntity(GameState.Inputs[0], GameState.Entities[0]);
    UpdateEntity(GameState.Inputs[1], GameState.Entities[1]);

}

int main(/*void*/ int argc, char** argv)
{
    //Default Window Title
    const char* WindowTitle = "Clock Sync Test";

    // Initallly doesn't boot using a connection
    BootNetworkSetting NetworkModeSetting = BootNetworkSetting::NoConnection;

    NetworkSyncType NetSyncType = NetworkSyncType::Rollback;
    //NetworkSyncType NetSyncType = NetworkSyncType::LockStep;

    // boots based on command arguments?, neeed to expirement on this
    // more
    if (argc >= 2)
    {
        if (argv[1][0] == 'c')
        {
            NetworkModeSetting = BootNetworkSetting::Client;
            NetLog(LOG_ALL, "Booted Client\n");
        }
        else if (argv[1][0] == 's')
        {
            NetworkModeSetting = BootNetworkSetting::Host;
            NetLog(LOG_ALL, "Booted Host\n");

        }

        if (argc >= 3)
        {
            NetLog(LOG_ALL, "Found 3 Arguments\n");
            // Override rollback mode and run  matches in deterministic lockstep (delay based)
            if (argv[2][0] == 'd')
            {
                NetSyncType = NetworkSyncType::LockStep;
            }
        }
    }

    // Network initally off
    NetworkState NetState = NetworkState::None;

    // Assuming Left Side at first
    int LocalSide = 0;
    int OpponentSide = 1;

    // latest frame count recieved from other player/side
    int NetworkFrame = 0;

    if (NetworkModeSetting == BootNetworkSetting::Host)
    {
        // Initialize left size as host instance
        InitalizeHost();
        OpponentSide = 1;
        LocalSide = 0;
        NetState = NetworkState::Host;
        WindowTitle = "Clock Sync Test P1 : Host";
    }
    else if (NetworkModeSetting == BootNetworkSetting::Client)
    {
        // Initialize right size as client instance
        InitalizeClient();
        NetState = NetworkState::Client;
        OpponentSide = 0;
        LocalSide = 1;
        WindowTitle = "Clock Sync Test P2 : Client";
    }


    //Look to see what needs to be added for this
    SetTraceLogCallback(LogCustom);

    // Set Windows size and name
    // 60 frames per second
    InitWindow(SCREEN_WIDTH,SCREEN_HEIGHT, WindowTitle);

    //switch (NetworkModeSetting)
    //{
    //    case BootNetworkSetting::Host:
    //        SetWindowPosition(2000, 0);
    //        break;
    //    case BootNetworkSetting::Client:
    //        SetWindowPosition(3000, 0);
    //        break;
    //    default:
    //        SetWindowPosition(3000, 0);
    //        break;
    //}

    SetTargetFPS(60);

    // Entire GameState instance for main()
    SimulationState GameState;

    // Store GameState when rolling back
    SimulationState SavedGameState;

    // Initial Positions for both players to stay in sync between clients
    GameState.Entities[0].Position.X = -(EntitySettings::StartDistance / 2);
    GameState.Entities[0].Position.Y = EntitySettings::StartHeight;

    GameState.Entities[1].Position.X = EntitySettings :: StartDistance/2;
    GameState.Entities[1].Position.Y = EntitySettings::StartHeight;

    // Initally no inputs are made
    GameState.Inputs[0] = static_cast<unsigned int>(InputCommand::None);
    GameState.Inputs[1] = static_cast<unsigned int>(InputCommand::None);

    // Network Input Delay
    constexpr int NET_INPUT_DELAY = 1;

    // Network Input Buffer used to delay by one frame
    // initally 0 before game loop
    unsigned int PolledInput = 0;

    // Indicates the most recent confirmed frame from the other palyer received over the network
    int LatestNetworkFrame = -1;

    //60 seconds * 60 frame * 3 = 3 minute match 
    constexpr int INPUT_HISTORY_SIZE = 3*60*60;

    // 46:28 for 12-25-21, probably need to go back earlier
    // Records input history for both players/entities 1 and 2
    //IMPORTANT
    unsigned int InputHistory[2][INPUT_HISTORY_SIZE];

    // Current Index of History, incrmement each time a package is recieved
    int InputHistoryIndex = 0;

    // memset sets the first num bytes (3rd param) of the block of memory pointed by ptr (1st param)
    // to the specified value (2nd param) (interpreted as an unsigned char)
    // clears InputHistory ?
    memset(InputHistory,0, 2 * INPUT_HISTORY_SIZE * sizeof(unsigned int));

    // accessors for  InputHistory need to fix
    //auto GetInputHistory = [&InputHistory](int player, int frame) -> unsigned int { return InputHistory[player * INPUT_HISTORY_SIZE + frame];
    //auto SendInputHistory = [&InputHistory](int player, int frame, unsigned int value) { InputHistory[player * INPUT_HISTORY_SIZE + frame]

    // we only want to poll for the local inputs once, even if the frame is not updated
    // desyncs in rollback are mostly caused by not storing inputs correctly
    // THINK ABOUT RESENDING PACKETS IF REQUESTED AFTER THIS IS DONE AS AN EXCERCISE
    bool bUpdatePolledInput = true;

    // Before we start the game, store the inital game state
    memcpy(&SavedGameState, &GameState, sizeof(SavedGameState));

    // Number of out-of-sync frames
    // int OutOfSyncFrames = 0;

    // Record the frame number of the last game state frame that we saved
    // -1 becayse we haven't stored frame 0 yet
    int LastSavedGameStateFrame = -1;

    // ---------------------------------------MAIN GAME LOOP--------------------------------------------------------
    while (!WindowShouldClose())
    {
        // clear inputs each frame
        GameState.Inputs[0] = 0;
        GameState.Inputs[1] = 0;

        //Save State for rollbacks
        if (NetworkModeSetting == BootNetworkSetting::NoConnection)
        {
            //Save Game State
            if (IsKeyPressed(KEY_S))
            {
                memcpy(&SavedGameState, &GameState, sizeof(SavedGameState));
            }
            // Restore Game State
            else if (IsKeyPressed(KEY_R))
            {
                memcpy(&GameState, &SavedGameState, sizeof(GameState));
            }
        }

        // Use the Input recorded during the last frame
        // using input buffer (a 1 frame delay occurs)
        // bc Have to record/poll at least one input (1 loop) before updating, poll then update
        // Note updates your side with input

        // (1 frame delay) so the next time the loop occurs, Inputs[] will record
        // the key of the previous loop
        // the NetBuffer value in the first loop will be 0, so nothing will be recorded
        // (NOT NEEDED ANYMORE, HERE FOR REFERENCE)
        // GameState.Inputs[!OpponentSide] = NetInputBuffer;

        // The input value we poll on the local client. This will be sent to the remote client after polling as well 
        PolledInput = 0;

        if (bUpdatePolledInput)
        {
            // Update Input
            if (IsWindowFocused())
            {
                // Storing Inputs in input buffer
                if (IsKeyDown(KEY_UP))
                {
                    PolledInput |= static_cast<unsigned int>(InputCommand::Up);
                }

                if (IsKeyDown(KEY_DOWN))
                {
                    PolledInput |= static_cast<unsigned int>(InputCommand::Down);
                }

                if (IsKeyDown(KEY_LEFT))
                {
                    PolledInput |= static_cast<unsigned int>(InputCommand::Left);
                }

                if (IsKeyDown(KEY_RIGHT))
                {
                    PolledInput |= static_cast<unsigned int>(InputCommand::Right);
                }
            }

            // Record this input in the history buffer to send to other player
            // Send as soon as possible to reduce latency
            // note: buffer overrun possible
            if (GameState.FrameCount < INPUT_HISTORY_SIZE - NET_INPUT_DELAY)
            {
                // Polling for the next frame due to 1 frame delay
                InputHistory[LocalSide][GameState.FrameCount + NET_INPUT_DELAY] = PolledInput;
            }

            bUpdatePolledInput = false;
        }


        // Input Package has frame count of -1
        // to make frame count 0 when imcermented  
        NetworkInputPackage LatestInputPackage{ 0, -1 };

        // check if input was recieved from ticks
        bool bRecievedNetworkInput = false;

        NetworkInputPackage  ToSendNetPackage;


        // Ended Fixing code at 47:53
        // Prepare the network package to send to the opponent
        {
            const int InputStartIndex = GameState.FrameCount - NET_PACKET_INPUT_HISTORY_SIZE + 1 + NET_INPUT_DELAY;

            // Fill the network package's input history with our local input.
            // or send/copy over last 10 inputs
            for (int i = 0; i < NET_PACKET_INPUT_HISTORY_SIZE; i++)
            {
                const int InputIndex = InputStartIndex + i;

                if (InputIndex >= 0)
                {
                    ToSendNetPackage.InputHistory[i] = InputHistory[LocalSide][InputStartIndex + i];
                }
                else
                {
                    //Sentinel Value for inputs not used
                    ToSendNetPackage.InputHistory[i] = UINT_MAX;
                }
            }

            // Indicates the frame the final input in the buffer is designated for
            ToSendNetPackage.FrameCount = GameState.FrameCount + NET_INPUT_DELAY;
        }


        //Update Network Based on Input Polling
        if (NetState == NetworkState::Host)
        {
            // stores in player 2's  input package?
            // +1 because we delayed the input by a frame
            LatestInputPackage = TickNetworkHost(ToSendNetPackage, bRecievedNetworkInput);
        }
        else if (NetState == NetworkState::Client)
        {
            // stores in player 1's  input package?
            LatestInputPackage = TickNetworkClient(ToSendNetPackage, bRecievedNetworkInput);
        }

        // store latest input package if inputs were recived from other side
        if (bRecievedNetworkInput)
        {
            NetLog(LOG_ALL, "Received Net Input: Frame [%d] \n", LatestInputPackage.FrameCount);

            const int StartFrame = LatestInputPackage.FrameCount - NET_PACKET_INPUT_HISTORY_SIZE + 1;

            // Used to see correct frames in log for testing
            // records oppoents frames for every inputs up to the current into buffer
            for (int i = 0; i < NET_PACKET_INPUT_HISTORY_SIZE; i++)
            {
                const int CheckFrame = StartFrame + i;

                if (CheckFrame == (LatestNetworkFrame + 1))
                {
                    // Advance the network frame so we know that we have the input for the next simulation
                    LatestNetworkFrame++;
                }

                //NET_LOG("Input[%u] Frame[%d] \n", LatestInputPackage.InputHistory[i] , (LatestInputPackage.FrameCount - NET_PACKET_INPUT_HISTORY_SIZE + i));
                // Records other player's input to be used in game simulation
                InputHistory[OpponentSide][StartFrame + i] = LatestInputPackage.InputHistory[i];
            }
        }

        // first frame we send the inputs, but don't simulate our side
        // we wait until we get the opponent input then we update
        // need make sure, but check if frame can be updated
        // can be true if frame count == 0 since we don't want to
        // apply a new input, but we want to move to the next frame
        bool bUpdateNextFrame = (GameState.FrameCount == 0);

        //The number of times we need to run the game simulation
        int SimulatedFrames = 1;


        if ((NetSyncType == NetworkSyncType::Rollback) && (IsClientConnected()))
        {
            // TODO??? (IMPLEMENT). Forward sim always at the moment 
            bUpdateNextFrame = true;

        }
        else if (NetSyncType == NetworkSyncType::LockStep)
        {
            // Only update frame in game simulation when we have input for target frame 
            if (LatestNetworkFrame >= GameState.FrameCount)
            {
                bUpdateNextFrame = true;
            }
        }

        // don't update frame if not classified as client/host
        if (NetState == NetworkState::None)
        {
            bUpdateNextFrame = false;
        }

        // Allow game simluation when not in network mode
        if (NetworkModeSetting == BootNetworkSetting::NoConnection)
        {
            bUpdateNextFrame = true;
        }

        // Indicates whether or not we are resimulating the game after a rollback restore event
        bool bIsResimulating = false;

        // How many times we run the game simulation
        // normally one, but we could need to do more during rollbacks
        int SimulationFrames = 1;

        //Detect if we have new inputs so we can resimulate the game (rollback)
        if (LatestNetworkFrame > LastSavedGameStateFrame)
        {
            NetLog(LOG_ALL, " Rolling Back Ticked[%d] LastSavedGameStateFrame[%d] GameState.FrameCount[%d]", LatestNetworkFrame,LastSavedGameStateFrame,GameState.FrameCount);
            //bIsResimulating = true;
            bUpdateNextFrame = true;

            // Calculates the number of frames we need to resimulate plus the current frame
            // during rollbacks
            SimulationFrames = GameState.FrameCount - LastSavedGameStateFrame;


            // Rextore the last saved game state
            memcpy(&GameState, &SavedGameState, sizeof(GameState));

        }

        // if allowed to move on to next frame, apply opponents input to
        // gamestate and increment frame
        if (bUpdateNextFrame)
        {
            // Run the game simulation the number of times previously determined
            //SimFrame = Simulation Frame
            for (int SimFrame = 0; SimFrame < SimulationFrames; SimFrame++)
            {

                NetLog(LOG_ALL, "Game Ticked [%d] \n", GameState.FrameCount);

                // Assign the local player's input with delay applied
                GameState.Inputs[LocalSide] = InputHistory[LocalSide][GameState.FrameCount];

                //Assign our opponents input from network
                GameState.Inputs[OpponentSide] = InputHistory[OpponentSide][GameState.FrameCount];

                SyncLog("Frame(%d) P1[%x] P2[%x]", GameState.FrameCount, GameState.Inputs[0], GameState.Inputs[1]);

                // Apply inputs to GameState
                UpdateSimulation(GameState);

                // allow for inital input to be polled for the buffer to send
                bUpdatePolledInput = true;

                // Update frame count
                GameState.FrameCount++;


                // resimlulate from last saved gamestate using correct inputs
                if (bIsResimulating)
                {
                    // Save the game state when we have input for this frame
                    // -1 because we just updated frame count, we know we
                    // had inputs for the previous
                    if (LatestNetworkFrame >= (GameState.FrameCount - 1))
                    {
                        // Save frame count
                        LastSavedGameStateFrame = GameState.FrameCount - 1;
                        //store the inital game state
                        memcpy(&SavedGameState, &GameState, sizeof(SavedGameState));
                    }

                }

                // If we are ahead of the latest confirmed input frame
                // track the numver of forward sim frames
                //if (LatestNetworkFrame > GameState.FrameCount)
                //{
                //    // how many frames to rollback
                //    OutOfSyncFrames++;
                //}
            }
        }

        // Drawing Section
        {
            BeginDrawing();
            ClearBackground(DARKGRAY);
            DrawEntity(GameState.Entities[0], BLUE);
            DrawEntity(GameState.Entities[1], RED);

            // Debug Test
            DrawText(TextFormat("FPS [%d]", GetFPS()), 10, 20, 10, WHITE);
            DrawText(TextFormat("P1: [%d, %d]", GameState.Entities[0].Position.X, GameState.Entities[0].Position.Y), 10, 40, 10, GREEN);
            DrawText(TextFormat("P2: [%d, %d]", GameState.Entities[1].Position.X, GameState.Entities[1].Position.Y), 10, 60, 10, GREEN);

            DrawText(TextFormat("Frame Count [%d]", GameState.FrameCount), 10, 80, 10, WHITE);
            DrawText(TextFormat("Net Frame   [%d]", LatestInputPackage.FrameCount), 10, 100, 10, WHITE);
            DrawText(TextFormat("Input Delay [%d]", NET_INPUT_DELAY), 10, 120, 10, WHITE);

            if (NetSyncType == NetworkSyncType::Rollback)
            {
                DrawText("[Rollback Mode]", 10, 140, 10, BLUE);
            }
            else if (NetSyncType == NetworkSyncType::LockStep)
            {
                DrawText("[Delay Mode]", 10, 140, 10, BLUE);
            }


            EndDrawing();
        }


    }

    CloseWindow();
    return 0;
}
