#pragma once
struct GameKeyState
{
	s32 halfTransCount;
	b32 wasDown;
};

struct GameMouseData
{
	s32 x;
	s32 y;
	s32 deltaX;
	s32 deltaY;
	s32 deltaWheel;
};

struct GameController
{
	b16 isConnected;
	GameMouseData mouse;

	union
	{
		GameKeyState buttons[12];
		struct
		{
			GameKeyState moveUp;
			GameKeyState moveDown;
			GameKeyState moveLeft;
			GameKeyState moveRight;

			GameKeyState actionFire;
			GameKeyState action1;
			GameKeyState action2;
			GameKeyState action3;
			GameKeyState action4;
			GameKeyState action5;

			GameKeyState back;
			GameKeyState start;

			// All buttons must be added above this line
			GameKeyState terminator;
		};
	};
};

struct GameInput
{
	GameController controllers[2];
};

#if GAME_INTERNAL
struct DebugFileOutput
{
	void *data;
	u32 dataSize;
};
#endif

inline GameController *getGameController(GameInput *input, u32 controllerID)
{
	GameAssert(controllerID < (u32)array_count_64(input->controllers));
	return &input->controllers[controllerID];
}