#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <cmath>

#include "token_bucket.h"

using namespace std;

#define nanoduration std::chrono::nanoseconds
#define secduration std::chrono::nanoseconds
#define stime std::chrono::system_clock::time_point
#define castNano(d) std::chrono::duration_cast<nanoduration>(d)

class testRateLimiter : public ratelimit::TokenBucketRateLimiter
{
public:
    testRateLimiter(nanoduration fillInterval, int64_t quantum, int64_t capacity) : ratelimit::TokenBucketRateLimiter(fillInterval, quantum, capacity)
    {
    }
    testRateLimiter(double rate, int64_t capacity) : ratelimit::TokenBucketRateLimiter(rate, capacity)
    {
    }
    virtual nanoduration take(stime now, int64_t count, nanoduration maxWait)
    {
        return ratelimit::TokenBucketRateLimiter::take(now, count, maxWait);
    }
    nanoduration GetInfinityDuration() { return infinityDuration_; }
    int64_t GetQuantum() { return quantum_; }
    nanoduration GetFillInterval() { return fillInterval_; }
    double GetRateMargin() { return rateMargin; }
};

TEST(MyTests, TestTake)
{
    struct testRequest
    {
        nanoduration time;
        int64_t count;
        nanoduration expectWait;
    };
    struct testCases
    {
        string name;
        nanoduration fillInterval;
        int64_t capacity;
        vector<testRequest> reqs;
    };

    vector<testCases> cases = {
        {"requests",
         castNano(std::chrono::milliseconds(250)),
         10,
         {{
              nanoduration(0),
              0,
              nanoduration(0),
          },
          {
              nanoduration(0),
              10,
              nanoduration(0),
          },
          {
              nanoduration(0),
              1,
              castNano(std::chrono::milliseconds(250)),
          },
          {
              castNano(std::chrono::milliseconds(250)),
              1,
              castNano(std::chrono::milliseconds(250)),
          }}},
        {"simulate concurrent requests for calculation",
         castNano(std::chrono::milliseconds(250)),
         10,
         {{
              nanoduration(0),
              10,
              nanoduration(0),
          },
          {
              nanoduration(0),
              2,
              castNano(std::chrono::milliseconds(500)),
          },
          {
              nanoduration(0),
              2,
              castNano(std::chrono::milliseconds(1000)),
          },
          {
              nanoduration(0),
              1,
              castNano(std::chrono::milliseconds(1250)),
          }}},
        {"quantum",
         castNano(std::chrono::milliseconds(10)),
         10,
         {{
              nanoduration(0),
              10,
              nanoduration(0),
          },
          {
              castNano(std::chrono::milliseconds(7)),
              1,
              castNano(std::chrono::milliseconds(3)),
          },
          {
              castNano(std::chrono::milliseconds(8)),
              1,
              castNano(std::chrono::milliseconds(12)),
          },
          {
              castNano(std::chrono::milliseconds(30)),
              1,
              castNano(std::chrono::milliseconds(0)),
          }}},
        {"capacity",
         castNano(std::chrono::milliseconds(10)),
         5,
         {{
              nanoduration(0),
              10,
              castNano(std::chrono::milliseconds(50)),
          },
          {
              castNano(std::chrono::milliseconds(60)),
              5,
              castNano(std::chrono::milliseconds(40)),
          },
          {
              castNano(std::chrono::milliseconds(60)),
              1,
              castNano(std::chrono::milliseconds(50)),
          }}}};

    for (auto &ca : cases)
    {
        auto rl = std::shared_ptr<testRateLimiter>(new testRateLimiter(ca.fillInterval, 1, ca.capacity));
        for (auto &req : ca.reqs)
        {
            auto wait = rl->take(rl->GetStartTime() + req.time, req.count, castNano(std::chrono::seconds(10)));

            EXPECT_EQ(req.expectWait, wait);
        }
    }

    for (auto &ca : cases)
    {
        auto rl = std::shared_ptr<testRateLimiter>(new testRateLimiter(ca.fillInterval, 1, ca.capacity));
        for (auto &req : ca.reqs)
        {
            if (req.expectWait > nanoduration(0))
            {
                auto fakeWaitExpect = req.expectWait - nanoduration(1);
                auto w = rl->take(rl->GetStartTime() + req.time, req.count, fakeWaitExpect);
                EXPECT_TRUE(w == req.expectWait);
            }
            auto w = rl->take(rl->GetStartTime() + req.time, req.count, req.expectWait);
            EXPECT_TRUE(req.expectWait == w);
        }
    }
}

