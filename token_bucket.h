
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
#define stime std::chrono::system_clock::time_point
#define nanoduration std::chrono::nanoseconds

    public:
        static std::shared_ptr<RateLimiter> NewBucket(std::chrono::nanoseconds fillInterval, int64_t quantum, int64_t capacity);
        static std::shared_ptr<RateLimiter> NewBucketWithRate(double rate, int64_t capacity);

        virtual ~TokenBucketRateLimiter() = default;

        virtual nanoduration Take(int64_t count);
        virtual int64_t TakeAvailable(int64_t count);
        virtual int64_t Available();
        virtual std::chrono::nanoseconds TakeMaxDuration(int64_t count, std::chrono::nanoseconds maxWait);

        virtual void Wait(int64_t count);
        virtual bool WaitMaxDuration(int64_t count, nanoduration maxWait);

        virtual stime GetStartTime() { return startTime_; }
        virtual double Rate();

    protected:
        TokenBucketRateLimiter(){};
        TokenBucketRateLimiter(nanoduration fillInterval, int64_t quantum, int64_t capacity);
        TokenBucketRateLimiter(double rate, int64_t capacity);

        inline stime snow() { return std::chrono::system_clock::now(); }
        int64_t nextQuantum(int64_t q);

        int64_t takeAvailable(const stime &now, int64_t count);
        int64_t currentTick(stime now);
        void adjustAvailableTokens(int64_t tick);
        virtual nanoduration take(stime now, int64_t count, nanoduration maxWait);

    protected:
        // the time when bucket was created
        stime startTime_;
        // the capacity of bucket
        int64_t capacity_ = 0;
        // the number of tokens are added on each tick
        int64_t quantum_ = 0;
        // the interval between each tick
        nanoduration fillInterval_;

        std::mutex mutex_;
        int64_t availableTokens_ = 0;
        int64_t latestTick_ = 0;

        const double rateMargin = 0.01;
        const nanoduration infinityDuration_ = nanoduration(0x7fffffffffffffff);
    };

} // namespace ratelimit