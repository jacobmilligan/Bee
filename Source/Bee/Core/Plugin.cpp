/*
 *  Plugin.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/DynamicLibrary.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Reflection.hpp"

#include <algorithm>


namespace bee {


/*
 *************************
 *
 * Plugin registry state
 *
 *************************
 */
using load_plugin_t = void (*)(PluginLoader* loader, const PluginState state);
using get_version_t = void (*)(PluginVersion* version);
using load_reflection_t = void* (*)();

static constexpr auto g_load_plugin_name = "bee_load_plugin";
static constexpr auto g_get_version_name = "bee_get_plugin_version";
static constexpr auto g_load_reflection_name = "bee_load_reflection";

#if BEE_OS_WINDOWS == 1
static constexpr auto g_plugin_extension = ".dll";
#elif BEE_OS_MACOS == 1
static constexpr auto g_plugin_extension = ".dylib";
#elif BEE_OS_LINUX == 1
    static constexpr auto g_plugin_extension = ".so";
#else
    #error Unsupported platform
#endif // BEE_OS_WINDOWS == 1

static constexpr size_t g_max_module_size = 1024;

struct ModuleHeader
{
    StaticString<256>   name;
    i32                 references { -1 };
};

struct Plugin
{
    String              name;
    PluginVersion       version;
    Path                library_path;
    Path                hot_reload_path;
    DynamicLibrary      library;
    PluginState         state { PluginState::unloaded };
    load_plugin_t       load_function { nullptr };
    get_version_t       get_version_function { nullptr };
    ReflectionModule*   reflection_module { nullptr };
    DynamicArray<void*> static_data;
    DynamicArray<u32>   static_data_hashes;
};

struct PluginRegistry
{
    DynamicArray<Plugin>                    plugins;
    DynamicArray<u32>                       plugin_hashes;

    DynamicArray<UniquePtr<ModuleHeader>>   modules;
    DynamicArray<u32>                       module_hashes;

    fs::DirectoryWatcher                    directory_watcher;
    DynamicArray<fs::FileNotifyInfo>        file_events;

    DynamicArray<Plugin*>                   load_stack;
};

PluginRegistry* g_registry = nullptr;


/*
 **********************************************
 *
 * Load stack utilities - used for determining
 * which plugin is currently loading for
 * i.e. static data association
 *
 **********************************************
 */
struct PluginLoadScope
{
    Plugin* plugin { nullptr };

    explicit PluginLoadScope(Plugin* current_plugin)
        : plugin(current_plugin)
    {
        g_registry->load_stack.push_back(plugin);
    }

    ~PluginLoadScope()
    {
        BEE_ASSERT(!g_registry->load_stack.empty() && g_registry->load_stack.back() == plugin);
        g_registry->load_stack.pop_back();
    }
};

#define BEE_PLUGIN_LOAD_SCOPE(plugin) PluginLoadScope BEE_CONCAT(plugin_load_scope_, __LINE__)(plugin)


static Plugin* get_loading_plugin()
{
    if (g_registry->load_stack.empty())
    {
        return nullptr;
    }

    return g_registry->load_stack.back();
}


/*
 ****************************************
 *
 * Plugin registry API implementation
 *
 ****************************************
 */
void init_plugins()
{
    if (g_registry != nullptr)
    {
        log_error("Plugin registry is already initialized");
        return;
    }

    g_registry = BEE_NEW(system_allocator(), PluginRegistry);
}

void shutdown_plugins()
{
    if (g_registry == nullptr)
    {
        log_error("Plugin registry is already shutdown");
        return;
    }

    BEE_DELETE(system_allocator(), g_registry);
}

