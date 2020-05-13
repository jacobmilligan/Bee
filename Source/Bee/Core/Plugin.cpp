/*
 *  PluginV2.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Time.hpp"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/Debug.hpp"

#include <algorithm>
#include <inttypes.h>

namespace bee {

#if BEE_OS_WINDOWS == 1
    static constexpr auto g_plugin_extension = ".dll";
#elif BEE_OS_MACOS == 1
    static constexpr auto g_plugin_extension = ".dylib";
#elif BEE_OS_LINUX == 1
    static constexpr auto g_plugin_extension = ".so";
#else
    #error Unsupported platform
#endif // BEE_OS_WINDOWS == 1

static constexpr auto g_pdb_extension = ".pdb";

static constexpr auto load_function_name = "bee_load_plugin";
static constexpr auto unload_function_name = "bee_unload_plugin";

static DynamicHashMap<u32, StaticPluginAutoRegistration*>   g_static_plugins;
static StaticPluginAutoRegistration*                        g_static_plugin_pending_registrations { nullptr };

StaticPluginAutoRegistration::StaticPluginAutoRegistration(const char* name, load_plugin_function_t load_function)
    : load_plugin(load_function)
{
    const auto name_hash = get_hash(name);
    auto* existing = g_static_plugins.find(name_hash);
    if (existing != nullptr)
    {
        log_error("Plugin \"%s\" was registered multiple times", name);
        return;
    }

    if (g_static_plugin_pending_registrations == nullptr)
    {
        g_static_plugin_pending_registrations = this;
    }
    else
    {
        g_static_plugin_pending_registrations->next = this;
    }

    g_static_plugins.insert(name_hash, this);
}


/*
 ************************************
 *
 * PluginRegistry - implementation
 *
 ************************************
 */
PluginRegistry::PluginRegistry()
{
    directory_watcher_.start("PluginWatcher");
}

PluginRegistry::~PluginRegistry()
{
    if (directory_watcher_.is_running())
    {
        directory_watcher_.stop();
    }

    for (auto& plugin : plugins_)
    {
        unload_plugin(&plugin.value);
    }

    for (auto& module : modules_)
    {
        destroy_module(module);
    }
}

bool PluginRegistry::add_search_path(const Path &path, const RegisterPluginMode register_mode)
{
    if (search_paths_.find(path) == nullptr)
    {
        search_paths_.insert(path, {});
    }

    if (!register_plugins_at_path(path, path, register_mode))
    {
        return false;
    }

    return directory_watcher_.add_directory(path);
}

void PluginRegistry::remove_search_path(const Path& path)
{
    auto* search_path = search_paths_.find(path);

    if (search_path == nullptr)
    {
        log_error("%s is not a registered plugin search path", path.c_str());
        return;
    }

    directory_watcher_.remove_directory(path);

    for (auto& name_hash : search_path->value)
    {
        auto* registered = plugins_.find(name_hash);

        if (registered == nullptr)
        {
            log_error("Unable to find plugin with hash %u", name_hash);
            return;
        }

        auto& plugin = registered->value;

        if (plugin.is_loaded)
        {
            unload_plugin(&plugin);
        }

        plugins_.erase(name_hash);
    }

    search_paths_.erase(path);
}

bool PluginRegistry::load_plugin(const StringView& name)
{
    const auto name_hash = get_hash(name);
    auto* plugin = plugins_.find(name_hash);

    if (plugin == nullptr)
    {
        log_error("Could not find a registered plugin for \"%" BEE_PRIsv "\"", BEE_FMT_SV(name));
        return false;
    }

    if (plugin->value.is_loaded)
    {
        log_error("Plugin \"%" BEE_PRIsv "\" is already loaded", BEE_FMT_SV(name));
        return false;
    }

    load_plugin(&plugin->value);
    return true;
}

