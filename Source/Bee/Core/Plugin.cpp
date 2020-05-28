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
static constexpr auto describe_function_name = "bee_describe_plugin";

using describe_plugin_function_t = void(*)(PluginDescriptor*);

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

    if (plugins_.size() > 0)
    {
        auto* unload_order = BEE_ALLOCA_ARRAY(Plugin*, plugins_.size());

        int plugin_index = 0;
        for (auto& plugin : plugins_)
        {
            unload_order[plugin_index] = &plugin.value;
            ++plugin_index;
        }

        std::sort(unload_order, unload_order + plugins_.size(), [&](Plugin* lhs, Plugin* rhs)
        {
            return lhs->desc.dependency_count > rhs->desc.dependency_count;
        });

        for (int i = 0; i < plugins_.size(); ++i)
        {
            unload_plugin(unload_order[i]);
        }
    }

    for (auto& module : modules_)
    {
        destroy_module(module);
    }

    modules_.clear();
    observers_.clear();
    persistent_.clear();
    search_paths_.clear();
    plugins_.clear();
    load_stack_.clear();
}

bool PluginRegistry::add_search_path(const Path &path, const RegisterPluginMode register_mode)
{
    if (search_paths_.find(path) == nullptr)
    {
        search_paths_.insert(path, {});
    }

    if (!register_plugins_at_path(path, register_mode))
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

bool PluginRegistry::load_plugin(const StringView& name, const PluginVersion& required_version)
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
        if (required_version == plugin_version_any)
        {
            return true;
        }

        return required_version == plugin->value.desc.version;
    }

    return load_plugin(&plugin->value, required_version);
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
                load_plugin(&plugin->value, plugin_version_any);
                break;
            }
            default: break;
        }
    }
}

bool PluginRegistry::is_plugin_registered(const StringView &name)
{
    return plugins_.find(get_hash(name)) != nullptr;
}

bool PluginRegistry::is_plugin_loaded(const StringView &name, const PluginVersion& version)
{
    auto* plugin = plugins_.find(get_hash(name));
    return plugin != nullptr && plugin->value.is_loaded && plugin->value.desc.version == version;
}

i32 PluginRegistry::get_loaded_plugins(PluginDescriptor* descriptors)
{
    int loaded_count = 0;

    for (auto& plugin : plugins_)
    {
        if (plugin.value.is_loaded)
        {
            if (descriptors != nullptr)
            {
                descriptors[loaded_count] = plugin.value.desc;
            }

            ++loaded_count;
        }
    }

    return loaded_count;
}

void PluginRegistry::add_observer(plugin_observer_t observer, void* user_data)
{
    const auto index = find_index_if(observers_, [&](const Observer& o)
    {
        return o.callback == observer && o.user_data == user_data;
    });

    if (index >= 0)
    {
        log_error("Observer %p is already registered", observer);
        return;
    }

    observers_.emplace_back(observer, user_data);
}

void PluginRegistry::remove_observer(plugin_observer_t observer, void* user_data)
{
    const auto index = find_index_if(observers_, [&](const Observer& o)
    {
        return o.callback == observer && o.user_data == user_data;
    });

    if (index < 0)
    {
        log_error("Observer %p is not registered", observer);
        return;
    }

    observers_.erase(index);
}


