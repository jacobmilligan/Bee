/*
 *  Database.inl
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Memory/LinearAllocator.hpp"

#include <lmdb.h>


namespace bee {


struct Dbi
{
    enum Enum
    {
        path_to_guid,
        guid_to_asset,
        count
    };

    BEE_ENUM_STRUCT(Dbi)
};

struct ThreadData
{
    LinearAllocator     temp_allocator { megabytes(4), system_allocator() };
    DynamicArray<u8>    artifact_buffer;
};

struct FileTypeInfo
{
    const char*         extension { nullptr };
    DynamicArray<u32>   importer_hashes;
};

struct AssetImporterInfo
{
    AssetImporter*      importer { nullptr };
    DynamicArray<u32>   file_type_hashes;
};

struct AssetPipeline
{
    Path                                config_path;
    Path                                full_cache_path;
    Path                                db_path;
    AssetDatabase*                      db { nullptr };
    FixedArray<ThreadData>              thread_data;

    AssetPipelineConfig                 config;
    fs::DirectoryWatcher                source_watcher;
    DynamicArray<fs::FileNotifyInfo>    fs_events;

    // Importer data
    DynamicArray<u32>                   file_type_hashes;
    DynamicArray<FileTypeInfo>          file_types;
    DynamicArray<u32>                   importer_hashes;
    DynamicArray<AssetImporterInfo>     importers;
};

class TempAlloc
{
public:
    TempAlloc(AssetPipeline* pipeline);

    ~TempAlloc();

    operator Allocator*()
    {
        return allocator_;
    }
private:
    LinearAllocator*    allocator_ { nullptr };
    size_t              offset_ { 0 };
};


} // namespace bee