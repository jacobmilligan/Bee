/*
 *  JobSystem.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Memory/Memory.hpp"
#include "Bee/Core/Memory/PoolAllocator.hpp"
#include "Bee/Core/Memory/LinearAllocator.hpp"
#include "Bee/Core/Random.hpp"
#include "Bee/Core/Thread.hpp"
#include "Bee/Core/Time.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Jobs/WorkStealingQueue.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Core/Atomic.hpp"

#include <mutex>
#include <condition_variable>


namespace bee {


/*
 ****************************************************************
 *
 * # Worker
 *
 * Holds all the data needed to process jobs on a single thread.
 * Also contains a variable-sized pool allocator for allocating
 * jobs and a linear allocator for temporary job allocations.
 * Both of these allocators are non-locking and not thread-safe
 * when shared between threads/workers but are safe to use in
 * this context as the job system guarantees that allocations/
 * deallocations are made on the same thread and are therefore
 * ensured to be thread-safe by the job system - job deletion
 * is deferred within a `job_complete` call until their owning
 * worker can safely delete the jobs in a queue on its own
 * thread.
 *
 ****************************************************************
 */
BEE_PUSH_WARNING
BEE_DISABLE_WARNING_MSVC(4324)
struct alignas(128) Worker final : public Noncopyable
{
    Thread                      thread;
    i32                         thread_local_idx { -1 };
    WorkStealingQueue           job_queue;
    Job*                        current_executing_job { nullptr };
    RandomGenerator<Xorshift>   random;

    Worker() = default;

    Worker(const i32 thread_index, const JobSystemInitInfo& info) noexcept
        : thread_local_idx(thread_index),
          job_queue(info.max_jobs_per_worker_per_chunk)
    {}

    Worker(Worker&& other) noexcept
        : thread(std::move(other.thread)),
          thread_local_idx(other.thread_local_idx),
          job_queue(std::move(other.job_queue)),
          current_executing_job(other.current_executing_job),
          random(other.random)
    {
        other.thread_local_idx = 0;
        other.current_executing_job = nullptr;
    }

    // cache-line pad to avoid false sharing
    volatile char pad[128 - sizeof(Thread) - sizeof(i32) - sizeof(WorkStealingQueue) - sizeof(Job*) - sizeof(RandomGenerator<Xorshift>)]{};
};
BEE_POP_WARNING

struct WorkerMainParams
{
    Worker*                 worker { nullptr };
    std::atomic_int32_t*    ready_counter { nullptr };
};

struct JobSystemContext
{
    std::atomic_bool            initialized { false };
    thread_id_t                 main_thread_id { 0 };
    FixedArray<Worker>          workers;

    // Signal indicating that the system is currently running and active
    std::atomic_bool            is_active { false };
    std::atomic_int32_t         pending_job_count { 0 };
    std::mutex                  worker_wait_mutex;
    std::condition_variable     worker_wait_cv;

    // Job pools
    AtomicStack                 free_jobs;
    AtomicStack                 allocated_jobs;
};

static JobSystemContext g_job_system; // NOLINT

Job* allocate_job()
{
    auto node = g_job_system.free_jobs.pop();
    if (node == nullptr)
    {
        node = static_cast<AtomicNode*>(BEE_MALLOC_ALIGNED(system_allocator(), sizeof(AtomicNode) + sizeof(Job), 64));
        new (node) AtomicNode{};
        node->data[0] = reinterpret_cast<u8*>(node) + sizeof(AtomicNode);
        g_job_system.allocated_jobs.push(node);
    }
    return static_cast<Job*>(node->data[0]);
}