static bool reload_plugin(Plugin* plugin)
{
    static thread_local StaticString<64>    timestamp;
    static thread_local Path                old_hot_reload_path;

    const bool reload = plugin->state == PluginState::loaded;

    /*
     * For DLL plugins we need to copy the OG file to a random path and instead load from *that* file
     * to get around windows .dll locking woes
     */
    str::to_static_string(time::now(), &timestamp);

    if (reload)
    {
        old_hot_reload_path.clear();
        old_hot_reload_path.append(plugin->hot_reload_path);
    }

    plugin->hot_reload_path.clear();
    plugin->hot_reload_path.append(plugin->library_path);
    plugin->hot_reload_path.set_extension(timestamp.view());
    plugin->hot_reload_path.append_extension(plugin->library_path.extension());

    g_registry->directory_watcher.suspend();
    const auto copy_success = fs::copy(plugin->library_path, plugin->hot_reload_path);
    g_registry->directory_watcher.resume();

    BEE_ASSERT_F(copy_success, "Failed to copy plugin to hot reload path at %s", plugin->hot_reload_path.c_str());

    auto new_library = load_library(plugin->hot_reload_path.c_str());

    if (BEE_FAIL_F(new_library.handle != nullptr, "Failed to load plugin at path: %s", plugin->library_path.c_str()))
    {
        return false;
    }

    auto* new_version_function = reinterpret_cast<get_version_t>(get_library_symbol(new_library, g_get_version_name));

    if (BEE_FAIL_F(new_version_function != nullptr, "Failed to get load function symbol `%s` for plugin at path: %s", g_get_version_name, plugin->library_path.c_str()))
    {
        unload_library(new_library);
        return false;
    }

    auto* new_load_function = reinterpret_cast<load_plugin_t>(get_library_symbol(new_library, g_load_plugin_name));

    if (BEE_FAIL_F(new_load_function != nullptr, "Failed to get load function symbol `%s` for plugin at path: %s", g_load_plugin_name, plugin->library_path.c_str()))
    {
        unload_library(new_library);
        return false;
    }

    if (!refresh_debug_symbols())
    {
        log_error("Failed to refresh debug symbols after loading plugin at path: %s", plugin->library_path.c_str());
    }

    plugin->state = PluginState::loading;

    PluginLoader loader{};
    {
        BEE_PLUGIN_LOAD_SCOPE(plugin);
        new_version_function(&plugin->version);
        new_load_function(&loader, PluginState::loading);
    }

    if (BEE_FAIL_F(plugin->version.major >= 0, "No version set in `%s` for plugin at path: %s", g_load_plugin_name, plugin->library_path.c_str()))
    {
        unload_library(new_library);
        return false;
    }

    if (reload)
    {
        plugin->load_function(&loader, PluginState::unloading);
        unload_library(plugin->library);

        if (!refresh_debug_symbols())
        {
            log_error("Failed to refresh debug symbols after loading plugin at path: %s", plugin->library_path.c_str());
        }

        if (old_hot_reload_path.exists())
        {
            g_registry->directory_watcher.suspend();
            fs::remove(old_hot_reload_path);
            g_registry->directory_watcher.resume();
        }
    }

    // Now that we 100% have a successfully loaded plugin it's safe to load/reload reflection data
    if (plugin->reflection_module != nullptr)
    {
        // reload
        destroy_reflection_module(plugin->reflection_module);
    }

    // get the new reflection module
    auto* new_load_reflection_function = reinterpret_cast<load_reflection_t>(get_library_symbol(new_library, g_load_reflection_name));
    if (new_load_reflection_function != nullptr)
    {
        plugin->reflection_module = static_cast<ReflectionModule*>(new_load_reflection_function());
        BEE_ASSERT(plugin->reflection_module != nullptr);
    }

    plugin->state = PluginState::loaded;
    plugin->library = new_library;
    plugin->load_function = new_load_function;
    plugin->get_version_function = new_version_function;
    log_info("%s plugin: %s", reload ? "Reloaded" : "Loaded", plugin->name.c_str());
    return true;
}

static i32 find_plugin(const StringView& name)
{
    const u32 hash = get_hash(name);
    return find_index(g_registry->plugin_hashes, hash);
}

static i32 find_module(const StringView& name)
{
    const u32 hash = get_hash(name);
    return find_index(g_registry->module_hashes, hash);
}

