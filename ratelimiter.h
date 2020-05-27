#pragma once

#include <chrono>
#include <memory>

namespace ratelimit
{

    class RateLimiter
    {
    public:
        virtual std::chrono::nanoseconds Take(int64_t count) = 0;
        virtual int64_t TakeAvailable(int64_t count) = 0;
        virtual int64_t Available() = 0;
        virtual std::chrono::nanoseconds TakeMaxDuration(int64_t count, std::chrono::nanoseconds maxWait) = 0;

        virtual void Wait(int64_t count) = 0;
        virtual bool WaitMaxDuration(int64_t count, std::chrono::nanoseconds maxWait) = 0;
    };

} // namespace ratelimit
