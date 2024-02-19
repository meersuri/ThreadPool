#include <thread>
#include <gtest/gtest.h>

#include "thread_pool.hpp"

using namespace std::chrono_literals;

TEST(SingleWorkerTests, SubmitLambdaWorks) {
    ThreadPool pool("pool", 1);
    bool done = false;
    auto task = [&done]() { 
        std::this_thread::sleep_for(100ms);
        done = true;
        return false;
    };
    pool.submitTask(task);
    pool.wait();
    ASSERT_TRUE(done);
}

bool sampleTask(int& x) {
    x++;
    return false;
}

TEST(SingleWorkerTests, PassByRefWorks) {
    ThreadPool pool("pool", 1);
    int x = 3;
    pool.submitTask(sampleTask, x);
    pool.wait();
    ASSERT_EQ(x, 4);
}

bool repeatTask(int& x) {
    x++;
    if (x == 20) {
        return false;
    }
    return true;
}

TEST(SingleWorkerTests, RepeatTaskWorks) {
    spdlog::set_level(spdlog::level::debug);
    ThreadPool pool("pool", 1);
    int x = 1;
    pool.submitTask(repeatTask, x);
    pool.wait();
    ASSERT_EQ(x, 20);
}

bool recurringTask(int& x) {
    x++;
    return true;
}

TEST(SingleWorkerTests, StopWorks) {
    ThreadPool pool("pool", 1);
    int x = 3;
    pool.submitTask(sampleTask, x);
    std::this_thread::sleep_for(100ms);
    pool.stop();
}

bool sumNums(int x, int y, int& z) {
    z = x + y;
    return false;
}

TEST(SingleWorkerTests, RValuesWork) {
    ThreadPool pool("pool", 1);
    int y = 5;
    int z = 0;
    pool.submitTask(sumNums, 3, std::move(y), z);
    pool.wait();
    ASSERT_EQ(z, 8);
}
