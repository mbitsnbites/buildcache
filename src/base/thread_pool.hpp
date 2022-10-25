//--------------------------------------------------------------------------------------------------
// Copyright (c) 2022 Marcus Geelnard
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

#ifndef BUILDCACHE_THREAD_POOL_HPP_
#define BUILDCACHE_THREAD_POOL_HPP_

#include <condition_variable>
#include <algorithm>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace bcache {
/// @brief A thread pool.
class thread_pool_t {
public:
  /// @brief Threading mode of the thread pool.
  enum mode_t {
    SINGLE_THREADED,  ///< Only use a single thread.
    ALL_HW_THREADS    ///< Use all available hardware threads (default).
  };

  /// @brief Create a new thread pool.
  /// @param mode The threading mode to use for this pool.
  explicit thread_pool_t(mode_t mode = ALL_HW_THREADS) : m_terminate(false) {
    const auto num_threads =
        (mode == SINGLE_THREADED) ? 1U : std::max(4U, std::thread::hardware_concurrency());
    for (unsigned i = 0; i < num_threads; ++i) {
      m_threads.emplace_back(std::thread(&thread_pool_t::worker, this));
    }
  }

  /// @brief Terminate all threads gracefully and tear down the thread pool.
  /// @note The last exception that was thrown in any of the enqueued functions will be rethrown in
  /// the calling thread (if no exception was thrown, no exception will be thrown).
  ~thread_pool_t() {
    {
      std::unique_lock<std::mutex> queue_lock(m_queue_mutex);
      m_terminate = true;
      m_queue_cond.notify_all();
    }
    for (auto& t : m_threads) {
      t.join();
    }
    m_threads.clear();
    throw_pending_exception();
  }

  /// @brief Enqueue a function to be executed in the thread pool
  /// @param fun The function to enqueue.
  void enqueue(std::function<void()>&& fun) {
    std::unique_lock<std::mutex> pending_funcs_lock(m_pending_funcs_mutex);
    ++m_pending_funcs;
    std::unique_lock<std::mutex> queue_lock(m_queue_mutex);
    m_queue.emplace(std::move(fun));
    m_queue_cond.notify_one();
  }

  /// @brief Wait for all enqueued functions to finish.
  /// @note The last exception that was thrown in any of the enqueued functions will be rethrown in
  /// the calling thread (if no exception was thrown, no exception will be thrown).
  void wait() {
    // Wait until all pending function calls have finished.
    std::unique_lock<std::mutex> lock(m_pending_funcs_mutex);
    while (m_pending_funcs > 0) {
      m_pending_funcs_cond.wait(lock);
    }
    throw_pending_exception();
  }

private:
  /// @brief Worker function for every thread in the thread pool.
  void worker() noexcept {
    // Loop until the pool is terminated.
    while (true) {
      // Wait for a function call to be enqueued or the pool to be terminated.
      std::unique_lock<std::mutex> queue_lock(m_queue_mutex);
      while (m_queue.empty() && !m_terminate) {
        m_queue_cond.wait(queue_lock);
      }

      // Were we requested to terminate?
      if (m_terminate) {
        break;
      }

      // Pop the function call from the queue.
      auto fun = std::move(m_queue.front());
      m_queue.pop();
      queue_lock.unlock();

      // Call the function and handle any exceptions.
      try {
        fun();
      } catch (...) {
        std::unique_lock<std::mutex> exception_lock(m_exception_mutex);
        m_exception = std::current_exception();
        m_got_exception = true;
      }

      // Signal that the function has finished executing.
      std::unique_lock<std::mutex> pending_funcs_lock(m_pending_funcs_mutex);
      --m_pending_funcs;
      m_pending_funcs_cond.notify_one();
    }
  }

  void throw_pending_exception() {
    if (m_got_exception) {
      m_got_exception = false;
      std::rethrow_exception(m_exception);
    }
  }

  bool m_terminate = false;

  std::vector<std::thread> m_threads;

  std::queue<std::function<void()>> m_queue;
  std::mutex m_queue_mutex;
  std::condition_variable m_queue_cond;

  int m_pending_funcs = 0;
  std::mutex m_pending_funcs_mutex;
  std::condition_variable m_pending_funcs_cond;

  std::exception_ptr m_exception;
  bool m_got_exception = false;
  std::mutex m_exception_mutex;
};
}  // namespace bcache

#endif  // BUILDCACHE_THREAD_POOL_HPP_
