/*
 *  GameMain.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/AppLoop.hpp"
#include "Bee/Application/Main.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Memory/Allocator.hpp"

namespace bee {


enum class GameCmdType
{
    force_quit
};

struct GameCmdHeader
{

};


BEE_GAME_API int game_run(const AppDescriptor& info, const char* game_plugin_name);


} // namespace bee