bool load_plugin(const StringView& name)
{
    const i32 index = find_plugin(name);

    BEE_ASSERT_F(index >= 0, "%" BEE_PRIsv " is not a registered plugin", BEE_FMT_SV(name));

    auto& plugin = g_registry->plugins[index];
    if (plugin.state == PluginState::loaded || plugin.state == PluginState::loading)
    {
        return true;
    }

    return reload_plugin(&plugin);
}

static void unload_plugin(Plugin* plugin)
{
    if (plugin->state == PluginState::unloaded || plugin->state == PluginState::unloading)
    {
        return;
    }

    plugin->state = PluginState::unloading;

    PluginLoader loader{};
    {
        BEE_PLUGIN_LOAD_SCOPE(plugin);
        plugin->load_function(&loader, PluginState::unloading);
    }

    // unload the dyn lib if this isn't a static plugin
    if (plugin->library.handle != nullptr)
    {
        unload_library(plugin->library);
    }

    if (plugin->hot_reload_path.exists())
    {
        fs::remove(plugin->hot_reload_path);
    }

    // Free the static data now the plugin is unloaded
    for (auto& static_data : plugin->static_data)
    {
        if (static_data != nullptr)
        {
            BEE_FREE(system_allocator(), static_data);
        }
    }

    // unload the reflection module if there is one
    if (plugin->reflection_module != nullptr)
    {
        destroy_reflection_module(plugin->reflection_module);
    }

    plugin->static_data.clear();
    plugin->static_data_hashes.clear();

    // reset the plugins state
    plugin->state = PluginState::unloaded;
    log_info("Unloaded plugin: %s", plugin->name.c_str());
}

void unload_plugin(const StringView& name)
{
    const i32 index = find_plugin(name);

    BEE_ASSERT_F(index >= 0, "%" BEE_PRIsv " is not a registered plugin", BEE_FMT_SV(name));

    unload_plugin(&g_registry->plugins[index]);
}

static bool is_temp_hot_reload_file(const Path& path)
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

static void register_plugin(const Path& lib_path)
{
    if (lib_path.extension() != g_plugin_extension)
    {
        return;
    }

    const auto name = lib_path.stem();
    if (find_plugin(name) >= 0)
    {
        log_error("Plugin \"%" BEE_PRIsv "\" is already registered", BEE_FMT_SV(name));
        return;
    }

    /*
     * skip registering any .dll with the pattern <Name>.<timestamp>.dll
     * but clean it from the filesystem
     */
    if (is_temp_hot_reload_file(lib_path))
    {
        g_registry->directory_watcher.suspend();
        fs::remove(lib_path);
        g_registry->directory_watcher.resume();
    }

    Plugin plugin;
    plugin.name = name;
    plugin.state = PluginState::unloaded;
    plugin.library_path = lib_path;
    g_registry->plugins.push_back(plugin);
    g_registry->plugin_hashes.push_back(get_hash(name));
}

static void unregister_plugin(const Path& lib_path)
{
    const i32 index = find_plugin(lib_path.stem());
    if (index < 0)
    {
        return;
    }

    unload_plugin(lib_path.stem());

    // unregister from the whole registry
    g_registry->plugins.erase(index);
    g_registry->plugin_hashes.erase(index);
}

void refresh_plugins()
{
    // Refresh the list of plugins and sort alphabetically before handling the pending events
    auto& file_events = g_registry->file_events;
    g_registry->directory_watcher.pop_events(&file_events);

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
                register_plugin(event.file);
                break;
            }
            case fs::FileAction::removed:
            {
                unregister_plugin(event.file);
                break;
            }
            case fs::FileAction::modified:
            {
                const i32 index = find_plugin(event.file.stem());
                if (index >= 0)
                {
                    reload_plugin(&g_registry->plugins[index]);
                }
                break;
            }
            default: break;
        }
    }
}

void add_plugin_search_path(const Path& path)
{
    for (const auto& file : fs::read_dir(path))
    {
        register_plugin(file);
    }
    g_registry->directory_watcher.add_directory(path);
}