PluginRegistry::Module& PluginRegistry::get_or_create_module(const StringView& name)
{
    const auto hash = get_hash(name);
    auto index = find_index_if(modules_, [&](const Module* m)
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

    notify_observers(PluginEventType::add_module, load_stack_.back(), &module);
}

void PluginRegistry::remove_module(const void* interface_ptr)
{
    auto index = find_index_if(modules_, [&](const Module* m)
    {
        return m->current == interface_ptr;
    });

    if (index < 0)
    {
        return;
    }

    // notify observers of the removed interface first in case observers need to call module functions
    notify_observers(PluginEventType::add_module, load_stack_.back(), modules_[index]);

    destroy_module(modules_[index]);
    modules_.erase(index);
}


void* PluginRegistry::get_module(const StringView& name)
{
    return get_or_create_module(name).storage;
}

bool PluginRegistry::has_module(const StringView& name)
{
    const auto hash = get_hash(name);
    auto index = find_index_if(modules_, [&](const Module* m)
    {
        return m->hash == hash;
    });
    return index >= 0;
}

bool PluginRegistry::register_plugins_at_path(const Path& root, const RegisterPluginMode register_mode)
{
    for (const auto& path : fs::read_dir(root))
    {
        if (fs::is_dir(path))
        {
            if (!register_plugins_at_path(path, register_mode))
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

        auto parent_path = path.parent_path(temp_allocator());
        auto* search_path = search_paths_.find(parent_path.view());

        while (search_path == nullptr)
        {
            parent_path = parent_path.parent_path(temp_allocator());
            search_path = search_paths_.find(parent_path.view());
        }

        BEE_ASSERT(search_path != nullptr);
        search_path->value.push_back(name_hash);

        if (register_mode == RegisterPluginMode::auto_load)
        {
            load_plugin(&existing->value, plugin_version_any);
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
        const auto search_index = find_index_if(search_path->value, [&](const u32 nh)
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

bool PluginRegistry::load_plugin(Plugin* plugin, const PluginVersion& required_version)
{
    static thread_local StaticString<64> timestamp_buffer;

    // find as static plugin
    auto* static_plugin = g_static_plugins.find(plugin->name_hash);

    DynamicLibrary new_library{};
    load_plugin_function_t new_load_function = nullptr;
    describe_plugin_function_t new_describe_function = nullptr;

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
            return false;
        }

        new_library = load_library(plugin->current_version_path.c_str());

        if (BEE_FAIL_F(new_library.handle != nullptr, "Failed to load plugin at path: %s", plugin->library_path.c_str()))
        {
            return false;
        }

        new_describe_function = reinterpret_cast<describe_plugin_function_t>(get_library_symbol(new_library, describe_function_name));

        if (BEE_FAIL_F(new_describe_function != nullptr,
            "Failed to load function symbol `%s` for plugin at path %s", describe_function_name,
            plugin->library_path.c_str()))
        {
            unload_library(new_library);
            return false;
        }

        new_load_function = reinterpret_cast<load_plugin_function_t>(get_library_symbol(new_library, load_function_name));

        if (BEE_FAIL_F(new_load_function != nullptr, "Failed to get load function symbol `%s` for plugin at path: %s", load_function_name, plugin->library_path.c_str()))
        {
            unload_library(new_library);
            return false;
        }
    }

    if (!refresh_debug_symbols())
    {
        log_error("Failed to refresh debug symbols after loading plugin at path: %s", plugin->library_path.c_str());
    }

    // Load dependencies first
    new_describe_function(&plugin->desc);

    if (required_version != plugin_version_any && plugin->desc.version != required_version)
    {
        log_error("Failed to load required plugin version");
        unload_library(new_library);
        return false;
    }

    for (int i = 0; i < plugin->desc.dependency_count; ++i)
    {
        if (!is_plugin_registered(plugin->desc.dependencies[i].name))
        {
            log_error("Plugin dependency \"%s\" is required by %s but not registered", plugin->desc.dependencies[i].name, plugin->name.c_str());
            unload_library(new_library);
            return false;
        }

        if (!load_plugin(plugin->desc.dependencies[i].name, plugin->desc.dependencies[i].version))
        {
            unload_library(new_library);
            return false;
        }
    }

    plugin->source_path = fs::get_root_dirs().install_root.join(plugin->desc.source_location);

    // Load the plugin - save the currently loading one onto a stack for referencing later
    load_stack_.push_back(plugin);
    {
        new_load_function(this, PluginState::loading);
    }
    BEE_ASSERT(!load_stack_.empty() && load_stack_.back() == plugin);
    load_stack_.pop_back();

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

    notify_observers(PluginEventType::load_plugin, plugin, nullptr);
    return true;
}

void PluginRegistry::unload_plugin(Plugin* plugin)
{
    BEE_ASSERT(plugin->load_function != nullptr);

    // Notify before unloading in case any observers need to call plugin functions
    notify_observers(PluginEventType::unload_plugin, plugin, nullptr);

    load_stack_.push_back(plugin);
    {
        plugin->load_function(this, PluginState::unloading);
    }
    BEE_ASSERT(!load_stack_.empty() && load_stack_.back() == plugin);
    load_stack_.pop_back();

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

void PluginRegistry::notify_observers(const PluginEventType event, const Plugin* plugin, const Module* module)
{
    StringView module_name = module == nullptr ? StringView{} : module->name.view();
    void* interface_ptr = module == nullptr ? nullptr : module->storage;

    for (auto& observer : observers_)
    {
        observer.callback(event, plugin->desc, module_name, interface_ptr, observer.user_data);
    }
}


} // namespace bee
