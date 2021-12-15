//--------------------------------------------------------------------------------------------------
// Copyright (c) 2021 Marcus Geelnard
//
// This software is provided 'as-is', without any express or implied warranty. In no event will the
// authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose, including commercial
// applications, and to alter it and redistribute it freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not claim that you wrote
//     the original software. If you use this software in a product, an acknowledgment in the
//     product documentation would be appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and must not be misrepresented as
//     being the original software.
//
//  3. This notice may not be removed or altered from any source distribution.
//--------------------------------------------------------------------------------------------------

#include <base/io_worker.hpp>

#include <base/debug_utils.hpp>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace bcache {
namespace io_worker {
namespace {
std::vector<std::thread> s_thread_pool;
std::condition_variable s_pool_cond;
std::mutex s_pool_mutex;

bool s_terminate = false;

std::queue<FILE*> s_fclose_queue;

void worker() {
  bool running = true;
  while (running) {
    FILE* f = nullptr;

    {
      // Wait for an item to arrive in the queue.
      std::unique_lock<std::mutex> lock(s_pool_mutex);
      while (s_fclose_queue.empty() && !s_terminate) {
        s_pool_cond.wait(lock);
      }

      // Pop the next work item.
      if (!s_fclose_queue.empty()) {
        f = s_fclose_queue.front();
        s_fclose_queue.pop();
      }

      // Stop work?
      if (s_terminate && s_fclose_queue.empty()) {
        running = false;
      }
    }

    // Perform the work (without holding the queue lock).
    if (f != nullptr) {
      std::fclose(f);
    }
  }
}

bool is_worker_active() {
  return !s_thread_pool.empty();
}

}  // namespace

void start(int num_threads) {
  for (int i = 0; i < num_threads; ++i) {
    s_thread_pool.emplace_back(std::thread(worker));
  }
}

void stop() {
  if (is_worker_active())
  {
    {
      std::unique_lock<std::mutex> lock(s_pool_mutex);
      s_terminate = true;
      s_pool_cond.notify_all();
    }
    for (auto& thread : s_thread_pool) {
      thread.join();
    }
    s_thread_pool.clear();
  }
}

void enqueue_fclose(FILE* f) {
  if (is_worker_active()) {
    std::unique_lock<std::mutex> lock(s_pool_mutex);
    s_fclose_queue.push(f);
    s_pool_cond.notify_one();
  } else {
    std::fclose(f);
  }
}

}  // namespace io_worker
}  // namespace bcache