void worker_execute_one_job(Worker* local_worker)
{
    // check the thread local queue for a node
    auto worker_idx = local_worker->thread_local_idx;
    auto node = local_worker->job_queue.pop();

    // Try and steal a node from another local_worker if we couldn't pop one from the local local_worker
    if (node == nullptr)
    {
        const auto num_workers = g_job_system.workers.size();
        // if there's only one local_worker and one main thread steal that local_worker immediately
        if (num_workers == 1)
        {
            worker_idx = 0;
            node = g_job_system.workers[worker_idx].job_queue.steal();
        }
        else
        {
            // Steal from a random local_worker that isn't this one
            const auto random_range_max = num_workers - 1;
            while (worker_idx == local_worker->thread_local_idx)
            {
                worker_idx = local_worker->random.random_range(0, random_range_max);
            }

            BEE_ASSERT_F(worker_idx >= 0 && worker_idx < num_workers, "Scheduler: invalid local_worker index");
            node = g_job_system.workers[worker_idx].job_queue.steal();
        }
    }

    // Found a job
    if (node != nullptr)
    {
        auto job = static_cast<Job*>(node->data[0]);

        // Wait on any dependencies the group the node belongs to might have
        while (job->parent() != nullptr && job->parent()->has_dependencies())
        {
            worker_execute_one_job(local_worker);
        }

        local_worker->current_executing_job = job;

        // NOTE: This is a blocking call
        job->complete();

        local_worker->current_executing_job = nullptr;

        g_job_system.pending_job_count.fetch_sub(1, std::memory_order_release);

        destruct(job);

        g_job_system.free_jobs.push(node);

        /*
         * NOTE(Jacob): this may look a bit naughty but it's no worse than getting a header from a pointer in
         * an allocator and is the easiest way to get the atomic node from the node pointer
         */
//        const auto job_data_offset = sizeof(AtomicNode) - sizeof(Job);
//        auto node = reinterpret_cast<AtomicNode*>(reinterpret_cast<u8*>(node) - job_data_offset);
//        g_job_system.free_jobs.push(node);
    }
}

void worker_main(const WorkerMainParams& params)
{
    // Wait until all workers are ready and initialized
    params.ready_counter->fetch_sub(1, std::memory_order_release);
    while (!g_job_system.initialized.load()) {}

    // Run until job system has shutdown
    while (g_job_system.is_active.load(std::memory_order_acquire))
    {
        worker_execute_one_job(params.worker);

        // we don't want to sleep if we're only running jobs while waiting on a counter
        if (g_job_system.pending_job_count.load() <= 0)
        {
            std::unique_lock<std::mutex> wait_lock(g_job_system.worker_wait_mutex);

            g_job_system.worker_wait_cv.wait(wait_lock, [&]()
            {
                return g_job_system.pending_job_count.load(std::memory_order_acquire) > 0
                    || !g_job_system.is_active.load(std::memory_order_acquire);
            });
        }
    }
}

bool job_system_init(const JobSystemInitInfo& info)
{
    BEE_ASSERT(!g_job_system.initialized.load());

    g_job_system.is_active.store(true, std::memory_order_relaxed);
    g_job_system.main_thread_id = current_thread::id();

    // allocate and initialize workers
    auto num_workers = info.num_workers;
    if (num_workers == JobSystemInitInfo::auto_worker_count)
    {
        num_workers = concurrency::logical_core_count() - 1;
    }

    const auto worker_count_with_main_thread = num_workers + 1;

    g_job_system.workers.resize_no_raii(worker_count_with_main_thread);

    // indicates to the workers to wait to run their main loop until all threads are initialized
    std::atomic_int32_t ready_counter(num_workers);

    ThreadCreateInfo thread_info{};
    thread_info.use_temp_allocator = true;
    thread_info.priority = ThreadPriority::time_critical;

    WorkerMainParams worker_params{};
    worker_params.ready_counter = &ready_counter;

    for (int current_cpu_idx = 0; current_cpu_idx < worker_count_with_main_thread; ++current_cpu_idx)
    {
        // Setup a name for debugging
        str::format_buffer(thread_info.name, Thread::max_name_length, "sky::jobs(%d)", current_cpu_idx + 1);

        // Initialize the worker data
        worker_params.worker = &g_job_system.workers[current_cpu_idx];

        new (worker_params.worker) Worker(current_cpu_idx, info);

        g_job_system.workers[current_cpu_idx].thread_local_idx = current_cpu_idx;

        // Add a thread if not main thread
        if (current_cpu_idx < worker_count_with_main_thread - 1)
        {
            new (&g_job_system.workers[current_cpu_idx].thread) Thread(thread_info, &worker_main, worker_params);

            // g_job_system.workers[current_cpu_idx].thread.set_affinity(current_cpu_idx); NOTE(Jacob): disabled for PC
        }
        else
        {
            current_thread::set_name("sky::main");
            // current_thread::set_affinity(current_cpu_idx); NOTE(Jacob): disabled for PC
        }
    }

    srand(static_cast<unsigned int>(time::now())); // seed random generators

    while (ready_counter.load(std::memory_order_acquire) > 0) {}

    g_job_system.initialized.store(true);
    return true;
}

