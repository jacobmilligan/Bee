/*
 *  AssetDatabaseV2.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/GUID.hpp"
#include "Bee/AssetPipeline/AssetPlatform.hpp"

struct MDB_env;
struct MDB_txn;

namespace bee {


enum class AssetDbTxnKind
{
    invalid,
    read_only,
    read_write
};

struct BEE_REFLECT(serializable, version = 1) AssetFile
{
    BEE_REFLECT(id = 1, added = 1)
    GUID                guid;

    BEE_REFLECT(id = 2, added = 1)
    String              name;

    BEE_REFLECT(id = 3, added = 1)
    Path                source;

    BEE_REFLECT(id = 4, nonserialized)
    Path                location;

    BEE_REFLECT(id = 5, added = 1)
    DynamicArray<u128>  artifacts;

    BEE_REFLECT(id = 6, added = 1)
    TypeInstance        options;

    AssetFile(Allocator* allocator = system_allocator())
        : name(allocator),
          source(allocator),
          location(allocator),
          artifacts(allocator)
    {}
};

struct AssetDbDescriptor
{
    AssetDbTxnKind  kind { AssetDbTxnKind::invalid };
    MDB_env*        env { nullptr };
    unsigned int    assets_dbi { 0 };
    unsigned int    guid_to_name_dbi { 0 };
    unsigned int    name_to_guid_dbi {0 };
    unsigned int    artifact_dbi { 0 };
};


class BEE_DEVELOP_API AssetDbTxn final : public Noncopyable
{
public:
    AssetDbTxn() = default;

    AssetDbTxn(const AssetDbDescriptor& desc);

    AssetDbTxn(AssetDbTxn&& other) noexcept;

    ~AssetDbTxn();

    AssetDbTxn& operator=(AssetDbTxn&& other) noexcept;

    void abort();

    void commit();

    bool put_asset(const AssetFile& asset);

    bool delete_asset(const GUID& guid);

    bool get_asset(const GUID& guid, AssetFile* asset);

    bool has_asset(const GUID& guid);

    bool set_asset_name(const GUID& guid, const StringView& name);

    bool get_name_from_guid(const GUID& guid, String* name);

    bool get_guid_from_name(const StringView& name, GUID* guid);

    bool put_artifact(const AssetArtifact& artifact);

    bool delete_artifact(const u128& hash);

    bool get_artifact(const u128& hash, AssetArtifact* artifact);

    bool get_artifacts_from_guid(const GUID& guid, DynamicArray<AssetArtifact>* result);

private:
    AssetDbDescriptor       desc_;
    MDB_txn*                ptr_ { nullptr };

    void move_construct(AssetDbTxn& other) noexcept;

    void destroy();
};


class BEE_DEVELOP_API AssetDatabase
{
public:
    ~AssetDatabase();

    void open(const Path& directory, const StringView& name);

    void close();

    AssetDbTxn read();

    AssetDbTxn write();

    inline bool is_open() const
    {
        return desc_.env != nullptr;
    }

    inline const Path& location() const
    {
        return location_;
    }

private:
    Path                location_;
    AssetDbDescriptor   desc_;
};


} // namespace bee