TEST(MyTests, TestException)
{
    bool fill = false;
    bool quantum = false;
    bool capacity = false;
    try
    {
        testRateLimiter::NewBucket(nanoduration(0), 1, 1);
    }
    catch (std::invalid_argument e)
    {
        fill = true;
    }
    EXPECT_TRUE(fill);

    try
    {
        testRateLimiter::NewBucket(nanoduration(1), 0, 1);
    }
    catch (std::invalid_argument e)
    {
        quantum = true;
    }
    EXPECT_TRUE(quantum);

    try
    {
        testRateLimiter::NewBucket(nanoduration(2), 1, 0);
    }
    catch (std::invalid_argument e)
    {
        capacity = true;
    }
    EXPECT_TRUE(capacity);

    bool rate = false;
    capacity = false;
    try
    {
        testRateLimiter::NewBucketWithRate(0, 1);
    }
    catch (std::invalid_argument e)
    {
        rate = true;
    }
    EXPECT_TRUE(rate);

    try
    {
        testRateLimiter::NewBucketWithRate(1, 0);
    }
    catch (std::invalid_argument e)
    {
        capacity = true;
    }
    EXPECT_TRUE(capacity);
}

TEST(MyTests, TestRate)
{

    auto proxCompare = [](double x, double y, double tolerance) -> bool {
        return std::fabs(x - y) / y < tolerance;
    };

    {
        //auto rl = testRateLimiter::NewBucket(nanoduration(1), 1, 1);
        auto rl = std::shared_ptr<testRateLimiter>(new testRateLimiter(nanoduration(1), 1, 1));
        EXPECT_EQ(1e9, rl->Rate());
    }
    {
        auto check = [&](double rate) {
            {
                auto rl = std::shared_ptr<testRateLimiter>(new testRateLimiter(rate, int64_t(1) << 62));

                ASSERT_TRUE(proxCompare(rl->Rate(), rate, rl->GetRateMargin())) << "want " << rate;
                auto w = rl->take(rl->GetStartTime(), int64_t(1) << 62, rl->GetInfinityDuration());
                EXPECT_EQ(nanoduration(0), w);

                w = rl->take(rl->GetStartTime(), rl->GetQuantum() * 2 - rl->GetQuantum() / 2, rl->GetInfinityDuration());
                auto expectTime = 1e9 * double(rl->GetQuantum()) * 2 / rl->Rate();
                ASSERT_TRUE(proxCompare(expectTime, w.count(), rl->GetRateMargin()));
            }
        };
        for (auto rate = double(1); rate < 1e6; rate += 7)
        {
            check(rate);
        }
        for (auto rate : (double[]){1024 * 1024 * 1024, 1e-5, 0.9e-5, 0.5, 0.9e8, 3e12, 4e18, double((uint64_t(1) << 63) - 1)})
        {
            check(rate);
            check(rate / 3);
            check(rate * 1.3);
        }
    }
}

TEST(MyTest, TestWaitThread)
{

    auto waitThread = [&](double rate, int64_t capacity, nanoduration cost) {
        auto start = std::chrono::system_clock::now();
        auto rl = std::shared_ptr<testRateLimiter>(new testRateLimiter(rate, capacity));

        for (int i = 0; i < 20; i++)
        {
            rl->Wait(1);
        }

        auto end = std::chrono::system_clock::now();
        EXPECT_LE(cost.count(), (end - start).count());
        cout << "TestWaitThread :  rate: " << rate << " ,  expected duration : " << cost.count() << "    real duration : " << (end - start).count() << endl;
    };

    struct request
    {
        double rate;
        int64_t capacity;
        nanoduration cost;
    };

    for (auto req : (request[]){{10, 10, secduration(1)}, {10, 20, secduration(0)}, {20, 10, secduration(2)}})
    {
        waitThread(req.rate, req.capacity, req.cost);
    }
}

TEST(MyTest, TestBenchmarkWait)
{
    int64_t count = 1e7;

    auto rl = std::shared_ptr<testRateLimiter>(new testRateLimiter(1e9, int64_t(1) << 62));

    auto start = std::chrono::system_clock::now();
    for (int64_t i = 0; i < count; i++)
    {
        rl->Wait(1);
    }
    auto end = std::chrono::system_clock::now();

    cout << "[------ RUN ----]" << endl;
    cout << "TestBenchmarkWait cost : " << (end - start).count() << "  ,    cost per wait : " << (end - start).count() / count << endl;
}

TEST(MyTest, TestBenchmarkWaitConcurrent)
{
    int64_t count = 1e5;

    auto rl = std::shared_ptr<testRateLimiter>(new testRateLimiter(1e9, int64_t(1) << 62));

    auto job = [&]() {
        for (int64_t i = 0; i < count; i++)
        {
            rl->Wait(1);
        }
    };

    vector<thread *> threads;
    auto start = std::chrono::system_clock::now();

    auto cores = std::thread::hardware_concurrency();
    cores = cores > 1 ? cores - 1 : cores;
    for (unsigned int i = 0; i < cores; i++)
    {
        threads.push_back(new thread(job));
    }

    for (auto t : threads)
    {
        t->join();
    }

    auto end = std::chrono::system_clock::now();
    cout << "[------ RUN ----]" << endl;
    cout << "TestBenchmarkWaitConcurrent cost : " << (end - start).count() << "  ,    cost per wait : " << (end - start).count() / count << endl;
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
