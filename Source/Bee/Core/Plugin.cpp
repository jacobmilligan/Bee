/*
 *  Plugin.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/DynamicLibrary.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/Debug.hpp"

namespace bee {
namespace plugins {

/**
 * Represents either a set of function pointers or a dynamic DLL/.dylib plugin.
 * NOTE: this doesn't do any locking as that's intended to be handled by the higher-level functions
 * (`unload_plugin` etc.)
 */
struct Plugin
{
    static constexpr i32 name_capacity = 1024;

    u32                 hash { 0 };
    char                name[1024];
    PluginAPI           api { nullptr };

    DynamicLibrary      lib;

    load_function_t*    load { nullptr };
    unload_function_t*  unload{ nullptr };

    ~Plugin()
    {
        if (unload != nullptr)
        {
            // Call the plugins unload function before doing any dylib unloading
            unload(api.ptr);
        }

        if (lib.handle != nullptr)
        {
            unload_library(lib);
            lib = DynamicLibrary{};

            // Make sure any dynamically loaded symbols are added to the debug trace system
            refresh_debug_symbols();
        }
    }
};

struct PluginRegistry
{
    SpinLock                mutex;
    DynamicArray<Plugin>    plugins;
};

static PluginRegistry* global_registry = nullptr;


#define BEE_ASSERT_PLUGIN_SYSTEM() BEE_ASSERT_F(global_registry != nullptr, "Plugin system is destroyed or not initialized")


void unload_plugin(Plugin* plugin);

void init_registry()
{
    if (BEE_FAIL_F(global_registry == nullptr, "Plugin system is already initialized"))
    {
        return;
    }

    global_registry = BEE_NEW(system_allocator(), PluginRegistry)();
}

void destroy_registry()
{
    BEE_ASSERT_PLUGIN_SYSTEM();
    BEE_DELETE(system_allocator(), global_registry);
    global_registry = nullptr;
}

bool is_registry_initialized()
{
    return global_registry != nullptr;
}

/**
 * Finds a plugin index in the global plugin array using the plugins name hash. NOTE: this doesn't
 * do any locking as that's intended to be handled by the higher-level functions (`load_dynamic_plugin` etc.)
 */
i32 find_plugin(const u32 hash)
{
    for (int plugin_idx = 0; plugin_idx < global_registry->plugins.size(); ++plugin_idx)
    {
        if (global_registry->plugins[plugin_idx].hash == hash)
        {
            return plugin_idx;
        }
    }

    return -1;
}

/**
 * Finds a plugin index in the global plugin array using the plugins name (this will hash the name). NOTE: this doesn't
 * do any locking as that's intended to be handled by the higher-level functions (`load_dynamic_plugin` etc.)
 */
i32 find_plugin(const char* name)
{
    return find_plugin(get_hash(name));
}

/**
 * Allocates a new plugin instance without any load/unload functions or API instances. NOTE: this doesn't
 * do any locking as that's intended to be handled by the higher-level functions (`load_dynamic_plugin` etc.)
 */
Plugin* allocate_plugin(const char* name)
{
    const auto hash = get_hash(name);
    if (BEE_FAIL_F(find_plugin(hash) < 0, "Cannot allocate plugin \"%s\": that plugin is already loaded", name))
    {
        return nullptr;
    }

    Plugin new_plugin{};
    new_plugin.hash = hash;
    str::copy(new_plugin.name, static_array_length(new_plugin.name), name);

    global_registry->plugins.push_back(new_plugin);
    return &global_registry->plugins.back();
}

void unload_and_destroy_plugin(const char* name)
{
    auto plugin_idx = find_plugin(name);
    if (BEE_FAIL_F(plugin_idx >= 0, "Cannot unload plugin_idx \"%s\" - the plugin is not loaded", name))
    {
        return;
    }

    global_registry->plugins.erase(plugin_idx);
}

PluginAPI load_dynamic_plugin(const char* name)
{
    BEE_ASSERT_PLUGIN_SYSTEM();

    scoped_spinlock_t lock(global_registry->mutex);

    const auto registered_plugin_idx = find_plugin(name);
    if (registered_plugin_idx >= 0)
    {
        return global_registry->plugins[registered_plugin_idx].api;
    }

    log_info("Skyrocket: Loading plugin \"%s\"...", name);

    auto plugin = allocate_plugin(name);
    if (plugin == nullptr)
    {
        return { nullptr };
    }

    const auto file_path = fs::get_appdata().binaries_root.join(plugin->name).append_extension(DynamicLibrary::file_extension);
    plugin->lib = load_library(file_path.c_str());
    if (BEE_FAIL_F(plugin->lib.handle != nullptr, "Failed to find DLL for plugin \"%s\"", plugin->name))
    {
        return PluginAPI { nullptr };
    }

    static constexpr i32 plugin_name_capacity = 1024;
    char symbol_name[plugin_name_capacity];

    str::format(symbol_name, plugin_name_capacity, "bee_load_plugin_%s", plugin->name);
    plugin->load = (load_function_t*)get_library_symbol(plugin->lib, symbol_name);

    str::format(symbol_name, plugin_name_capacity, "bee_unload_plugin_%s", plugin->name);
    plugin->unload = (unload_function_t*)get_library_symbol(plugin->lib, symbol_name);

    const auto plugin_load_success = plugin->load != nullptr && plugin->unload != nullptr;
    if (BEE_FAIL_F(plugin_load_success, "Failed to load `bee_load_plugin_*` or `bee_unload_plugin_*` symbols for dynamic plugin"))
    {
        unload_and_destroy_plugin(plugin->name);
        return PluginAPI { nullptr };
    }

    plugin->api.ptr = plugin->load();

    log_info("Skyrocket: Plugin \"%s\" loaded successfully", name);

    // Make sure any dynamically loaded symbols are added to the debug trace system
    refresh_debug_symbols();

    return plugin->api;
}

PluginAPI load_static_plugin(const char* name, load_function_t* load, unload_function_t* unload)
{
    BEE_ASSERT_PLUGIN_SYSTEM();
    scoped_spinlock_t lock(global_registry->mutex);

    const auto registered_plugin_idx = find_plugin(name);
    if (registered_plugin_idx >= 0)
    {
        return global_registry->plugins[registered_plugin_idx].api;
    }

    log_info("Skyrocket: Loading plugin \"%s\"...", name);

    auto plugin = allocate_plugin(name);
    plugin->load = load;
    plugin->unload = unload;
    plugin->api.ptr = plugin->load();

    log_info("Skyrocket: Plugin \"%s\" loaded successfully", name);

    return plugin->api;
}

void unload_plugin(const char* name)
{
    log_info("=> Skyrocket: Unloading plugin \"%s\"...", name);

    BEE_ASSERT_PLUGIN_SYSTEM();
    scoped_spinlock_t lock(global_registry->mutex);

    unload_and_destroy_plugin(name);
}

PluginAPI get_plugin_api(const char* name)
{
    BEE_ASSERT_PLUGIN_SYSTEM();
    scoped_spinlock_t lock(global_registry->mutex);

    auto plugin_idx = find_plugin(name);
    if (BEE_FAIL_F(plugin_idx >= 0, "Plugin \"%s\" has not been loaded by the plugin_idx registry", name))
    {
        return PluginAPI { nullptr };
    }

    return global_registry->plugins[plugin_idx].api;
}

bool is_plugin_loaded(const char* name)
{
    BEE_ASSERT_PLUGIN_SYSTEM();
    scoped_spinlock_t lock(global_registry->mutex);
    return find_plugin(name) >= 0;
}


} // namespace plugins
} // namespace bee