bool PluginRegistry::unload_plugin(const StringView& name)
{
    const auto name_hash = get_hash(name);
    auto* plugin = plugins_.find(name_hash);

    if (plugin == nullptr)
    {
        log_error("Could not find a registered plugin for \"%" BEE_PRIsv "\"", BEE_FMT_SV(name));
        return false;
    }

    if (!plugin->value.is_loaded)
    {
        log_error("Plugin \"%" BEE_PRIsv "\" is already unloaded", BEE_FMT_SV(name));
        return false;
    }

    unload_plugin(&plugin->value);
    return true;
}

void PluginRegistry::refresh_plugins()
{
    // Refresh the list of plugins and sort alphabetically before handling the pending events
    auto file_events = directory_watcher_.pop_events();
    std::sort(file_events.begin(), file_events.end(), [&](const fs::FileNotifyInfo& lhs, const fs::FileNotifyInfo& rhs)
    {
        return lhs.file < rhs.file;
    });

    for (auto& event : file_events)
    {
        const auto ext = event.file.extension();
        if (ext != g_plugin_extension)
        {
            continue;
        }

        switch (event.action)
        {
            case fs::FileAction::added:
            {
                register_plugin(event.file, RegisterPluginMode::manual_load);
                break;
            }
            case fs::FileAction::removed:
            {
                unregister_plugin(event.file);
                break;
            }
            case fs::FileAction::modified:
            {
                auto* plugin = plugins_.find(get_hash(event.file.stem()));
                BEE_ASSERT(plugin != nullptr);
                load_plugin(&plugin->value);
                break;
            }
            default: break;
        }
    }
}

bool PluginRegistry::is_plugin_loaded(const StringView &name)
{
    return plugins_.find(get_hash(name)) != nullptr;
}

void PluginRegistry::add_observer(plugin_observer_t observer, void* user_data)
{
    const auto index = container_index_of(observers_, [&](const Observer& o)
    {
        return o.callback == observer;
    });

    if (index >= 0)
    {
        log_error("Observer %p is already registered", observer);
        return;
    }

    observers_.emplace_back(observer, user_data);
}

void PluginRegistry::remove_observer(plugin_observer_t observer)
{
    const auto index = container_index_of(observers_, [&](const Observer& o)
    {
        return o.callback == observer;
    });

    if (index < 0)
    {
        log_error("Observer %p is not registered", observer);
        return;
    }

    observers_.erase(index);
}

void PluginRegistry::require_module(const StringView &module_name, module_observer_t observer, void *user_data)
{
    auto& module = get_or_create_module(module_name);

    // call the observer immediately if the module is already loaded
    if (module.current != nullptr)
    {
        observer(module.storage, user_data);
    }
    else
    {
        module.on_add_observers.emplace_back(observer, user_data);
    }
}

PluginRegistry::Module& PluginRegistry::get_or_create_module(const StringView& name)
{
    const auto hash = get_hash(name);
    auto index = container_index_of(modules_, [&](const Module* m)
    {
        return m->hash == hash;
    });

    // Reserve the module if it hasn't been added yet
    if (index >= 0)
    {
        return *modules_[index];
    }

    modules_.push_back(create_module(hash, name));

    return *modules_.back();
}

void PluginRegistry::add_module(const StringView& name, const void* interface_ptr, const size_t size)
{
    if (BEE_FAIL_F(size <= module_storage_capacity_, "Module interface struct is too large to store in plugin registry"))
    {
        return;
    }

    auto& module = get_or_create_module(name);

    module.current = interface_ptr;
    memcpy(module.storage, interface_ptr, size);

    // notify the once-off module-specific load callbacks
    for (auto& observer : module.on_add_observers)
    {
        observer.callback(module.storage, observer.user_data);
    }

    module.on_add_observers.clear();

    // notify observers of the new interface
    for (auto& observer : observers_)
    {
        observer.callback(PluginEventType::add_module, module.name.view(), module.storage, observer.user_data);
    }
}

