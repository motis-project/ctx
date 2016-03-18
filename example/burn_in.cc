#include <algorithm>
#include <chrono>
#include <iostream>
#include <functional>
#include <queue>
#include <thread>
#include <random>
#include <vector>

#include "boost/thread/thread.hpp"

#include "ctx/ctx.h"

using namespace ctx;
using namespace std::chrono_literals;
using sys_clock = std::chrono::system_clock;

struct simple_data {
  void on_resume(op_id) {}
  void on_finish(op_id) {}
  void on_suspend(op_id, op_id) {}
};

constexpr auto kDesiredWorkload = 100;
constexpr auto kWorkerCount = 8;
constexpr auto kRunTime = 10s;

std::random_device rd;
std::mt19937 gen(rd());

std::bernoulli_distribution branch_dist(0.1);
std::bernoulli_distribution sleep_dist(0.2);
std::bernoulli_distribution throw_dist(0.2);
std::bernoulli_distribution catch_dist(0.8);
std::exponential_distribution<> sleep_dur_dist;

std::uniform_int_distribution<> int_dist(1, 6);

void sleep_maybe() {
  if (branch_dist(gen)) {
    auto ms = std::min(500, static_cast<int>(sleep_dur_dist(gen) * 10));
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  }
}

int work() {
  sleep_maybe();

  if (sleep_dist(gen)) {
    throw std::runtime_error("error");
  }

  if (branch_dist(gen)) {
    std::vector<future_ptr<simple_data, int>> futures;
    auto count = int_dist(gen);
    for (int i = 0; i < count; ++i) {
      futures.push_back(ctx_call(simple_data(), work));
    }

    int result = 0;
    for (int i = 0; i < count; ++i) {
      if (catch_dist(gen)) {
        try {
          result += futures[i]->val();
        } catch (...) {
        }
      } else {
        result += futures[i]->val();
      }
    }
    return result;
  }
  return 1;
}

struct controller {
  void run() {
    end_time_ = sys_clock::now() + kRunTime;

    // initial workload
    while (open_.size() < kDesiredWorkload) {
      submit_work();
    }

    // refill
    while (end_time_ > sys_clock::now()) {
      finish_work();
      submit_work();
    }

    // finalize
    while (!open_.empty()) {
      finish_work();
    }
  }

  void submit_work() {
    open_.push(ctx_call(simple_data(), work));
    ++submitted_;
  }

  void finish_work() {
    auto fut = open_.front();
    open_.pop();
    try {
      fut->val();
      ++completed_;
    } catch (...) {
      ++exception_;
    }
  }

  std::queue<future_ptr<simple_data, int>> open_;

  sys_clock::time_point end_time_;
  unsigned long submitted_ = 0;
  unsigned long completed_ = 0;
  unsigned long exception_ = 0;
};

int main() {
  std::cout << "burn in started" << std::endl;
  auto start_time = sys_clock::now();
  controller c;

  scheduler<simple_data> sched;
  sched.enqueue(simple_data(), std::bind(&controller::run, &c),
                op_id("?", "?", 0));

  std::vector<boost::thread> threads(kWorkerCount);
  for (int i = 0; i < kWorkerCount; ++i) {
    threads[i] = boost::thread([&]() { sched.ios_.run(); });
  }
  std::for_each(begin(threads), end(threads),
                [](boost::thread& t) { t.join(); });

  using namespace std::chrono;
  auto millies = duration_cast<milliseconds>(sys_clock::now() - start_time);
  std::cout << "burn in completed " << millies.count() << "ms" << std::endl;
  std::cout << " submitted: " << c.submitted_ << " completed: " << c.completed_
            << " exception: " << c.exception_ << std::endl;
  if (c.submitted_ - (c.completed_ + c.exception_) == 0) {
    std::cout << "all jobs processed successfully!" << std::endl;
  }
}
