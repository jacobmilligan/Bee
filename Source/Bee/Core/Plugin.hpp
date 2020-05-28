/*
 *  PluginV2.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/DynamicLibrary.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Random.hpp"
#include "Bee/Core/Memory/PoolAllocator.hpp"


namespace bee {


enum class PluginEventType
{
    none,
    add_module,
    remove_module,
    load_plugin,
    unload_plugin
};

enum class RegisterPluginMode
{
    auto_load,
    manual_load
};

enum class PluginState
{
    loading,
    unloading
};


class PluginRegistry;


struct PluginVersion
{
    u8 major { 0 };
    u8 minor { 0 };
    u8 patch { 0 };
};

constexpr PluginVersion plugin_version_any { limits::max<u8>(), limits::max<u8>(), limits::max<u8>() };

inline constexpr bool operator==(const PluginVersion& lhs, const PluginVersion& rhs)
{
    return lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.patch == rhs.patch;
}

inline constexpr bool operator!=(const PluginVersion& lhs, const PluginVersion& rhs)
{
    return !(lhs == rhs);
}

struct PluginDependency
{
    const char*     name { nullptr };
    PluginVersion   version;
};

struct PluginDescriptor
{
    PluginVersion           version;
    const char*             name { nullptr };
    const char*             description { nullptr };
    const char*             source_location { nullptr };
    i32                     dependency_count { 0 };
    const PluginDependency* dependencies { nullptr };

    PluginDescriptor() = default;

    template <i32 Size>
    PluginDescriptor(
        const PluginVersion& new_version,
        const char* new_name,
        const char* new_description,
        const char* new_source_location,
        const PluginDependency(&dependency_array)[Size]
    )
        : version(new_version),
          name(new_name),
          description(new_description),
          source_location(new_source_location),
          dependency_count(Size),
          dependencies(dependency_array)
    {}

    PluginDescriptor(
        const PluginVersion& new_version,
        const char* new_name,
        const char* new_description,
        const char* new_source_location
    )
        : version(new_version),
          name(new_name),
          description(new_description),
          source_location(new_source_location)
    {}

    inline void get_full_path(Path* dst) const
    {
        dst->clear();
        dst->append(fs::get_root_dirs().install_root).append(source_location);
    }
};


using load_plugin_function_t = void*(*)(PluginRegistry* registry, PluginState state);

using plugin_observer_t = void(*)(const PluginEventType event, const PluginDescriptor& plugin, const StringView& module_name, void* module, void* user_data);

using module_observer_t = void(*)(const PluginEventType event, void* module, void* user_data);


class BEE_CORE_API PluginRegistry
{
public:
    PluginRegistry();

    ~PluginRegistry();

    bool add_search_path(const Path& path, const RegisterPluginMode register_mode = RegisterPluginMode::manual_load);

    void remove_search_path(const Path& path);

    bool load_plugin(const StringView& name, const PluginVersion& required_version);

    bool unload_plugin(const StringView& name);

    void add_module(const StringView& name, const void* interface_ptr, const size_t size);

    void remove_module(const void* interface_ptr);

    void* get_module(const StringView& name);

    bool has_module(const StringView& name);

    void refresh_plugins();

    bool is_plugin_registered(const StringView& name);

    bool is_plugin_loaded(const StringView& name, const PluginVersion& version);

    i32 get_loaded_plugins(PluginDescriptor* descriptors);

    void add_observer(plugin_observer_t observer, void* user_data = nullptr);

    void remove_observer(plugin_observer_t observer, void* user_data = nullptr);

    void* get_or_create_persistent(const u32 unique_hash, const size_t size);

    template <typename T>
    inline void add_module(const StringView& name, const T* module)
    {
        add_module(name, module, sizeof(T));
    }

    template <typename T>
    inline T* get_module(const StringView& name)
    {
        return static_cast<T*>(get_module(name));
    }

    template <typename T>
    inline void toggle_module(const PluginState state, const StringView& name, const T* module)
    {
        if (state == PluginState::loading)
        {
            add_module(name, module);
        }
        else
        {
            remove_module(module);
        }
    }

    template <typename T, i32 Size, typename... ConstructorArgs>
    inline T* get_or_create_persistent(const char(&name)[Size], ConstructorArgs&&... args)
    {
        T* ptr = nullptr;
        if (!get_or_create_persistent(get_static_string_hash(name), sizeof(T), &static_cast<void*>(ptr)))
        {
            new (ptr) T(std::forward<ConstructorArgs>(args)...);
        }
        return ptr;
    }
private:
    struct Plugin
    {
        bool                        is_loaded { false };
        PluginDescriptor            desc;
        String                      name;
        u32                         name_hash { 0 };
        Path                        source_path;
        Path                        library_path;
        Path                        current_version_path;
        Path                        old_version_path;
        DynamicLibrary              library;
        load_plugin_function_t      load_function { nullptr };

        Plugin() = default;

        explicit Plugin(const Path& path, const StringView& new_name)
            : is_loaded(false),
              library_path(path),
              name(new_name)
        {
            name_hash = get_hash(new_name);
        }
    };

    struct Observer
    {
        plugin_observer_t   callback { nullptr };
        void*               user_data { nullptr };

        Observer() = default;

        Observer(plugin_observer_t new_callback, void* new_user_data)
            : callback(new_callback),
              user_data(new_user_data)
        {}
    };

    struct ModuleObserver
    {
        module_observer_t   callback { nullptr };
        void*               user_data { nullptr };

        ModuleObserver() = default;

        ModuleObserver(module_observer_t new_callback, void* new_user_data)
            : callback(new_callback),
              user_data(new_user_data)
        {}
    };

    struct Module
    {
        u32                             hash { 0 };
        StaticString<256>               name;
        const void*                     current {nullptr };
        void*                           storage;
    };

    static constexpr size_t                     module_size_ = 1024;
    static constexpr size_t                     module_storage_capacity_ = module_size_ - sizeof(Module);

    DynamicArray<Module*>                       modules_;
    DynamicArray<Observer>                      observers_;
    DynamicHashMap<u32, DynamicArray<u8>>       persistent_;
    DynamicHashMap<u32, Plugin>                 plugins_;
    DynamicHashMap<Path, DynamicArray<u32>>     search_paths_;
    fs::DirectoryWatcher                        directory_watcher_;
    DynamicArray<Plugin*>                       load_stack_;

    bool register_plugins_at_path(const Path& root, const RegisterPluginMode search_path_type);

    void register_plugin(const Path& path, const RegisterPluginMode register_mode);

    void unregister_plugin(const Path& path);

    bool load_plugin(Plugin* plugin, const PluginVersion& required_version);

    void unload_plugin(Plugin* plugin);

    Module& get_or_create_module(const StringView& name);

    bool get_or_create_persistent(const u32 unique_hash, const size_t size, void** out_data);

    void notify_observers(const PluginEventType event, const Plugin* plugin, const Module* module);

    inline Module* create_module(const u32 hash, const StringView& name)
    {
        auto* module_memory = static_cast<u8*>(BEE_MALLOC(system_allocator(), module_size_));
        auto* module = reinterpret_cast<Module*>(module_memory);

        new (module) Module{};
        module->hash = hash;
        module->name = name;
        module->current = nullptr;
        module->storage = module_memory + sizeof(Module);
        memset(module->storage, 0, module_storage_capacity_);

        return module;
    }

    inline void destroy_module(Module* module)
    {
        destruct(module);
        BEE_FREE(system_allocator(), module);
    }
};


struct BEE_CORE_API StaticPluginAutoRegistration
{
    StaticPluginAutoRegistration*   next { nullptr };
    load_plugin_function_t          load_plugin {nullptr };

    StaticPluginAutoRegistration(const char* name, load_plugin_function_t load_function);
};


#define BEE_PLUGIN_API extern "C" BEE_EXPORT_SYMBOL

#if BEE_CONFIG_MONOLITHIC_BUILD == 0
    #define BEE_REGISTER_PLUGIN(name)
#else
    #define BEE_REGISTER_PLUGIN(name) \
        StaticPluginAutoRegistration auto_plugin_registration(#name, bee_load_plugin, bee_unload_plugin)
#endif // BEE_CONFIG_MONOLITHIC_BUILD == 0


} // namespace bee