void remove_plugin_search_path(const Path& path)
{
    g_registry->directory_watcher.remove_directory(path);
}

void* get_module(const StringView& name)
{
    const i32 index = find_module(name);
    if (index < 0)
    {
        log_error("No module added with name %" BEE_PRIsv, BEE_FMT_SV(name));
        return nullptr;
    }

    return reinterpret_cast<u8*>(g_registry->modules[index].get()) + sizeof(ModuleHeader);
}

bool PluginLoader::get_static(void** dst, const u32 hash, const size_t size, const size_t alignment)
{
    auto* plugin = get_loading_plugin();
    BEE_ASSERT(plugin != nullptr);

    const i32 index = find_index(plugin->static_data_hashes, hash);
    if (index >= 0)
    {
        *dst = plugin->static_data[index];
        return true;
    }

    plugin->static_data.push_back(BEE_MALLOC_ALIGNED(system_allocator(), size, alignment));
    plugin->static_data_hashes.push_back(hash);

    *dst = plugin->static_data.back();
    memset(*dst, 0, size);

    return false;
}

void* PluginLoader::get_module(const StringView& name)
{
    return ::bee::get_module(name);
}

bool PluginLoader::require_plugin(const StringView& name, const PluginVersion& minimum_version) const
{
    const i32 index = find_plugin(name);

    if (index < 0)
    {
        log_error("No plugin registered with name %" BEE_PRIsv, BEE_FMT_SV(name));
        return false;
    }

    auto& plugin = g_registry->plugins[index];

    bool unload_if_incompatible = plugin.state == PluginState::unloaded;

    if (plugin.state == PluginState::unloaded)
    {
        reload_plugin(&plugin);
    }

    if (plugin.version >= minimum_version)
    {
        return true;
    }

    log_error(
        "Registered version for plugin %" BEE_PRIsv " is %d.%d.%d but required version is %d.%d.%d",
        BEE_FMT_SV(name),
        plugin.version.major,
        plugin.version.minor,
        plugin.version.patch,
        minimum_version.major,
        minimum_version.minor,
        minimum_version.patch
    );

    if (unload_if_incompatible)
    {
        unload_plugin(&plugin);
    }

    return false;
}

static void* get_module_storage(const i32 index)
{
    return reinterpret_cast<u8*>(g_registry->modules[index].get()) + sizeof(ModuleHeader);
}

void PluginLoader::add_module_interface(const StringView& name, const void* module, const size_t module_size)
{
    i32 index = find_module(name);
    if (index >= 0 && g_registry->modules[index]->references > 0)
    {
        log_error("Module %" BEE_PRIsv " was already added to the plugin registry", BEE_FMT_SV(name));
        return;
    }

    if (sizeof(ModuleHeader) + module_size >= g_max_module_size)
    {
        log_error("Module %" BEE_PRIsv " exceeds the maximum size allowed for moudles (%zu)", BEE_FMT_SV(name), g_max_module_size);
        return;
    }

    if (index < 0)
    {
        auto* ptr = BEE_MALLOC(system_allocator(), g_max_module_size);
        memset(ptr, 0, g_max_module_size);
        auto* header = static_cast<ModuleHeader*>(ptr);
        header->references = 0;
        header->name = name;
        g_registry->modules.emplace_back(header, system_allocator());
        g_registry->module_hashes.push_back(get_hash(name));

        index = g_registry->modules.size() - 1;
    }

    ++g_registry->modules[index]->references;
    auto* storage = get_module_storage(index);
    memcpy(storage, module, module_size);
}

void PluginLoader::remove_module_interface(const void* module)
{
    const i32 index = find_index_if(g_registry->modules, [&](const UniquePtr<ModuleHeader>& m)
    {
        return m.get() == module;
    });

    if (index < 0)
    {
        log_error("Unable to find module with address %p in the plugin registry", module);
        return;
    }

    const i32 references = g_registry->modules[index]->references--;
    if (references < 0)
    {
        g_registry->modules.erase(index);
    }
}


} // namespace bee
