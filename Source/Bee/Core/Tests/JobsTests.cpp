//
//  JobsTests.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 23/06/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Math/float4x4.hpp"
#include "Bee/Core/Time.hpp"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/Thread.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

#include <gtest/gtest.h>

class JobsTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        bee::JobSystemInitInfo info{};
        info.max_jobs_per_worker_per_chunk = 1024;
        info.num_workers = bee::JobSystemInitInfo::auto_worker_count;
        bee::job_system_init(info);
    }

    void TearDown() override
    {
        bee::job_system_shutdown();
    }
};

bool ready = false;
std::atomic_int32_t done(0);
int result[1000];

struct CountJob : public bee::Job
{
    bee::i32 job_id { -1 };

    CountJob(const bee::i32 job_id)
        : job_id(job_id)
    {}

    void execute() override
    {
        auto count = 0;
        for (int i = 0; i < 100000; ++i)
        {
            ++count;
        }
        result[job_id] = count;
        ++done;
    }
};

std::atomic_int32_t next_job_id(0);

void count_job_function(int* result_array)
{
    const auto job_id = next_job_id++;
    auto count = 0;
    for (int i = 0; i < 100000; ++i)
    {
        ++count;
    }
    result_array[job_id] = count;
    ++done;
}


TEST_F(JobsTests, test_count)
{
    bee::Job* jobs[1000];

    memset(result, 0, sizeof(int) * 1000);
    done = 0;
    const auto sync_begin = bee::time::now();
    for (int i = 0; i < 1000; ++i)
    {
        auto count = 0;
        for (int j = 0; j < 100000; ++j)
        {
            ++count;
        }
        result[i] = count;
        ++done;
    }
    const auto sync_time = bee::TimePoint(bee::time::now() - sync_begin).total_milliseconds();

    printf("Single-threaded time: %f. Done: %d\n", sync_time, done.load());

    // Reset array
    memset(result, 0, sizeof(int) * bee::static_array_length(result));
    for (int j = 0; j < 1000; ++j)
    {
        jobs[j] = bee::allocate_job(&count_job_function, result);
        ASSERT_NE(jobs[j], nullptr);
    }

    // Test we aren't allocating jobs from the pool that point to the same memory
    for (int j = 0; j < 1000; ++j)
    {
        for (int inner = 0; inner < 1000; ++inner)
        {
            if (j == inner)
            {
                continue;
            }
            ASSERT_NE(jobs[j], jobs[inner]);
        }
    }

    done = 0;
    auto jobs_begin = bee::time::now();
    bee::JobGroup group{};
    bee::job_schedule_group(&group, jobs, bee::static_array_length(jobs));
    bee::job_wait(&group);
    auto jobs_time = bee::TimePoint(bee::time::now() - jobs_begin).total_milliseconds();

    printf("Job function time: %f. Done: %d\n", jobs_time, done.load());

    ASSERT_EQ(done, 1000);
    for (int i = 0; i < bee::static_array_length(result); ++i)
    {
        ASSERT_EQ(result[i], 100000) << "Job: " << i;
    }

    ASSERT_EQ(bee::get_local_job_allocator_size(), 0);

    done = 0;

    // Reset array
    memset(result, 0, sizeof(int) * bee::static_array_length(result));

    for (int j = 0; j < 1000; ++j)
    {
        jobs[j] = bee::allocate_job<CountJob>(j);
        ASSERT_NE(jobs[j], nullptr);
        reinterpret_cast<CountJob*>(jobs[j])->job_id = j;
    }

    jobs_begin = bee::time::now();
    bee::job_schedule_group(&group, jobs, bee::static_array_length(jobs));
    bee::job_wait(&group);
    jobs_time = bee::TimePoint(bee::time::now() - jobs_begin).total_milliseconds();

    printf("Job struct time: %f. Done: %d\n", jobs_time, done.load());

    ASSERT_EQ(done, 1000);
    for (int i = 0; i < bee::static_array_length(result); ++i)
    {
        ASSERT_EQ(result[i], 100000) << "Job: " << i;
    }
}

TEST_F(JobsTests, parallel_for)
{
    struct ParallelForData
    {
        int x { 1 };
        int y { 1 };
        int z { 1 };
        int w { 1 };
    };

    ParallelForData data[1000];

    const auto jobs_begin = bee::time::now();
    bee::JobGroup group{};
    bee::parallel_for(&group, 1000, 1, [&](const bee::i32 index)
    {
        auto count = 0;
        for (int i = 0; i < 100000; ++i)
        {
            ++count;
        }

        data[index].x = count;
        data[index].y = count;
        data[index].z = count;
        data[index].w = count;
    });

    bee::job_wait(&group);
    const auto jobs_time = bee::TimePoint(bee::time::now() - jobs_begin).total_milliseconds();
    printf("Parallel for time: %f\n", jobs_time);

    for (const auto& i : data)
    {
        ASSERT_EQ(i.x, 100000);
        ASSERT_EQ(i.y, 100000);
        ASSERT_EQ(i.z, 100000);
        ASSERT_EQ(i.w, 100000);
    }
}
