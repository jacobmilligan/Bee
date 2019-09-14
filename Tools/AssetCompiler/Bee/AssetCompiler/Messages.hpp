/*
 *  Messages.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Serialization/Serialization.hpp"
#include "Bee/AssetCompiler/Pipeline.hpp"


namespace bee {


/*
 ****************************************************************
 *
 * # Message ID's
 *
 * Enum used to identify asset compiler messages as they are
 * received by the server/client in their processing loops
 *
 ****************************************************************
 */
enum class ACMessageId
{
    unknown,
    shutdown,
    complete,
    load_plugin,
    unload_plugin,
    compile
};


/*
 ****************************************************************
 *
 * # Messages
 *
 * Base message types that help to identify the type of message
 * being processed and their expected ID's/response types
 *
 ****************************************************************
 */
struct ACMessage
{
    ACMessageId id { ACMessageId::unknown };
    i32         size { 0 };
};

template <ACMessageId Id>
struct ACMessageData : public ACMessage
{
    static constexpr ACMessageId type = Id;

    ACMessageData()
        : ACMessage { Id, 0 }
    {}
};


/*
 ********************************
 *
 * # Shutdown Message
 *
 ********************************
 */

struct ACShutdownMsg : public ACMessageData<ACMessageId::shutdown>
{
};

BEE_SERIALIZE(ACShutdownMsg, 1) {}


/*
 ********************************************
 *
 * # Plugin loading/unloading messages
 *
 ********************************************
 */
struct ACLoadPluginMsg : public ACMessageData<ACMessageId::load_plugin>
{
    char directory[1024];
    char filename[64];
};

struct ACUnloadPluginMsg : public ACMessageData<ACMessageId::unload_plugin>
{
    char name[64];
};

BEE_SERIALIZE(ACLoadPluginMsg, 1)
{
    BEE_ADD_FIELD(1, directory);
    BEE_ADD_FIELD(1, filename);
}

BEE_SERIALIZE(ACUnloadPluginMsg, 1)
{
    BEE_ADD_FIELD(1, name);
}

/*
 ********************************************
 *
 * # Compile message
 *
 ********************************************
 */
struct ACCompileMsg : public ACMessageData<ACMessageId::compile>
{
    AssetPlatform   platform { AssetPlatform::unknown };
    char            src_path[1024];
    char            dst_path[1024];
};

BEE_SERIALIZE(ACCompileMsg, 1)
{
    BEE_ADD_FIELD(1, platform);
    BEE_ADD_FIELD(1, src_path);
    BEE_ADD_FIELD(1, dst_path);
}


} // namespace bee