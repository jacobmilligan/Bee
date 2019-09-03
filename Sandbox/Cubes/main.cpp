/*
 *  main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Application/Main.hpp>


struct Ctx : public bee::AppContext
{

};

int init(Ctx* ctx)
{
    return 0;
}

void tick(Ctx* ctx)
{

}

int bee_main(int argc, char** argv)
{
    bee::AppCreateInfo<Ctx> info{};
    info.init = &init;
    info.tick = &tick;

    return bee::create_and_run_app(info);
}