void PluginRegistry::remove_module(const void* interface_ptr)
{
    auto index = container_index_of(modules_, [&](const Module* m)
    {
        return m->current == interface_ptr;
    });

    if (index < 0)
    {
        return;
    }

    // notify observers of the removed interface
    for (auto& observer : observers_)
    {
        observer.callback(PluginEventType::remove_module, modules_[index]->name.view(), modules_[index]->storage, observer.user_data);
    }

    destroy_module(modules_[index]);
    modules_.erase(index);
}


void* PluginRegistry::get_module(const StringView& name)
{
    return get_or_create_module(name).storage;
}

bool PluginRegistry::register_plugins_at_path(const Path& search_root, const Path& root, const RegisterPluginMode register_mode)
{
    for (const auto& path : fs::read_dir(root))
    {
        if (fs::is_dir(path))
        {
            if (!register_plugins_at_path(search_root, root, register_mode))
            {
                return false;
            }

            continue;
        }

        register_plugin(path, register_mode);
    }

    return true;
}

bool is_temp_hot_reload_file(const Path& path)
{
    const auto name = path.stem();
    const auto timestamp_begin = str::last_index_of(name, '.') + 1;

    for (int i = timestamp_begin; i < name.size(); ++i)
    {
        if (!str::is_digit(name[i]))
        {
            return false;
        }
    }

    return true;
}

void PluginRegistry::register_plugin(const Path& path, const RegisterPluginMode register_mode)
{
    const auto ext = path.extension();
    const auto is_plugin = ext == g_plugin_extension;
    const auto is_pdb = ext == g_pdb_extension;

    if (!is_plugin && !is_pdb)
    {
        return;
    }

    if (is_temp_hot_reload_file(path))
    {
        directory_watcher_.suspend();
        fs::remove(path);
        directory_watcher_.resume();
        return;
    }

    if (is_plugin)
    {
        // Register the plugin
        const auto name = path.stem();
        const auto name_hash = get_hash(name);
        auto* existing = plugins_.find(name_hash);

        if (existing != nullptr)
        {
            log_error("A plugin is already registered with the name \"%" BEE_PRIsv "\"", BEE_FMT_SV(name));
            return;
        }

        existing = plugins_.insert(name_hash, Plugin(path, name));

        auto* search_path = search_paths_.find(path.parent_view());
        BEE_ASSERT(search_path != nullptr);
        search_path->value.push_back(name_hash);

        if (register_mode == RegisterPluginMode::auto_load)
        {
            load_plugin(&existing->value);
        }
    }
}

void PluginRegistry::unregister_plugin(const Path& path)
{
    const auto name_hash = get_hash(path.stem());
    auto* plugin = plugins_.find(name_hash);
    auto* search_path = search_paths_.find(path.parent_view());

    BEE_ASSERT(search_path != nullptr);

    if (plugin != nullptr)
    {
        const auto search_index = container_index_of(search_path->value, [&](const u32 nh)
        {
            return nh == name_hash;
        });

        search_path->value.erase(search_index);

        if (plugin->value.is_loaded)
        {
            unload_plugin(&plugin->value);
        }

        plugins_.erase(plugin->value.name_hash);
    }
}

