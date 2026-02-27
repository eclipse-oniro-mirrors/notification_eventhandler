 /*
  * Copyright (c) 2025-2026 Huawei Device Co., Ltd.
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *     http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */

#ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_PI_LOCK_H
#define BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_PI_LOCK_H

#include "lock_base.h"
#include <pthread.h>

namespace OHOS {
namespace AppExecFwk {

class PriorityInheritanceLock : public LockBase {
public:
    PriorityInheritanceLock()
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
        pthread_mutex_init(&mutex_, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    
    ~PriorityInheritanceLock()
    {
        pthread_mutex_destroy(&mutex_);
    }
    
    void lock() override
    {
        pthread_mutex_lock(&mutex_);
    }
    
    void unlock() override
    {
        pthread_mutex_unlock(&mutex_);
    }

private:
    pthread_mutex_t mutex_;
};

} // namespace AppExecFwk
} // namespace OHOS

#endif  // #ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_PI_LOCK_H