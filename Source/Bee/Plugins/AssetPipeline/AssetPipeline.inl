/*
 *  AssetPipeline.inl
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

namespace bee {


inline AssetDbTxn::AssetDbTxn(AssetDbTxn&& other) noexcept
{
    destruct(this);
    assetdb = other.assetdb;
    handle = other.handle;
    kind = other.kind;
    other.kind = AssetDbTxnKind::invalid;
    other.handle = nullptr;
    other.assetdb = nullptr;
}

inline AssetDbTxn::~AssetDbTxn()
{
    switch(kind)
    {
        case AssetDbTxnKind::read_write:
        {
            assetdb->abort_transaction(this);
            break;
        }
        case AssetDbTxnKind::read_only:
        {
            assetdb->commit_transaction(this);
            break;
        }
        default:
        {
            break;
        }
    }

    kind = AssetDbTxnKind::invalid;
    handle = nullptr;
    assetdb = nullptr;
}

inline AssetDbTxn& AssetDbTxn::operator=(AssetDbTxn&& other) noexcept
{
    destruct(this);
    assetdb = other.assetdb;
    handle = other.handle;
    kind = other.kind;
    other.kind = AssetDbTxnKind::invalid;
    other.handle = nullptr;
    other.assetdb = nullptr;
    return *this;
}


} // namespace bee
