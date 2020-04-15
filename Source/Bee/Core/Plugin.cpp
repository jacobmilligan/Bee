/*
 *  PluginV2.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Time.hpp"

namespace bee {

#if BEE_OS_WINDOWS == 1
    static constexpr auto plugin_extension = "dll";
#elif BEE_OS_MACOS == 1
    static constexpr auto plugin_extension = "dylib";
#elif BEE_OS_LINUX == 1
    static constexpr auto plugin_extension = "so";
#else
    #error Unsupported platform
#endif // BEE_OS_WINDOWS == 1

static constexpr auto load_function_name = "bee_load_plugin";
static constexpr auto unload_function_name = "bee_unload_plugin";

static DynamicHashMap<String, StaticPluginAutoRegistration*> g_static_plugins;
static StaticPluginAutoRegistration* g_static_plugin_pending_registrations { nullptr };

StaticPluginAutoRegistration::StaticPluginAutoRegistration(const char* name, load_plugin_function_t load_function, load_plugin_function_t unload_function)
    : load_plugin(load_function),
      unload_plugin(unload_function)
{
    auto* existing = g_static_plugins.find(name);
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

    g_static_plugins.insert(String(name), this);
}


/*
 ************************************
 *
 * Plugin storage
 *
 ************************************
 */
struct Plugin
{
    u64                         timestamp { 0 };
    String                      name;
    Path                        library_path;
    DynamicLibrary              library;
    load_plugin_function_t      load_function { nullptr };
    load_plugin_function_t      unload_function { nullptr };
};

struct Interface
{
    String  api;
    i32     index {-1 };
    void*   ptr { nullptr };

    Interface() = default;

    explicit Interface(void* interface_ptr)
        : ptr(interface_ptr)
    {}
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


struct Api
{
    DynamicArray<void*>    interfaces;
    DynamicArray<Observer> observers;

    i32 find_interface(const void* interface) const
    {
        return container_index_of(interfaces, [&](const void* i)
        {
            return i == interface;
        });
    }

    i32 find_or_add_interface(void* interface)
    {
        const auto interface_index = find_interface(interface);
        if (interface_index >= 0)
        {
            return interface_index;
        }

        interfaces.push_back(interface);
        return interfaces.size() - 1;
    }

    i32 find_observer(const plugin_observer_t observer) const
    {
        return container_index_of(observers, [&](const Observer& o)
        {
            return o.callback == observer;
        });
    }
};

struct PluginEvent
{
    PluginEventType     type {PluginEventType::none };
    const char*         plugin_name { nullptr };
    const char*         api_name { nullptr };
    void*               interface { nullptr };
};

struct PluginCache
{
    bool                                initialized { false };
    RecursiveSpinLock                   mutex;
    DynamicArray<Interface>             interfaces;
    DynamicHashMap<String, Api>         apis;
    DynamicHashMap<String, Plugin>      plugins;
    DynamicArray<PluginEvent>           pending_events;
    DynamicArray<Path>                  search_paths;

    i32 find_interface(const void* interface) const
    {
        return container_index_of(interfaces, [&](const Interface& i)
        {
            return i.ptr == interface;
        });
    }

    Api& find_or_add_api(const char* name)
    {
        auto* api = apis.find(name);

        if (api != nullptr)
        {
            return api->value;
        }

        return apis.insert(String(name), Api())->value;
    }

    i32 find_or_add_interface(void* interface)
    {
        const auto interface_index = find_interface(interface);
        if (interface_index >= 0)
        {
            return interface_index;
        }

        interfaces.emplace_back(interface);
        return interfaces.size() - 1;
    }

    Span<void*> enumerate_api(const char* name)
    {
        auto* api = apis.find(name);
        if (BEE_FAIL_F(api != nullptr, "\"%s\" is not a registered plugin API", name))
        {
            return Span<void*>{};
        }

        return api->value.interfaces.span();
    }

