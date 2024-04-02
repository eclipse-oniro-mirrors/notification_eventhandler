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
#include <dlfcn.h>
#include "inner_event.h"

namespace OHOS {
namespace AppExecFwk {
constexpr uint64_t HITRACE_TAG_NOTIFICATION = (1ULL << 40); // Notification module tag.
bool IsTagEnabled(uint64_t tag);
void StartTrace(uint64_t label, const std::string& value, float limit = -1);
void FinishTrace(uint64_t label);

#ifdef APP_USE_ARM
static const std::string TRACE_LIB_PATH = "/system/lib/chipset-pub-sdk/libhitrace_master.so";
#else
static const std::string TRACE_LIB_PATH = "/system/lib64/chipset-pub-sdk/libhitrace_master.so";
#endif

class TraceAdapter {
public:
    TraceAdapter()
    {
        Load();
    }

    ~TraceAdapter()
    {
        Unload();
    }

    static TraceAdapter* Instance()
    {
        static TraceAdapter instance;
        return &instance;
    }

#define REG_FUNC(func) using func##Type = decltype(func)*; func##Type func = nullptr
    REG_FUNC(IsTagEnabled);
    REG_FUNC(StartTrace);
    REG_FUNC(FinishTrace);
#undef REG_FUNC

private:
    bool Load()
    {
        if (handle != nullptr) {
            return true;
        }

        handle = dlopen(TRACE_LIB_PATH.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            return false;
        }
#define LOAD_FUNC(x) x = reinterpret_cast<x##Type>(dlsym(handle, #x));        \
        if (x == nullptr)                                                     \
        {                                                                     \
            return false;                                                     \
        }
            LOAD_FUNC(IsTagEnabled);
            LOAD_FUNC(StartTrace);
            LOAD_FUNC(FinishTrace);

#undef LOAD_FUNC
        return true;
    }

    bool Unload()
    {
        if (handle != nullptr) {
            if (dlclose(handle) != 0) {
                return false;
            }
            handle = nullptr;
            return true;
        }
        return true;
    }

    void* handle = nullptr;
};

static inline void StartTraceAdapter(const InnerEvent::Pointer &event)
{
    if (TraceAdapter::Instance()->IsTagEnabled(HITRACE_TAG_NOTIFICATION)) {
        TraceAdapter::Instance()->StartTrace(HITRACE_TAG_NOTIFICATION, event->TraceInfo(), -1);
    }
}

static inline void FinishTraceAdapter()
{
    TraceAdapter::Instance()->FinishTrace(HITRACE_TAG_NOTIFICATION);
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
