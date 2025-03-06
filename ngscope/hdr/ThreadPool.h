#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <condition_variable>

#include "srsran/srsran.h"

#include "main.h"
#include "common_type_define.h"
#include "sfworker.h"
#include "sfTask.h"
#include "runtimeConfig.h"


#ifdef __cplusplus
extern "C" {
#endif

class ThreadPool
{
public:
ThreadPool() { }
ThreadPool(const size_t nof_sfworkers, std::shared_ptr<ngscope_config_t> ngscope_config);
~ThreadPool() { 
    for (auto& worker : sfWorkers) {
        worker.reset();
    }
    // stop = true;
    // cv.notify_all();
}

void enqueue(std::unique_ptr<sfTask> task, SFWorker* curWorker);

SFWorker* findAvailableWorker();

SFWorker* findWorkerByIdx(uint32_t idx);

cf_t** getWorkerBuffer(int workerIdx);

void stopAllWorkers();

private:
std::vector<std::unique_ptr<SFWorker>> sfWorkers;
std::mutex workerMutex;
// std::vector<std::thread> sfThreads;
// std::queue<std::unique_ptr<sfTask>> sfTaskQueue;
// std::mutex queueMutex;
// std::condition_variable cv;
// std::atomic<bool> stop;
std::shared_ptr<ngscope_config_t> ngscope_config;
};


#ifdef __cplusplus
}
#endif

#endif