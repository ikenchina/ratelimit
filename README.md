# rate limit

Provides a C++ rate limit algorithm.   

supported algorithms:
- [x] token-bucket 
- [] 


## token bucket

### Usage

include header
```
#include "token_bucket.h"
```


#### New an instance


```
static std::shared_ptr<RateLimiter> NewBucket(std::chrono::nanoseconds fillInterval, int64_t quantum, int64_t capacity);
```
- fillInterval : interval between each tick
- quantum : the number of tokens are added to bucket
- capacity : the capacity of the bucket


```
static std::shared_ptr<RateLimiter> NewBucketWithRate(double rate, int64_t capacity);
```
- rate : the number of tokens are added to bucket per second
- capacity : the capacity of the bucket



#### Take

```
std::chrono::nanoseconds Take(int64_t count)
```
get count tokens from bucket without blocking.  

**parameters**
- count : the number of tokens 

**return**
- duration that the caller should wait 


#### TakeAvailable
```
int64_t TakeAvailable(int64_t count)
```
get min(count, available) tokens from the bucket.

**parameters**
- count : the number of tokens

**return**
- int64_t : the number of tokens obtained



#### Available

```
int64_t Available()
```

returns the number of available tokens

**return**
- int64_t : the number of available tokens



#### TakeMaxDuration
```
std::chrono::nanoseconds TakeMaxDuration(int64_t count, std::chrono::nanoseconds maxWait)
```

get count tokens from bucket, if the wait time returned is no greater than maxWait.

**paremeters**
- count : the number of tokens wish to obtain
- maxWait : max waiting time 

**return**
- std::chrono::nanoseconds : 
  - if the wait time which is returned is greater than maxWait, it does nothing.
  - if the wait time which is returned is not greater than maxWait, the count token becomes available.


#### Wait
```
void Wait(int64_t count)
```

wait takes count tokens from the bucket, waiting until they are available.

**parameters**
- count : the number of tokens 


#### WaitMaxDuration
```
bool WaitMaxDuration(int64_t count, std::chrono::nanoseconds maxWait)
```

take tokens from the bucket if it needs to wait for no greater than maxWait.

**parameters**
- count : the number of tokens
- maxWait : max waiting time

**return**
- bool : 
  - if the wait time is greater than maxWait, it does nothing and return false. 
  - if the wait time is not greater than maxWait, it takes count tokens until they are available.




