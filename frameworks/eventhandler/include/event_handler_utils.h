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

#ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_EVENT_HANDLER_UTILS_H
#define BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_EVENT_HANDLER_UTILS_H

#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>

#include "hitrace/trace.h"
#include "inner_event.h"

#define LOCAL_API __attribute__((visibility ("hidden")))
namespace OHOS {
namespace AppExecFwk {
inline const int64_t NANOSECONDS_PER_ONE_MILLISECOND = 1000000;
inline const int64_t NANOSECONDS_PER_ONE_SECOND = 1000000000;
inline const int32_t INFINITE_TIMEOUT = -1;
inline const uint8_t MAX_ERRORMSG_LEN = 128;

// Help to convert time point into delay time from now.
LOCAL_API static inline int64_t TimePointToTimeOut(const InnerEvent::TimePoint &when)
{
    InnerEvent::TimePoint now = InnerEvent::Clock::now();
    if (when <= now) {
        return 0;
    }

    auto duration = when - now;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

LOCAL_API static inline int32_t NanosecondsToTimeout(int64_t nanoseconds)
{
    if (nanoseconds < 0) {
        return INFINITE_TIMEOUT;
    }

    int64_t milliseconds = nanoseconds / NANOSECONDS_PER_ONE_MILLISECOND;
    if ((nanoseconds % NANOSECONDS_PER_ONE_MILLISECOND) > 0) {
        milliseconds += 1;
    }

    return (milliseconds > INT32_MAX) ? INT32_MAX : static_cast<int32_t>(milliseconds);
}

using HiTraceChain = OHOS::HiviewDFX::HiTraceChain;

LOCAL_API static inline bool AllowHiTraceOutPut(const std::shared_ptr<HiTraceId>& traceId, bool isSyncEvent)
{
    if ((!traceId) || (!traceId->IsValid())) {
        return false;
    }
    if ((!isSyncEvent) && (!traceId->IsFlagEnabled(HITRACE_FLAG_INCLUDE_ASYNC))) {
        return false;
    }
    return true;
}

LOCAL_API static inline void HiTracePointerOutPutEventId(const std::shared_ptr<HiTraceId> &spanId, const char *action,
    HiTraceTracepointType type, const InnerEvent::EventId &innerEventId)
{
    if (spanId == nullptr || action == nullptr) {
        return;
    }
    if (innerEventId.index() == TYPE_U32_INDEX) {
        HiTraceChain::Tracepoint(type, *spanId, "%s event, event id: %u", action, std::get<uint32_t>(innerEventId));
    } else {
        HiTraceChain::Tracepoint(
            type, *spanId, "%s event, event id: %s", action, std::get<std::string>(innerEventId).c_str());
    }
}

LOCAL_API static inline void HiTracePointerOutPut(const std::shared_ptr<HiTraceId>& spanId,
    const InnerEvent::Pointer& event, const char* action, HiTraceTracepointType type)
{
    if (!event->HasTask()) {
        auto eventId = event->GetInnerEventIdEx();
        HiTracePointerOutPutEventId(spanId, action, type, eventId);
    } else if (!event->GetTaskName().empty()) {
        HiTraceChain::Tracepoint(type, *spanId, "%s task with name, name: %s", action, event->GetTaskName().c_str());
    } else {
        HiTraceChain::Tracepoint(type, *spanId, "%s UnNamed Task", action);
    }
}

LOCAL_API static inline void GetLastErr(char *errmsg, size_t size = MAX_ERRORMSG_LEN)
{
    size = size > MAX_ERRORMSG_LEN ? MAX_ERRORMSG_LEN : size;
    strerror_r(errno, errmsg, size);
}

LOCAL_API static inline int64_t GetTimeStamp()
{
    InnerEvent::TimePoint now = InnerEvent::Clock::now();
    auto epoch = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count();
}
}  // namespace AppExecFwk
}  // namespace OHOS

#endif  // #ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_EVENT_HANDLER_UTILS_H
