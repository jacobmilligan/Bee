/*
 *  GameMain.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/Application.hpp"
#include "Bee/AssetV2/AssetV3.hpp"
#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Graphics/Renderer.hpp"

namespace bee {


struct EngineSubsystems
{
    AssetRegistry   asset_registry;
    Renderer        renderer;
};


} // namespace bee