void job_system_shutdown()
{
    const auto pending_job_count = g_job_system.pending_job_count.load(std::memory_order_seq_cst);
    BEE_ASSERT_F(pending_job_count <= 0, "Tried to shut down the job system with %d jobs still pending", pending_job_count);

    g_job_system.is_active.store(false, std::memory_order_release);
    g_job_system.worker_wait_cv.notify_all();

    for (auto& worker : g_job_system.workers)
    {
        if (worker.thread.joinable())
        {
            worker.thread.join();
        }
    }

    // Cleanup the systems heap allocation and reset to default state
    g_job_system.initialized.store(false);
    
    new (&g_job_system) JobSystemContext{};
}

void job_schedule_group(JobGroup* group, Job** dependencies, const i32 dependency_count)
{
    BEE_ASSERT_F(g_job_system.initialized.load(), "Attempted to run jobs without initializing the job system");

    const auto local_worker_idx = get_local_job_worker_id();
    auto& local_worker = g_job_system.workers[local_worker_idx];

    for (int d = 0; d < dependency_count; ++d)
    {
        group->add_job(dependencies[d]);
        g_job_system.pending_job_count.fetch_add(1, std::memory_order_release);
        local_worker.job_queue.push(cast_job_to_node(dependencies[d]));
    }

    g_job_system.worker_wait_cv.notify_all();
}

void job_schedule(JobGroup* group, Job* job)
{
    job_schedule_group(group, &job, 1);
}

bool job_wait(JobGroup* group)
{
    BEE_ASSERT_F(g_job_system.initialized.load(), "Attempted to wait on a job without initializing the job system");

    // Wait on the job to finish before we actually go and complete it
    const auto local_worker_idx = get_local_job_worker_id();
    if (BEE_FAIL_F(local_worker_idx >= 0, "Couldn't find a worker for the current thread. Ensure you're not calling job system functions from an non-worker, external thread"))
    {
        return false;
    }
    auto local_worker = &g_job_system.workers[local_worker_idx];

    // Try and help execute jobs while we're waiting for this job to complete
    while (group->has_pending_jobs())
    {
        if (!g_job_system.is_active.load(std::memory_order_acquire))
        {
            break;
        }
        worker_execute_one_job(local_worker);
    }

    return true;
}

Job* get_local_executing_job()
{
    const auto local_worker = get_local_job_worker_id();
    return g_job_system.workers[local_worker].current_executing_job;
}

i32 get_local_job_worker_id()
{
    static thread_local i32 thread_local_idx = -1;

    // check if the thread local worker has already been found previously
    if (thread_local_idx >= 0)
    {
        return thread_local_idx;
    }

    // Main thread is always the last thread in the workers array
    if (current_thread::id() == g_job_system.main_thread_id)
    {
        thread_local_idx = g_job_system.workers.back().thread_local_idx;
        return thread_local_idx;
    }

    // first time looking for worker, so search for it
    // Note: the last thread in the workers array won't have been initialized and will therefore
    // have the launching threads ID (usually the main thread)
    for (const auto& worker : g_job_system.workers)
    {
        if (worker.thread.id() == current_thread::id())
        {
            thread_local_idx = worker.thread_local_idx;
            return thread_local_idx;
        }
    }

    // couldn't find a worker so this some other non-worker thread
    BEE_UNREACHABLE("Couldn't find a worker for the current thread: there may be an error setting thread affinities at startup");
}

i32 get_job_worker_count()
{
    return g_job_system.workers.size(); // last 'worker' is the main thread
}


} // namespace bee
