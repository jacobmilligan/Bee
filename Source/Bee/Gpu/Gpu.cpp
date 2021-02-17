/*
 *  Gpu.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Gpu/Gpu.hpp"


namespace bee {


struct GpuSetup
{
    DynamicArray<GpuBackend*> backends;
};

static GpuSetup* g_setup = nullptr;

void register_backend(GpuBackend* backend)
{
    if (find_index(g_setup->backends, backend) >= 0)
    {
        log_error("GPU backend \"%s\" is already registered", backend->get_name());
        return;
    }

    g_setup->backends.push_back(backend);
}

void unregister_backend(const GpuBackend* backend)
{
    const int index = find_index_if(g_setup->backends, [&](GpuBackend* b) { return b == backend; });
    if (index < 0)
    {
        log_error("GPU backend \"%s\" is not registered", backend->get_name());
        return;
    }

    g_setup->backends.erase(index);
}

i32 enumerate_available_backends(GpuBackend** dst)
{
    if (dst != nullptr)
    {
        memcpy(dst, g_setup->backends.data(), sizeof(GpuBackend*) * g_setup->backends.size());
    }
    return g_setup->backends.size();
}

GpuBackend* get_default_backend(const GpuApi api)
{
    const int index = find_index_if(g_setup->backends, [&](GpuBackend* backend)
    {
        return backend->get_api() == api;
    });

    return index >= 0 ? g_setup->backends[index] : nullptr;
}

GpuBackend* get_backend(const char* name)
{
    const int index = find_index_if(g_setup->backends, [&](GpuBackend* backend)
    {
        return str::compare(backend->get_name(), name) == 0;
    });

    return index >= 0 ? g_setup->backends[index] : nullptr;
}


} // namespace bee

static bee::GpuModule g_module{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    bee::g_setup = loader->get_static<bee::GpuSetup>("Bee.GpuSetup");

    g_module.register_backend = bee::register_backend;
    g_module.unregister_backend = bee::unregister_backend;
    g_module.enumerate_available_backends = bee::enumerate_available_backends;
    g_module.get_default_backend = bee::get_default_backend;
    g_module.get_backend = bee::get_backend;

    loader->set_module(BEE_GPU_MODULE_NAME, &g_module, state);
}
