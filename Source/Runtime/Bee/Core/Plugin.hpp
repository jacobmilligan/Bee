/*
 *  Plugin.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"


namespace bee {
namespace plugins {


struct PluginAPI
{
    void* ptr { nullptr };

    template <typename APIType>
    APIType* as()
    {
        BEE_ASSERT(ptr != nullptr);
        return static_cast<APIType*>(ptr);
    }

    inline bool is_valid()
    {
        return ptr != nullptr;
    }
};


using load_function_t = void*();

using unload_function_t = void(void*);


BEE_CORE_API void init_registry();

BEE_CORE_API void destroy_registry();

BEE_CORE_API bool is_registry_initialized();

BEE_CORE_API PluginAPI load_dynamic_plugin(const char* name);

BEE_CORE_API PluginAPI load_static_plugin(const char* name, load_function_t* load, unload_function_t* unload);

BEE_CORE_API void unload_plugin(const char* name);

BEE_CORE_API PluginAPI get_plugin_api(const char* name);

BEE_CORE_API bool is_plugin_loaded(const char* name);

template <typename APIType>
APIType* get_plugin(const char* name)
{
    return get_plugin_api(name).as<APIType>();
}


} // namespace plugins
} // namespace bee



#if BEE_CONFIG_MONOLITHIC_BUILD == 0
    #define BEE_LOAD_PLUGIN(name) bee::plugins::load_dynamic_plugin(#name).as<name##_plugin_t>()

    #define BEE_DECLARE_PLUGIN(name, api_type)                                      \
        static const char* BEE_PLUGIN_NAME_##name = #name;                          \
        typedef api_type name##_plugin_t;                                           \
        extern "C" BEE_EXPORT_SYMBOL void* bee_load_plugin_##name();                \
        extern "C" BEE_EXPORT_SYMBOL void bee_unload_plugin_##name(void* plugin)
#else
    #define BEE_LOAD_PLUGIN(name) bee::plugins::load_static_plugin(#name, &bee_load_plugin_##name, &bee_unload_plugin_##name).as<name##_plugin_t>()

    #define BEE_DECLARE_PLUGIN(name, api_type)                  \
        static const char* BEE_PLUGIN_NAME_##name = #name;      \
        typedef api_type name##_plugin_t;                       \
        extern "C" void* bee_load_plugin_##name();              \
        extern "C" void bee_unload_plugin_##name(void* library)
#endif // BEE_DLL

#define BEE_PLUGIN_ENTRY_LOAD(name) void* bee_load_plugin_##name()

#define BEE_PLUGIN_ENTRY_UNLOAD(name)                                       \
void bee_unload_plugin_##name(name##_plugin_t* plugin);                     \
void bee_unload_plugin_##name(void* plugin)                                 \
{                                                                           \
    bee_unload_plugin_##name(static_cast<name##_plugin_t*>(plugin));        \
}                                                                           \
void bee_unload_plugin_##name(name##_plugin_t* plugin)

