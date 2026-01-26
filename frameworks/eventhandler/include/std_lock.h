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
 	 
#ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_STD_LOCK_H
#define BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_STD_LOCK_H

#include "lock_base.h"
#include <mutex>

namespace OHOS {
namespace AppExecFwk {

class StdLock : public LockBase {
public:
    void lock() override
    {
        mutex_.lock();
    }
    
    void unlock() override
    {
        mutex_.unlock();
    }

private:
    std::mutex mutex_;
};

} // namespace AppExecFwk
} // namespace OHOS

#endif  // #ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_STD_LOCK_H