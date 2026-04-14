
/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_ASYNC_STACK_ADAPTER_H
#define BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_ASYNC_STACK_ADAPTER_H

#include <cstdint>
 
namespace OHOS {
namespace AppExecFwk {

using EventSetStackIdFunc = void(*)(uint64_t stackId);
using EventCollectAsyncStackFunc = uint64_t(*)(uint64_t taskType);
using EventReleaseStackIdFunc = void(*)(uint64_t stackId);

class AsyncStackAdapter final {
public:
    static AsyncStackAdapter& GetInstance();
    void SetAsyncStackFunc(EventCollectAsyncStackFunc func);
    void SetStackIdFunc(EventSetStackIdFunc func);
    void SetReleaseStackIdFunc(EventReleaseStackIdFunc func);
    uint64_t EventCollectAsyncStack(uint64_t type);
    void EventSetStackId(uint64_t stackId);
    void EventReleaseStackId(uint64_t stackId);

private:
    AsyncStackAdapter();
    ~AsyncStackAdapter();
    EventCollectAsyncStackFunc asyncStackFunc = nullptr;
    EventSetStackIdFunc stackIdFunc = nullptr;
    EventReleaseStackIdFunc releaseStackIdFunc = nullptr;
};
}  // namespace AppExecFwk
}  // namespace OHOS
 
#endif  // #ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_ASYNC_STACK_ADAPTER_H