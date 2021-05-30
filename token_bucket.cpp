#include <assert.h>
#include <cmath>
#include <thread>
#include <mutex>
#include <stdexcept>
#include <thread>
#include "token_bucket.h"

namespace ratelimit
{
#define stime std::chrono::system_clock::time_point
#define nanoduration std::chrono::nanoseconds

    std::shared_ptr<RateLimiter> TokenBucketRateLimiter::NewBucket(std::chrono::nanoseconds fillInterval, int64_t quantum, int64_t capacity)
    {
        if (fillInterval <= std::chrono::nanoseconds(0))
        {
            throw std::invalid_argument("fill interval <= 0.");
        }
        if (quantum <= int64_t(0))
        {
            throw std::invalid_argument("quantum <= 0.");
        }
        if (capacity <= 0)
        {
            throw std::invalid_argument("capacity <= 0.");
        }
        std::shared_ptr<TokenBucketRateLimiter> rl(new TokenBucketRateLimiter(fillInterval, quantum, capacity));
        return rl;
    }

    std::shared_ptr<RateLimiter> TokenBucketRateLimiter::NewBucketWithRate(double rate, int64_t capacity)
    {
        if (rate <= 0.0)
            throw std::invalid_argument("rate <= 0.");
        if (capacity <= 0)
            throw std::invalid_argument("capacity <= 0.");

        std::shared_ptr<TokenBucketRateLimiter> tb(new TokenBucketRateLimiter(rate, capacity));

        return tb;
    }

    TokenBucketRateLimiter::TokenBucketRateLimiter(double rate, int64_t capacity) : TokenBucketRateLimiter(std::chrono::nanoseconds(1), 1, capacity)
    {
        for (int64_t quantum = 1; quantum < int64_t(1) << 50; quantum = nextQuantum(quantum))
        {
            nanoduration fillInterval(int64_t(double(1e9) * double(quantum) / rate));
            if (fillInterval < nanoduration(0))
                continue;

            fillInterval_ = fillInterval;
            quantum_ = quantum;

            auto diff = std::fabs(Rate() - rate);
            if (diff / rate <= rateMargin)
                return;
        }
        throw std::logic_error("no suitable quantum.");
    }

    TokenBucketRateLimiter::TokenBucketRateLimiter(nanoduration fillInterval, int64_t quantum, int64_t capacity)
    {
        startTime_ = snow();
        fillInterval_ = fillInterval;
        quantum_ = quantum;
        capacity_ = capacity;
        availableTokens_ = capacity_;
    }

    double TokenBucketRateLimiter::Rate()
    {
        return 1e9 * double(quantum_) / double(fillInterval_.count());
    }

    std::chrono::nanoseconds TokenBucketRateLimiter::Take(int64_t count)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return take(snow(), count, infinityDuration_);
    }

    int64_t TokenBucketRateLimiter::TakeAvailable(int64_t count)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return takeAvailable(snow(), count);
    }

    std::chrono::nanoseconds TokenBucketRateLimiter::TakeMaxDuration(int64_t count, std::chrono::nanoseconds maxWait)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return take(snow(), count, maxWait);
    }

    int64_t TokenBucketRateLimiter::Available()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        adjustAvailableTokens(currentTick(snow()));
        return availableTokens_;
    }

    void TokenBucketRateLimiter::Wait(int64_t count)
    {
        auto t = Take(count);
        if (t.count() > 0)
            std::this_thread::sleep_for(t);
    }

    bool TokenBucketRateLimiter::WaitMaxDuration(int64_t count, nanoduration maxWait)
    {
        auto d = TakeMaxDuration(count, maxWait);
        if (d > maxWait)
            return false;
        else if (d > std::chrono::nanoseconds(0))
            std::this_thread::sleep_for(d);
        return true;
    }

    int64_t TokenBucketRateLimiter::nextQuantum(int64_t q)
    {
        auto q1 = q * 11 / 10;
        if (q1 == q)
            q1++;
        return q1;
    }

    int64_t TokenBucketRateLimiter::takeAvailable(const stime &now, int64_t count)
    {
        if (count <= 0)
            return 0;

        adjustAvailableTokens(currentTick(snow()));
        if (availableTokens_ <= 0)
            return 0;

        if (count > availableTokens_)
            count = availableTokens_;

        availableTokens_ -= count;
        return count;
    }

    int64_t TokenBucketRateLimiter::currentTick(stime now)
    {
        return (now - startTime_).count() / fillInterval_.count();
    }

    void TokenBucketRateLimiter::adjustAvailableTokens(int64_t tick)
    {
        auto lastTick = latestTick_;
        latestTick_ = tick;
        if (availableTokens_ >= capacity_)
            return;

        availableTokens_ += (tick - lastTick) * quantum_;
        if (availableTokens_ > capacity_)
            availableTokens_ = capacity_;

        return;
    }

    std::chrono::nanoseconds TokenBucketRateLimiter::take(stime now, int64_t count, nanoduration maxWait)
    {
        if (count <= 0)
            return nanoduration(0);

        auto tick = currentTick(now);
        adjustAvailableTokens(tick);

        int64_t avail = availableTokens_ - count;
        if (avail >= 0)
        {
            availableTokens_ = avail;
            return nanoduration(0);
        }

        int64_t endTick = tick + (-avail + quantum_ - 1) / quantum_;
        auto endTime = startTime_ + nanoduration(endTick * fillInterval_.count());
        auto waitTime = endTime - now;
        if (waitTime > maxWait)
            return waitTime;

        availableTokens_ = avail;
        return waitTime;
    }

} // namespace ratelimit
