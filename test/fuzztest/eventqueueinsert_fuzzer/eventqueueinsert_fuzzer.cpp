/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "event_queue.h"
#include "event_queue_base.h"
#include "eventqueueinsert_fuzzer.h"
#include "securec.h"

namespace OHOS {

bool DoSomethingInterestingWithMyAPI(FuzzedDataProvider *fdp)
{
    std::shared_ptr<AppExecFwk::IoWaiter> ioWaiter = nullptr;
    AppExecFwk::EventQueueBase eventQueue(ioWaiter, AppExecFwk::EventLockType::STANDARD);
    uint32_t innerEventId = fdp->ConsumeIntegral<uint32_t>();
    std::shared_ptr<AppExecFwk::EventHandler> myHandler;
    eventQueue.HasInnerEvent(myHandler, innerEventId);
    AppExecFwk::InnerEvent::Pointer event = AppExecFwk::InnerEvent::Get(innerEventId);
    AppExecFwk::EventQueue::Priority priority = AppExecFwk::EventQueue::Priority::LOW;
    eventQueue.Insert(event, priority);
    return true;
}
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    FuzzedDataProvider fdp(data, size);
    OHOS::DoSomethingInterestingWithMyAPI(&fdp);
    return 0;
}
