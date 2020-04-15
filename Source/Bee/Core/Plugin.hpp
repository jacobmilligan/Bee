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


namespace bee {


enum class PluginEventType
{
    none,
    add_interface,
    remove_interface,
    load_plugin,
    unload_plugin
};


struct PluginCache;

class BEE_CORE_API PluginRegistry
{
public:
    PluginRegistry(PluginCache* plugins, const char* plugin_name, const bool is_reloading);

    void add_interface(const char* api_name, void* interface);

    void remove_interface(void* interface);

    Span<void*> enumerate_api(const char* name);

    inline bool is_reloading() const
    {
        return reloading_;
    }

private:
    PluginCache*    plugins_ {nullptr };
    const char*     plugin_name_ { nullptr };
    bool            reloading_ { false };
};

using load_plugin_function_t = void(*)(PluginRegistry* context);

using plugin_observer_t = void(*)(const PluginEventType event, const char* plugin_name, void* interface, void* user_data);


struct BEE_CORE_API StaticPluginAutoRegistration
{
    StaticPluginAutoRegistration*   next { nullptr };
    load_plugin_function_t          load_plugin {nullptr };
    load_plugin_function_t          unload_plugin {nullptr };

    StaticPluginAutoRegistration(const char* name, load_plugin_function_t load_function, load_plugin_function_t unload_function);
};


#define BEE_PLUGIN_API extern "C" BEE_EXPORT_SYMBOL

#if BEE_CONFIG_MONOLITHIC_BUILD == 0
    #define BEE_REGISTER_PLUGIN(name)
#else
    #define BEE_REGISTER_PLUGIN(name) \
        StaticPluginAutoRegistration auto_plugin_registration(#name, bee_load_plugin, bee_unload_plugin)
#endif // BEE_CONFIG_MONOLITHIC_BUILD == 0

/*
 ************************************
 *
 * Plugin loading functions
 *
 ************************************
 */
BEE_CORE_API void init_plugin_registry();

BEE_CORE_API void destroy_plugin_registry();

BEE_CORE_API void add_plugin_search_path(const Path& path);

BEE_CORE_API bool load_plugin(const char* name);

BEE_CORE_API bool unload_plugin(const char* name);

BEE_CORE_API void refresh_plugins();

BEE_CORE_API bool is_plugin_loaded(const char* name);

BEE_CORE_API void add_plugin_observer(const char* api_name, plugin_observer_t observer, void* user_data = nullptr);

BEE_CORE_API void remove_plugin_observer(const char* api_name, plugin_observer_t observer);


} // namespace bee