void PluginRegistry::load_plugin(Plugin* plugin)
{
    static thread_local StaticString<64> timestamp_buffer;

    // find as static plugin
    auto* static_plugin = g_static_plugins.find(plugin->name_hash);

    DynamicLibrary new_library{};
    load_plugin_function_t new_load_function = nullptr;

    if (static_plugin != nullptr)
    {
        new_load_function = static_plugin->value->load_plugin;
    }
    else
    {
        /*
         * Dynamic plugin - register via loading the DLL/dylib/so
         *
         * First we need to copy the OG file to a random path and instead load from *that* file
         * to get around windows .dll locking woes
         */
        timestamp_buffer.resize(timestamp_buffer.capacity());
        const auto size = str::format_buffer(timestamp_buffer.data(), timestamp_buffer.size(), "%" PRIu64, time::now());
        timestamp_buffer.resize(size);

        plugin->current_version_path.clear();
        plugin->current_version_path.append(plugin->library_path);
        plugin->current_version_path.set_extension(timestamp_buffer.view());
        plugin->current_version_path.append_extension(plugin->library_path.extension());

        directory_watcher_.suspend();
        const auto copy_success = fs::copy(plugin->library_path, plugin->current_version_path);
        directory_watcher_.resume();

        if (!copy_success)
        {
            return;
        }

        new_library = load_library(plugin->current_version_path.c_str());

        if (BEE_FAIL_F(new_library.handle != nullptr, "Failed to load plugin at path: %s", plugin->library_path.c_str()))
        {
            return;
        }

        new_load_function = reinterpret_cast<load_plugin_function_t>(get_library_symbol(new_library, load_function_name));

        if (BEE_FAIL_F(new_load_function != nullptr, "Failed to get load function symbol `%s` for plugin at path: %s", load_function_name, plugin->library_path.c_str()))
        {
            unload_library(new_library);
            return;
        }
    }

    if (!refresh_debug_symbols())
    {
        log_error("Failed to refresh debug symbols after loading plugin at path: %s", plugin->library_path.c_str());
    }

    // Load the new plugin version

    new_load_function(this, PluginState::loading);

    if (plugin->is_loaded && static_plugin == nullptr)
    {
        plugin->load_function(this, PluginState::unloading);
        unload_library(plugin->library);

        if (!refresh_debug_symbols())
        {
            log_error("Failed to refresh debug symbols after loading plugin at path: %s", plugin->library_path.c_str());
        }
    }

    const auto reload = plugin->is_loaded;

    plugin->is_loaded = true;
    plugin->library = new_library;
    plugin->load_function = new_load_function;

    // Delete the old version of the dll if any exist
    if (!plugin->old_version_path.empty())
    {
        directory_watcher_.suspend();
        fs::remove(plugin->old_version_path);

        // Remove the old PDB if one exists
        plugin->old_version_path.set_extension(".pdb");
        if (plugin->old_version_path.exists())
        {
            fs::remove(plugin->old_version_path);
        }

        directory_watcher_.resume();

        plugin->old_version_path = plugin->current_version_path;
    }

    log_info("%s plugin: %s", reload ? "Reloaded" : "Loaded", plugin->name.c_str());
}

void PluginRegistry::unload_plugin(Plugin* plugin)
{
    BEE_ASSERT(plugin->load_function != nullptr);

    plugin->load_function(this, PluginState::unloading);

    // unload the dyn lib if this isn't a static plugin
    if (plugin->library.handle != nullptr)
    {
        unload_library(plugin->library);
    }

    if (plugin->current_version_path.exists())
    {
        fs::remove(plugin->current_version_path);
    }

    if (plugin->old_version_path.exists())
    {
        fs::remove(plugin->old_version_path);
    }

    plugin->is_loaded = false;

    log_info("Unloaded plugin: %s", plugin->name.c_str());
}

void* PluginRegistry::get_or_create_persistent(const u32 unique_hash, const size_t size)
{
    void* ptr = nullptr;
    if (!get_or_create_persistent(unique_hash, size, &ptr))
    {
        memset(ptr, 0, size);
    }
    return ptr;
}

bool PluginRegistry::get_or_create_persistent(const u32 unique_hash, const size_t size, void** out_data)
{
    BEE_ASSERT(out_data != nullptr);

    auto* keyval = persistent_.find(unique_hash);
    const auto create = keyval == nullptr;

    if (create)
    {
        keyval = persistent_.insert(unique_hash, {});
    }

    auto& persistent = keyval->value;

    if (size > persistent.size())
    {
        persistent.append(size - persistent.size(), 0);
    }

    *out_data = persistent.data();
    return !create;
}


} // namespace bee
