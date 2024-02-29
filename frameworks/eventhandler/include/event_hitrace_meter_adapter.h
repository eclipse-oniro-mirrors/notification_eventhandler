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
#ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_HITRACE_METER_H
#define BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_HITRACE_METER_H

#ifdef EH_HITRACE_METER_ENABLE
#include "hitrace_meter.h"
#include "inner_event.h"

namespace OHOS {
namespace AppExecFwk {
static inline void StartTraceAdapter(const InnerEvent::Pointer &event)
{
    if (IsTagEnabled(HITRACE_TAG_NOTIFICATION)) {
        StartTrace(HITRACE_TAG_NOTIFICATION, event->TraceInfo());
    }
}

static inline void FinishTraceAdapter()
{
    FinishTrace(HITRACE_TAG_NOTIFICATION);
}
}}
#else
namespace OHOS {
namespace AppExecFwk {
static inline void StartTraceAdapter(const InnerEvent::Pointer &event)
{
}
static inline void FinishTraceAdapter()
{
}
}}
#endif
#endif // BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_HITRACE_METER_H
