
#pragma once

#include <cstdint>
#include <chrono>
#include <mutex>
#include <memory>
#include "ratelimiter.h"

namespace ratelimit
{

    class TokenBucketRateLimiter : public RateLimiter
    {
    public:
        static std::shared_ptr<RateLimiter> NewBucket(std::chrono::nanoseconds fillInterval, int64_t quantum, int64_t capacity);
        static std::shared_ptr<RateLimiter> NewBucketWithRate(double rate, int64_t capacity);

        virtual ~TokenBucketRateLimiter() = default;

        virtual std::chrono::nanoseconds Take(int64_t count);
        virtual int64_t TakeAvailable(int64_t count);
        virtual int64_t Available();
        virtual std::chrono::nanoseconds TakeMaxDuration(int64_t count, std::chrono::nanoseconds maxWait);

        virtual void Wait(int64_t count);
        virtual bool WaitMaxDuration(int64_t count, std::chrono::nanoseconds maxWait);

        virtual std::chrono::system_clock::time_point GetStartTime() { return startTime_; }
        virtual double Rate();

    protected:
        TokenBucketRateLimiter(){};
        TokenBucketRateLimiter(std::chrono::nanoseconds fillInterval, int64_t quantum, int64_t capacity);
        TokenBucketRateLimiter(double rate, int64_t capacity);

        inline std::chrono::system_clock::time_point snow() { return std::chrono::system_clock::now(); }
        int64_t nextQuantum(int64_t q);

        int64_t takeAvailable(const std::chrono::system_clock::time_point &now, int64_t count);
        int64_t currentTick(std::chrono::system_clock::time_point now);
        void adjustAvailableTokens(int64_t tick);
        virtual std::chrono::nanoseconds take(std::chrono::system_clock::time_point now, int64_t count, std::chrono::nanoseconds maxWait);

    protected:
        // the time when bucket was created
        std::chrono::system_clock::time_point startTime_;
        // the capacity of bucket
        int64_t capacity_ = 0;
        // the number of tokens are added on each tick
        int64_t quantum_ = 0;
        // the interval between each tick
        std::chrono::nanoseconds fillInterval_;

        std::mutex mutex_;
        int64_t availableTokens_ = 0;
        int64_t latestTick_ = 0;

        const double rateMargin = 0.01;
        const std::chrono::nanoseconds infinityDuration_ = std::chrono::nanoseconds(0x7fffffffffffffff);
    };

} // namespace ratelimit