#pragma once

#ifdef LIBSILK_EXPORTS
#define PLAYER_API __declspec(dllexport)
#else
#define PLAYER_API __declspec(dllimport)
#endif

#include "..\Player\IPlayer.h"

PLAYER_API IPlayer* CreateSilkPlayer();
PLAYER_API void ReleaseSilkPlayer(IPlayer* player);