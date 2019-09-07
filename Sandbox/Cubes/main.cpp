/*
 *  main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Application/Main.hpp>
#include <Bee/Graphics/GPU.hpp>


class CubesApp final : public bee::Application
{
public:
    int launch(bee::AppContext* ctx) override
    {
        bee::PhysicalDeviceInfo pd_info[BEE_GPU_MAX_PHYSICAL_DEVICES];
        bee::gpu_enumerate_physical_devices(pd_info, BEE_GPU_MAX_DEVICES);

        bee::DeviceCreateInfo device_info{};
        device_info.physical_device_id = 0;
        device_ = bee::gpu_create_device(device_info);
        return 0;
    }

    void shutdown(bee::AppContext* ctx) override
    {
        bee::gpu_destroy_device(device_);
    }

    void tick(bee::AppContext* ctx) override
    {
        if (bee::key_typed(ctx->default_input, bee::Key::escape))
        {
            ctx->quit = true;
        }
    }

private:
    bee::DeviceHandle device_;
};

int bee_main(int argc, char** argv)
{
    CubesApp app;
    bee::AppLaunchConfig config{};
    return bee::app_loop(config, &app);
}