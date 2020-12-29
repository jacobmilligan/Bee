/*
 *  Plugin.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"
#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Core/Hash.hpp"
#include "Bee/Core/Path.hpp"


namespace bee {


#define BEE_PLUGIN_API extern "C" BEE_EXPORT_SYMBOL

#define BEE_PLUGIN_VERSION(v_major, v_minor, v_patch) \
    extern "C" BEE_EXPORT_SYMBOL void bee_get_plugin_version(bee::PluginVersion* version)   \
    {                                                                                       \
        version->major = v_major;                                                           \
        version->minor = v_minor;                                                           \
        version->patch = v_patch;                                                           \
    }


enum class PluginState
{
    loading,
    unloading,
    loaded,
    unloaded
};

struct PluginVersion
{
    i32 major { -1 };
    i32 minor { -1 };
    i32 patch { -1 };
};

inline constexpr bool operator==(const PluginVersion& lhs, const PluginVersion& rhs)
{
    return lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.patch == rhs.patch;
}

inline constexpr bool operator!=(const PluginVersion& lhs, const PluginVersion& rhs)
{
    return !(lhs == rhs);
}

inline constexpr bool operator>(const PluginVersion& lhs, const PluginVersion& rhs)
{
    if (lhs.major != rhs.major)
    {
        return lhs.major > rhs.major;
    }

    if (lhs.minor != rhs.minor)
    {
        return lhs.minor > rhs.minor;
    }

    return lhs.patch > rhs.patch;
}

inline constexpr bool operator>=(const PluginVersion& lhs, const PluginVersion& rhs)
{
    return lhs == rhs || lhs > rhs;
}

inline constexpr bool operator<(const PluginVersion& lhs, const PluginVersion& rhs)
{
    return !(lhs > rhs);
}

inline constexpr bool operator<=(const PluginVersion& lhs, const PluginVersion& rhs)
{
    return lhs == rhs || lhs < rhs;
}

struct PluginStaticDataCallbacks
{
    void (*construct)(void* data) { nullptr };
    void (*destruct)(void* data) { nullptr };
};

class BEE_CORE_API PluginLoader
{
public:
    void* get_static(const PluginStaticDataCallbacks& static_callbacks, const u32 hash, const size_t size, const size_t alignment);

    bool require_plugin(const StringView& name, const PluginVersion& minimum_version) const;

    bool is_plugin_loaded(const StringView& name, const PluginVersion& minimum_version) const;

    void* get_module(const StringView& name);

    void add_module_interface(const StringView& name, const void* module, const size_t module_size);

    void remove_module_interface(const void* module);

    template <typename T, i32 Size, typename... ConstructorArgs>
    inline T* get_static(const char(&name)[Size], ConstructorArgs&&... args)
    {
        static auto constructor = [](void* data) { new (static_cast<T*>(data)) T(); };
        static auto destructor = [](void* data) { destruct(static_cast<T*>(data)); };

        PluginStaticDataCallbacks callbacks{};
        callbacks.construct = constructor;
        callbacks.destruct = destructor;

        T* ptr = static_cast<T*>(get_static(callbacks, get_static_string_hash(name), sizeof(T), alignof(T)));
        BEE_ASSERT_F(ptr != nullptr, "Failed to get or create static plugin data \"%s\"", name);
        return ptr;
    }

    template <typename T>
    inline void set_module(const StringView& name, const T* module, const PluginState state)
    {
        if (state == PluginState::loading)
        {
            add_module_interface(name, module, sizeof(T));
        }
        else
        {
            remove_module_interface(module);
        }
    }
};

BEE_CORE_API void init_plugins();

BEE_CORE_API void shutdown_plugins();

BEE_CORE_API bool load_plugin(const StringView& name);

BEE_CORE_API void unload_plugin(const StringView& name);

BEE_CORE_API void refresh_plugins();

BEE_CORE_API void add_plugin_search_path(const Path& path);

BEE_CORE_API void remove_plugin_search_path(const Path& path);

BEE_CORE_API void* get_module(const StringView& name);



} // namespace bee


