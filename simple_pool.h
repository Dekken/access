

#include <atomic>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

#include <future>
#include <chrono>

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE > 200112L
#include <pthread.h>
#endif

#include <utmpx.h>
#include "kul/log.hpp"

namespace tick { 

class SimplePool{

  private:
    bool stop = 0;
    uint16_t m_n_threads;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<uint16_t> m_done;
    std::vector<std::function<void()> > m_tasks;
    std::vector<std::thread> m_threads;
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE > 200112L
    uint16_t m_smt = 0, m_cpus = 0;
    std::unordered_map<uint16_t, uint16_t> threadIX2NAlloc;
#endif
  public:
    SimplePool(const SimplePool&) = delete;
    SimplePool(const SimplePool&&) = delete;
    SimplePool& operator=(const SimplePool&) = delete;
    SimplePool& operator=(const SimplePool&&) = delete;

    SimplePool(uint16_t n_threads, uint16_t smt = 0)
       : m_n_threads(n_threads), m_done(0)
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE > 200112L
       , m_smt(smt), m_cpus(std::thread::hardware_concurrency())
#endif       
       {
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE > 200112L
      // Create a cpu_set_t object representing a set of CPUs. Clear it and mark
      cpu_set_t cpuset;
      for(uint16_t i = 0; i < m_n_threads; ++i){
        threadIX2NAlloc[i] = 0;
      }
#endif

      for(uint16_t i = 0; i < m_n_threads; ++i){
        m_threads.emplace_back(
          [this](uint16_t ix)
          {
            for(;;)
            {
              std::function<void()> task;
              {
                std::unique_lock<std::mutex> lock(this->m_mutex);
                this->m_condition.wait(lock,
                    [this, ix]{ return this->m_tasks.size() > ix && m_done > ix; });
                if(this->stop) return;
                task = std::move(this->m_tasks[0]);
                this->m_tasks.erase(this->m_tasks.begin());
              }
              task();
              this->m_done--;
            }
          }, i
        );
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE > 200112L
        
        if(smt){
          // only CPU i as set.
          CPU_ZERO(&cpuset);
          auto ix = m_cpus - (i + 1);
          // KLOG(INF) << "affinity: " << ix;
          
          CPU_SET(ix, &cpuset);
          int rc = pthread_setaffinity_np(m_threads[i].native_handle(),
                                          sizeof(cpu_set_t), &cpuset);
          if (rc != 0) {
            std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
          }
        }        
#endif
      }
    }

    ~SimplePool(){
      {
        std::unique_lock<std::mutex> lock(m_mutex);
        stop = 1;
        m_tasks.resize(m_n_threads);
        m_done = m_n_threads;
      }      
      m_condition.notify_all();
      for(auto &th : m_threads) th.join();
    }

    SimplePool& sync() {
      while(m_done > 0) {
        std::this_thread::sleep_for( std::chrono::nanoseconds(100) );
      }
      m_tasks.clear();
      return *this;
    }

    template <class T>
    SimplePool& async(std::vector<std::function<T> >& funcs){
      for(auto& func : funcs){
        m_tasks.emplace_back([&](){
          func();
        });
      }
      m_done = funcs.size();
      m_condition.notify_all();
      return *this;
    }
};

}  // namespace tick


#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__) || defined(__INTEL_COMPILER)
#define TICK_PREFETCH(ptr, rw, val) __builtin_prefetch(ptr, rw, val);
#else
TICK_PREFETCH(ptr, rw, val)
#endif
