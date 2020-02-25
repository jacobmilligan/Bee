/*
 *  WorkStealingQueueTests.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#define BEE_ENABLE_RELACY
#include <Bee/Core/Jobs/WorkStealingQueue.hpp>
#include <Bee/Core/Jobs/JobSystem.hpp>
#include <Bee/Core/Concurrency.hpp>
#include <Bee/Core/Main.hpp>
#include <Bee/Core/Random.hpp>

static constexpr auto max_workers = 8;
static constexpr auto max_jobs = 8192;

void test_job()
{
    bee::current_thread::sleep(100);
}


struct WSTest final : public rl::test_suite<WSTest, max_workers>
{
    bee::DynamicArray<bee::WorkStealingQueue> queues;
    bee::RandomGenerator<bee::Xorshift> random[max_workers];

    void before()
    {
        for (int i = 0; i < max_workers; ++i)
        {
            queues.emplace_back(max_jobs);
        }
    }

    void thread(const unsigned thread_index)
    {
        auto node = queues[thread_index].pop();

        if (node == nullptr)
        {
            // Steal from a random local_worker that isn't this one
            auto worker_index = thread_index;
            const auto random_range_max = max_workers - 1;
            while (worker_index == thread_index)
            {
                worker_index = random[thread_index].random_range(0, random_range_max);
            }

            BEE_ASSERT_F(worker_index >= 0 && worker_index < 8, "Scheduler: invalid local_worker index");
            node = queues[worker_index].steal();
        }

        if (node != nullptr)
        {
            static_cast<bee::Job*>(node->data[0])->complete();
            BEE_FREE(bee::system_allocator(), node);
        }

        auto ptr = static_cast<bee::u8*>(BEE_MALLOC_ALIGNED(bee::system_allocator(), sizeof(bee::AtomicNode) + sizeof(bee::Job), 64));
        auto job = reinterpret_cast<bee::Job*>(ptr + sizeof(bee::AtomicNode));
        node = reinterpret_cast<bee::AtomicNode*>(ptr);
        node->data[0] = job;

        auto fn = [=]() { test_job(); };
        new (job) bee::CallableJob<decltype(fn)>(fn);

        queues[thread_index].push(node);
    }

    void after()
    {
        bee::destruct(&queues);
    }
};


struct ParallelForData
{
    int x { 1 };
    int y { 1 };
    int z { 1 };
    int w { 1 };
};


struct ParallelForTest final : public rl::test_suite<ParallelForTest, max_workers>
{
    void thread(const unsigned thread_index)
    {
        bee::JobGroup group;
        ParallelForData array[16];
        bee::parallel_for(&group, 16, 1, [&](const bee::i32 index)
        {
            auto count = 0;
            for (int i = 0; i < 1000; ++i)
            {
                ++count;
            }

            array[index].x = count;
            array[index].y = count;
            array[index].z = count;
            array[index].w = count;
        });
        job_wait(&group);
    }
};

int bee_main(int argc, char** argv)
{
    bee::JobSystemInitInfo info{};
    info.num_workers = max_workers;
    bee::job_system_init(info);

    rl::test_params params;
    params.search_type = rl::scheduler_type_e::sched_random;
    params.iteration_count = max_jobs;
    rl::simulate<WSTest>(params);

    rl::simulate<ParallelForTest>(params);

    bee::job_system_shutdown();
    return EXIT_SUCCESS;
}