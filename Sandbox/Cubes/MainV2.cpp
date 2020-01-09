/*
 *  MainV2.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "TestAssetTypes.hpp"

#include <Bee/Application/Main.hpp>
#include <Bee/AssetV2/AssetV2.hpp>
#include <Bee/AssetPipelineV2/AssetCompilerV2.hpp>
#include <Bee/Core/Containers/ResourcePool.hpp>
#include <Bee/Core/Jobs/JobSystem.hpp>
#include <Bee/Core/IO.hpp>


struct TextureLoader final : bee::AssetLoader
{
    bee::ResourcePool<bee::AssetHandle, bee::Texture> textures { sizeof(bee::Texture) };

    bee::AssetHandle allocate(const bee::Type* type) override
    {
        BEE_ASSERT(type == bee::get_type<bee::Texture>());
        return textures.allocate();
    }

    bee::AssetData get(const bee::Type* type, const bee::AssetHandle& handle) override
    {
        BEE_ASSERT(type == bee::get_type<bee::Texture>());
        return bee::AssetData(type, &textures[handle]);
    }

    bee::AssetStatus load(bee::AssetData* dst_data, bee::io::Stream* src_stream) override
    {
        bee::log_info("Loading texture...");
        dst_data->as<bee::Texture>()->loaded = true;
        return bee::AssetStatus::loaded;
    }

    bee::AssetStatus unload(bee::AssetData* data, const bee::AssetUnloadType unload_type) override
    {
        bee::log_info("Unloading texture...");
        data->as<bee::Texture>()->loaded = false;
        return bee::AssetStatus::unloaded;
    }
};


struct DefaultLocator final : bee::AssetLocator
{
    bool locate(const bee::GUID& guid, bee::AssetLocation* location) override
    {
        location->type = bee::AssetLocationType::in_memory;
        return true;
    }
};


int bee_main(int argc, char** argv)
{
    bee::job_system_init(bee::JobSystemInitInfo{});
    bee::assets_init();

    TextureLoader texture_loader;
    DefaultLocator locator;
    bee::register_asset_locator("DefaultLocator", &locator);
    bee::register_asset_loader("TextureLoader", &texture_loader, { bee::get_type<bee::Texture>() });

    bee::register_asset_compiler<bee::TextureCompiler>();
    bee::compile_asset_sync(bee::AssetPlatform::windows, "test/test.png");
//    bee::TextureSettings settings{};
//    settings.mipmap = true;
//    compile_asset(AssetPlatform::windows, "texture/path/tex.png");
    const auto texture_guid = bee::generate_guid();
    bee::register_asset_name("textures::cube", texture_guid);
    auto texture = bee::load_asset<bee::Texture>("textures::cube");
    texture.unload();

    bee::assets_shutdown();
    bee::job_system_shutdown();
    return 0;
}