    void add_search_path(const Path& path)
    {
        if (!fs::is_dir(path))
        {
            log_error("Plugin search path %s is not a directory", path.c_str());
            return;
        }

        const auto index = container_index_of(search_paths, [&](const Path& p)
        {
            return p == path;
        });

        if (index >= 0)
        {
            log_warning("Plugin search path %s was added multiple times", path.c_str());
            return;
        }

        search_paths.push_back(path);
    }
};

static PluginCache g_plugins;


void add_plugin_event_threadsafe(const PluginEventType type, const char* plugin_name, const char* api_name, void* interface)
{
    scoped_recursive_spinlock_t lock(g_plugins.mutex);

    PluginEvent event{};
    event.type = type;
    event.plugin_name = plugin_name;
    event.api_name = api_name;
    event.interface = interface;

    g_plugins.pending_events.push_back(event);
}

/*
 *****************************************
 *
 * # Plugin API context - implementation
 *
 * None of these functions should lock
 * because they're all called from
 * within a scoped-locked context
 *
 *****************************************
 */
PluginRegistry::PluginRegistry(PluginCache* plugins, const char* plugin_name, const bool is_reloading)
    : plugins_(plugins),
      plugin_name_(plugin_name),
      reloading_(is_reloading)
{}

void PluginRegistry::add_interface(const char* api_name, void* interface_ptr)
{
    add_plugin_event_threadsafe(PluginEventType::add_interface, plugin_name_, api_name, interface_ptr);
}

void PluginRegistry::remove_interface(void* interface_ptr)
{
    add_plugin_event_threadsafe(PluginEventType::remove_interface, plugin_name_, nullptr, interface_ptr);
}

Span<void*> PluginRegistry::enumerate_api(const char* name)
{
    return plugins_->enumerate_api(name);
}


/*
 *****************************************
 *
 * Plugin implementation
 *
 *****************************************
 */
void unload_plugin_nolock(Plugin* plugin)
{
    BEE_ASSERT(plugin->unload_function != nullptr);
    BEE_ASSERT(plugin->library.handle != nullptr);

    PluginRegistry ctx(&g_plugins, plugin->name.c_str(), false);
    plugin->unload_function(&ctx);

    // unload the dyn lib if this isn't a static plugin
    if (plugin->library.handle != nullptr)
    {
        unload_library(plugin->library);
    }
}

void process_load_event(const PluginEvent& event)
{
    // see if the plugin is registered as a static one first
    Plugin plugin;
    plugin.name = event.plugin_name;

    auto* static_plugin = g_static_plugins.find(event.plugin_name);

    if (static_plugin != nullptr)
    {
        plugin.load_function = static_plugin->value->load_plugin;
        plugin.unload_function = static_plugin->value->unload_plugin;
    }
    else
    {
        // Dynamic plugin - register via loading the DLL/dylib/so
        plugin.library_path = fs::get_appdata().binaries_root.join(event.plugin_name);
        plugin.library_path.append_extension(plugin_extension);

        plugin.library = load_library(plugin.library_path.c_str());

        if (BEE_FAIL_F(plugin.library.handle != nullptr, "Failed to load plugin at path: %s", plugin.library_path.c_str()))
        {
            return;
        }

        plugin.load_function = reinterpret_cast<load_plugin_function_t>(get_library_symbol(plugin.library, load_function_name));

        if (BEE_FAIL_F(plugin.load_function != nullptr, "Failed to get load function symbol `%s` for plugin at path: %s", load_function_name, plugin.library_path.c_str()))
        {
            unload_library(plugin.library);
            return;
        }

        plugin.unload_function = reinterpret_cast<load_plugin_function_t>(get_library_symbol(plugin.library, unload_function_name));

        if (BEE_FAIL_F(plugin.unload_function != nullptr, "Failed to get unload function symbol `%s` for plugin at path: %s", unload_function_name, plugin.library_path.c_str()))
        {
            unload_library(plugin.library);
            return;
        }
    }

    if (!refresh_debug_symbols())
    {
        unload_plugin_nolock(&plugin);
        return;
    }

    auto* loaded_plugin = g_plugins.plugins.find(event.plugin_name);
    const bool reload = loaded_plugin != nullptr;

    // If the plugin is already loaded then we can reload it
    if (loaded_plugin == nullptr)
    {
        loaded_plugin = g_plugins.plugins.insert(String(event.plugin_name), plugin);
    }

    PluginRegistry ctx(&g_plugins, event.plugin_name, reload);
    plugin.load_function(&ctx);
}

void process_unload_event(const PluginEvent& event)
{
    auto* plugin = g_plugins.plugins.find(event.plugin_name);

    if (BEE_FAIL_F(plugin != nullptr, "Plugin \"%s\" is not loaded or registered as a static plugin", event.plugin_name))
    {
        return;
    }

    unload_plugin_nolock(&plugin->value);

    if (plugin->value.library.handle != nullptr)
    {
        unload_library(plugin->value.library);
    }

    g_plugins.plugins.erase(event.plugin_name);
}

void process_add_interface_event(const PluginEvent& event)
{
    auto& api = g_plugins.find_or_add_api(event.api_name);

    // Add new interface entry for this pointer if not already added
    auto interface_index = g_plugins.find_or_add_interface(event.interface);
    auto& interface = g_plugins.interfaces[interface_index];
    interface.api = event.api_name;

    // Add interface to the API entry if it wasn't already added
    interface.index = api.find_or_add_interface(g_plugins.interfaces[interface_index].ptr);

    // notify observers of the new interface
    for (auto& observer : api.observers)
    {
        observer.callback(PluginEventType::add_interface, event.plugin_name, event.interface, observer.user_data);
    }
}

void process_remove_interface_event(const PluginEvent& event)
{
    const auto index = g_plugins.find_interface(event.interface);

    if (BEE_FAIL_F(index >= 0, "No such plugin interface"))
    {
        return;
    }

    auto& interface = g_plugins.interfaces[index];
    auto* api = g_plugins.apis.find(interface.api);

    BEE_ASSERT(api != nullptr);

    // Notify observers before removing the interface
    for (auto& observer : api->value.observers)
    {
        observer.callback(PluginEventType::remove_interface, event.plugin_name, event.interface, observer.user_data);
    }

    api->value.interfaces.erase(interface.index);
    g_plugins.interfaces.erase(index);
}

/*
 *****************************************
 *
 * Plugin API
 *
 *****************************************
 */
void init_plugin_registry()
{
    scoped_recursive_spinlock_t lock(g_plugins.mutex);
    BEE_ASSERT_F(!g_plugins.initialized, "Plugin Registry is already initialized");
    g_plugins.initialized = true;
}

void destroy_plugin_registry()
{
    scoped_recursive_spinlock_t lock(g_plugins.mutex);

    BEE_ASSERT_F(g_plugins.initialized, "Plugin Registry is not initialized");

    for (auto& plugin : g_plugins.plugins)
    {
        unload_plugin_nolock(&plugin.value);
    }

    destruct(&g_plugins);
    g_plugins.initialized = false;
}

void add_plugin_search_path(const Path& path)
{
    scoped_recursive_spinlock_t lock(g_plugins.mutex);
    g_plugins.add_search_path(path);
}

bool load_plugin(const char* name)
{
    if (is_plugin_loaded(name))
    {
        log_warning("Plugin \"%s\" is already loaded", name);
        return true;
    }

    add_plugin_event_threadsafe(PluginEventType::load_plugin, name, nullptr, nullptr);
    return true;
}

bool unload_plugin(const char* name)
{
    if (BEE_FAIL_F(is_plugin_loaded(name), "Plugin \"%s\" is not loaded or registered as a static plugin", name))
    {
        return false;
    }

    add_plugin_event_threadsafe(PluginEventType::unload_plugin, name, nullptr, nullptr);
    return true;
}

void reload_plugins(const Path& root_dir)
{
    for (auto entry : fs::read_dir(root_dir))
    {
        if (entry.extension() == plugin_extension)
        {

        }
    }
}

void refresh_plugins()
{
    scoped_recursive_spinlock_t lock(g_plugins.mutex);

    // Refresh the list of plugins before handling the pending events
    for (auto& path : g_plugins.search_paths)
    {
        reload_plugins(path);
    }

    while (!g_plugins.pending_events.empty())
    {
        auto event = g_plugins.pending_events[0];
        g_plugins.pending_events.erase(0);

        switch (event.type)
        {
            case PluginEventType::add_interface:
            {
                process_add_interface_event(event);
                break;
            }
            case PluginEventType::remove_interface:
            {
                process_remove_interface_event(event);
                break;
            }
            case PluginEventType::load_plugin:
            {
                process_load_event(event);
                break;
            }
            case PluginEventType::unload_plugin:
            {
                process_unload_event(event);
                break;
            }
            default: break;
        }

        if (event.type == PluginEventType::load_plugin || event.type == PluginEventType::unload_plugin)
        {
            if (!refresh_debug_symbols())
            {
                log_error("Failed to refresh debug symbols after unloading plugins");
            }
        }
    }

    g_plugins.pending_events.clear();
}

bool is_plugin_loaded(const char* name)
{
    scoped_recursive_spinlock_t lock(g_plugins.mutex);

    return g_plugins.plugins.find(name) != nullptr;
}


void add_plugin_observer(const char* api_name, plugin_observer_t observer, void* user_data)
{
    scoped_recursive_spinlock_t lock(g_plugins.mutex);

    BEE_ASSERT(g_plugins.initialized);

    auto& api = g_plugins.find_or_add_api(api_name);

    if (api.find_observer(observer) >= 0)
    {
        return;
    }

    api.observers.emplace_back(observer, user_data);
}

void remove_plugin_observer(const char* api_name, plugin_observer_t observer)
{
    scoped_recursive_spinlock_t lock(g_plugins.mutex);

    if (!g_plugins.initialized)
    {
        return;
    }

    auto* api = g_plugins.apis.find(api_name);

    BEE_ASSERT(api != nullptr);

    const auto index = api->value.find_observer(observer);

    if (BEE_FAIL_F(index >= 0, "No such observer registered to %s", api_name))
    {
        return;
    }

    api->value.observers.erase(index);
}


} // namespace bee
