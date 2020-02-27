/*
 *  AssetDatabase.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Path.hpp"
#include "Bee/Core/GUID.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/AssetPipeline/AssetCompiler.hpp"


struct MDB_txn;

namespace bee {


BEE_RAW_HANDLE_I32(AssetDBHandle);


struct BEE_REFLECT(serializable, version = 1) AssetMeta
{
    BEE_REFLECT(id = 1, added = 1)
    GUID                    guid;

    BEE_REFLECT(id = 2, added = 1)
    u128                    content_hash;

    BEE_REFLECT(id = 3, added = 1)
    u32                     compiler { 0 };

    BEE_REFLECT(id = 4, added = 1)
    StaticString<128>       name;

    BEE_REFLECT(id = 5, added = 1)
    StaticString<512>       source;

    BEE_REFLECT(id = 6, nonserialized)
    StaticString<512>       location;
};

struct BEE_REFLECT(serializable) AssetFile
{
    AssetMeta       meta;
    TypeInstance    options;
};


class BEE_DEVELOP_API AssetDBTxn final : public Noncopyable
{
public:
    enum class Kind : u8
    {
        invalid,
        read_only,
        read_write
    };

    AssetDBTxn() = default;

    AssetDBTxn(const Kind kind, MDB_txn* txn, AssetMeta* meta, TypeInstance&& options)
        : kind_(kind),
          txn_(txn),
          meta_(meta),
          options_(std::move(options))
    {}

    AssetDBTxn(AssetDBTxn&& other) noexcept
    {
        move_construct(other);
    }

    ~AssetDBTxn();

    AssetDBTxn& operator=(AssetDBTxn&& other) noexcept
    {
        move_construct(other);
        return *this;
    }

    void abort();

    void commit();

    inline Kind kind() const
    {
        return kind_;
    }

    template <typename T>
    T* get_options()
    {
        if (BEE_CHECK(kind_ == Kind::read_write))
        {
            return options_.get<T>();
        }
        return nullptr;
    }

    template <typename T>
    const T* get_options() const
    {
        options_.get<T>();
    }

protected:
    Kind            kind_ { Kind::invalid };
    AssetMeta*      meta_ { nullptr };
    MDB_txn*        txn_ { nullptr };
    TypeInstance    options_;

    void move_construct(AssetDBTxn& other) noexcept;

    void destroy();
};

template <typename T>
class AssetDBReader final : public Noncopyable
{
public:
    AssetDBReader() = default;

    explicit AssetDBReader(AssetDBTxn&& txn)
        : txn_(std::move(txn))
    {
        BEE_ASSERT(txn_.kind() == AssetDBTxn::Kind::read_only);
#if BEE_DEBUG == 1
        data_ = txn_.get_options<T>();
#endif // BEE_DEBUG == 1
    }

    AssetDBReader(AssetDBReader<T>&& other) noexcept
    {
        move_construct();
    }

    ~AssetDBReader()
    {
#if BEE_DEBUG == 1
        data_ = nullptr;
#endif // BEE_DEBUG == 1
    }

    AssetDBReader& operator=(AssetDBReader<T>&& other) noexcept
    {
        move_construct();
        return *this;
    }

    inline const T* operator->() const
    {
        return txn_.get_options<T>();
    }

    inline void abort()
    {
        txn_.abort();
#if BEE_DEBUG == 1
        data_ = nullptr;
#endif // BEE_DEBUG == 1
    }
private:
    AssetDBTxn  txn_;
#if BEE_DEBUG == 1
    const T*    data_ { nullptr };
#endif // BEE_DEBUG == 1

    void move_construct() noexcept
    {
#if BEE_DEBUG == 1
        data_ = other.data_;
        other.data_ = nullptr;
#endif // BEE_DEBUG == 1
    }
};

template <typename T>
class AssetDBWriter
{
public:
    AssetDBWriter() = default;

    explicit AssetDBWriter(AssetDBTxn&& txn)
        : txn_(std::move(txn))
    {
        BEE_ASSERT(txn_.kind() == AssetDBTxn::Kind::read_write);
#if BEE_DEBUG == 1
        data_ = txn_.get_options<T>();
#endif // BEE_DEBUG == 1
    }

    AssetDBWriter(AssetDBWriter<T>&& other) noexcept
    {
        move_construct();
    }

    ~AssetDBWriter()
    {
#if BEE_DEBUG == 1
        data_ = nullptr;
#endif // BEE_DEBUG == 1
    }

    AssetDBWriter& operator=(AssetDBWriter<T>&& other) noexcept
    {
        move_construct();
        return *this;
    }

    inline T* operator->()
    {
        return txn_.get_options<T>();
    }

    inline void abort()
    {
        txn_.abort();
#if BEE_DEBUG == 1
        data_ = nullptr;
#endif // BEE_DEBUG == 1
    }

    inline void commit()
    {
        txn_.commit();
#if BEE_DEBUG == 1
        data_ = nullptr;
#endif // BEE_DEBUG == 1
    }
private:
    AssetDBTxn  txn_;
#if BEE_DEBUG == 1
    const T*    data_ { nullptr };
#endif // BEE_DEBUG == 1

    void move_construct() noexcept
    {
#if BEE_DEBUG == 1
        data_ = other.data_;
        other.data_ = nullptr;
#endif // BEE_DEBUG == 1
    }
};



BEE_DEVELOP_API void assetdb_open(const Path& root, AssetCompilerPipeline* compiler_pipeline);

BEE_DEVELOP_API void assetdb_close();

BEE_DEVELOP_API void assetdb_import(const StringView& name, const Path& source_path, const Path& target_folder, JobGroup* wait_group = nullptr);

BEE_DEVELOP_API void assetdb_import(const Path& source_path, const Path& target_folder, JobGroup* wait_group = nullptr);

BEE_DEVELOP_API void assetdb_save();

BEE_DEVELOP_API bool assetdb_get_guid(const StringView& name, GUID* dst_guid);

BEE_DEVELOP_API AssetDBTxn assetdb_transaction(const AssetDBTxn::Kind kind, const GUID& guid, const Type* type);

template <typename T>
inline AssetDBReader<T> assetdb_read(const StringView& name)
{
    GUID guid;
    if (!assetdb_get_guid(name, &guid))
    {
        return {};
    }

    return AssetDBReader<T>(std::move(assetdb_transaction(AssetDBTxn::Kind::read_only, guid, get_type<T>())));
}

template <typename T>
inline AssetDBWriter<T> assetdb_write(const StringView& name)
{
    GUID guid;
    if (!assetdb_get_guid(name, &guid))
    {
        return {};
    }
    return AssetDBWriter<T>(std::move(assetdb_transaction(AssetDBTxn::Kind::read_write, guid, get_type<T>())));
}

template <typename T>
inline AssetDBReader<T> assetdb_read(const GUID& guid)
{
    return AssetDBReader<T>(std::move(assetdb_transaction(AssetDBTxn::Kind::read_only, guid, get_type<T>())));
}

template <typename T>
inline AssetDBWriter<T> assetdb_write(const GUID& guid)
{
    return AssetDBWriter<T>(std::move(assetdb_transaction(AssetDBTxn::Kind::read_write, guid, get_type<T>())));
}

BEE_DEVELOP_API u128 assetdb_get_content_hash(const AssetPlatform platform, const AssetFile& asset);


} // namespace bee