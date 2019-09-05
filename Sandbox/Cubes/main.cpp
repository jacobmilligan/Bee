/*
 *  main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Application/Main.hpp>


class CubesApp final : public bee::Application
{
public:
    int launch(bee::AppContext* ctx) override
    {
        return 0;
    }

    void shutdown(bee::AppContext* ctx) override
    {

    }

    void tick(bee::AppContext* ctx) override
    {
        if (bee::is_key_down(ctx->default_input, bee::Key::escape))
        {
            ctx->quit = true;
        }
    }

private:
};

int bee_main(int argc, char** argv)
{
    CubesApp app;
    bee::AppLaunchConfig config{};
    return bee::app_loop(config, &app);
}