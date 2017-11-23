#include "random.hpp"
#include "kul/log.hpp"
#include "kul/time.hpp"
#include "kul/string.hpp"
#include "kul/threads.hpp"

#include "simple_pool.h"

using Random = effolkronium::random_static;

#ifndef SIZE
#define SIZE 128
#endif 

#ifndef ITER
#define ITER 100
#endif 


volatile bool NOOP = 0;
std::atomic<size_t> check(0);

void seq(const size_t i, bool d[][SIZE]){
  for(size_t j = 0; j < SIZE; j++) if(d[i][j]) NOOP = d[i][j];
#if defined(WITH_CHECK)
  check++;
#endif
}

int main(int argc, char* argv[]){
  size_t SIZE2 = SIZE * SIZE;
  bool b [SIZE2][SIZE]={0};
  size_t total = 0, test = 0;

  auto checkVal = [&](size_t line, bool b [][SIZE]){
    for(size_t i = 0; i < SIZE2; i++)
      for(size_t j = 0; j < SIZE; j++) 
        if(b[i][j]){
          KOUT(ERR) << "Fail @ Line: " << line;
          exit(1);
        };
  };

  auto set = [&](bool b [][SIZE]){
    for(size_t i = 0; i < SIZE2; i++)
      for(size_t j = 0; j < SIZE; j++) b[i][j] = Random::get(0, 1);
  };

  set(b);

  auto now = kul::Now::NANOS();
  auto ser = now;
  {
    now = kul::Now::NANOS();
    for(uint t = 0; t < ITER; t++) for(uint i = 0; i < SIZE2; i++) seq(i, b);
    ser = (kul::Now::NANOS() - now) / ITER;
  }

  auto threaded = [&](size_t n_threads){
#if defined(WITH_CHECK)
    check = 0;
#endif
    size_t it = std::floor((SIZE2)/n_threads);
    size_t high = (it * n_threads);
    now = kul::Now::NANOS();
    for(uint t = 0; t < ITER; t++){
      std::vector<std::thread> threads;
      for(size_t i = 0; i < n_threads; i++)
        threads.emplace_back([&](const size_t s, const size_t m){
            for(uint i = s; i < m; i++) seq(i, b);
        }, i * it, (i * it) + it);
      for (size_t i = 0; i < n_threads; i++)
        threads[i].join();
      for(uint i = high; i < SIZE2; i++) seq(i, b);
    }
    auto res = (kul::Now::NANOS() - now) / ITER;
#if defined(WITH_CHECK)
    KLOG(INF) << "check : " << check << " : threads = " << n_threads;
#endif
    return res;
  };

  auto job = [&](const size_t s, const size_t m){
    for(uint i = s; i < m; i++) seq(i, b);
  };

  auto pooled = [&](size_t n_threads){
#if defined(WITH_CHECK)
    check = 0;
#endif
    size_t it = std::floor((SIZE2)/n_threads);
    tick::SimplePool simP(n_threads);
    now = kul::Now::NANOS();
    for(uint t = 0; t < ITER; t++){
      std::vector<std::function<void()>> funcs;
      for(size_t i = 0; i < n_threads; i++)
        funcs.emplace_back(std::bind(job, i * it, (i * it) + it));
      simP.async(funcs).sync();
    }
    auto res = (kul::Now::NANOS() - now) / ITER;
#if defined(WITH_CHECK)
    KLOG(INF) << "check : " << check << " : threads = " << n_threads;
#endif
    return res;
  };

  auto prefetch = [&](size_t s){
    TICK_PREFETCH(&b[s], 0, 3) 
  };

  auto batch = [&](size_t n_threads, size_t batch = SIZE, size_t smt = 0, size_t pre = 0){
#if defined(WITH_CHECK)
    check = 0;
#endif
    size_t bit  = std::ceil((SIZE2)/(batch)/n_threads);
    tick::SimplePool simP(n_threads, smt);
    if(pre){
        std::vector<std::function<void()>> funcs;
        for(size_t i = 0; i < n_threads; i++)
          funcs.emplace_back(std::bind(prefetch, (i * batch) + pre));
        simP.async(funcs).sync();
    }
    now = kul::Now::NANOS();
    for(uint t = 0; t < ITER; t++){
      for(size_t bx = 0; bx < bit; bx++){
        std::vector<std::function<void()>> funcs;
        for(size_t i = 0; i < n_threads; i++)
          funcs.emplace_back(std::bind(job, (i * batch * bx), (i * batch * bx) + batch));
        simP.async(funcs).sync();
      }      
    }
    auto res = (kul::Now::NANOS() - now) / ITER;
#if defined(WITH_CHECK)
    KLOG(INF) << "check : " << check << " : threads = " << n_threads;
#endif
    return res;
  };

  auto t1  = threaded(1);  auto p1  = pooled(1);  auto b1  = batch(1,  1024); auto bs1  = batch(1,  1024, 2); 
  auto t2  = threaded(2);  auto p2  = pooled(2);  auto b2  = batch(2,  1024); auto bs2  = batch(2,  1024, 2);
  auto t4  = threaded(4);  auto p4  = pooled(4);  auto b4  = batch(4,  1024); auto bs4  = batch(4,  1024, 2);
  auto t6  = threaded(6);  auto p6  = pooled(6);  auto b6  = batch(6,  1024); //auto bs6  = batch(6,  1024, 2);
  auto t8  = threaded(8);  auto p8  = pooled(8);  auto b8  = batch(8,  1024); //auto bs8  = batch(8,  1024, 2);
  auto t10 = threaded(10); auto p10 = pooled(10); auto b10 = batch(10, 1024); //auto bs10 = batch(10, 1024, 2);
  auto t12 = threaded(12); auto p12 = pooled(12); auto b12 = batch(12, 1024); //auto bs12 = batch(12, 1024, 2);
  auto t14 = threaded(14); auto p14 = pooled(14); auto b14 = batch(14, 1024); //auto bs14 = batch(14, 1024, 2);
  auto t16 = threaded(16); auto p16 = pooled(16); auto b16 = batch(16, 1024); auto bs16 = batch(16, 1024, 2); auto bsp16 = batch(16, 1024, 0, 3);

  KLOG(INF) << "time for    serial seq  : " << ser;

  KLOG(INF) << "time for 1  thread seq  : " << t1;
  KLOG(INF) << "time for 2  thread seq  : " << t2;
  KLOG(INF) << "time for 4  thread seq  : " << t4;
  KLOG(INF) << "time for 6  thread seq  : " << t6;
  KLOG(INF) << "time for 8  thread seq  : " << t8;
  KLOG(INF) << "time for 10 thread seq  : " << t10;
  KLOG(INF) << "time for 12 thread seq  : " << t12;
  KLOG(INF) << "time for 14 thread seq  : " << t14;
  KLOG(INF) << "time for 16 thread seq  : " << t16;

  KLOG(INF) << "time for 1  thpool seq  : " << p1;
  KLOG(INF) << "time for 2  thpool seq  : " << p2;
  KLOG(INF) << "time for 8  thpool seq  : " << p8;
  KLOG(INF) << "time for 12 thpool seq  : " << p16;
  KLOG(INF) << "time for 16 thpool seq  : " << p16;
  
  KLOG(INF) << "time for 1  pbatch seq  : " << b1;
  KLOG(INF) << "time for 2  pbatch seq  : " << b2;
  KLOG(INF) << "time for 4  pbatch seq  : " << b4;
  KLOG(INF) << "time for 6  pbatch seq  : " << b6;
  KLOG(INF) << "time for 8  pbatch seq  : " << b8;
  KLOG(INF) << "time for 10 pbatch seq  : " << b10;
  KLOG(INF) << "time for 12 pbatch seq  : " << b12;
  KLOG(INF) << "time for 16 pbatch seq  : " << b16;

  KLOG(INF) << "time for 1  sbatch seq  : " << bs1;
  KLOG(INF) << "time for 2  sbatch seq  : " << bs2;
  KLOG(INF) << "time for 4  sbatch seq  : " << bs4;
  // KLOG(INF) << "time for 6  sbatch seq  : " << bs6;
  // KLOG(INF) << "time for 8  sbatch seq  : " << bs8;
  // KLOG(INF) << "time for 10 sbatch seq  : " << bs10;
  // KLOG(INF) << "time for 12 sbatch seq  : " << bs12;
  KLOG(INF) << "time for 16 sbatch seq  : " << bs16;


  KLOG(INF) << "time for 16 sbatch preq : " << bsp16;

  return 0;
}

