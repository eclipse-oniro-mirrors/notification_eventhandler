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
#include <mutex>
#include <map>
#include <string>
#include "inner_event.h"

namespace OHOS {
namespace AppExecFwk {
constexpr uint64_t HITRACE_TAG_NOTIFICATION = (1ULL << 40); // Notification module tag.
bool IsTagEnabled(uint64_t tag);
void StartTrace(uint64_t label, const std::string& value, float limit = -1);
void FinishTrace(uint64_t label);
using IsTagEnabledType = decltype(IsTagEnabled);
using StartTraceType = decltype(StartTrace);
using FinishTraceType = decltype(FinishTrace);

#ifdef APP_USE_ARM
static const std::string TRACE_LIB_PATH = "/system/lib/chipset-pub-sdk/libhitrace_meter.so";
#else
static const std::string TRACE_LIB_PATH = "/system/lib64/chipset-pub-sdk/libhitrace_meter.so";
#endif

class TraceAdapter {
public:
    TraceAdapter()
    {
        Load();
    }

    ~TraceAdapter()
    {
        functionMap_.clear();
    }

    static TraceAdapter* Instance()
    {
        static TraceAdapter instance;
        return &instance;
    }

    template<typename T>
    T *GetFunction(const std::string &symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle_ == nullptr) {
            HILOGE("get handle failed, handle is null.");
            return nullptr;
        }

        auto iterAddr = functionMap_.find(TRACE_LIB_PATH + '|' + symbol);
        if (iterAddr != functionMap_.end()) {
            return reinterpret_cast<T *>(iterAddr->second);
        }
        auto addr = dlsym(handle_, symbol.c_str());
        if (addr == nullptr) {
            HILOGE("dlsymc %{public}s error", symbol.c_str());
            return nullptr;
        }
        functionMap_[TRACE_LIB_PATH + "|" + symbol] = reinterpret_cast<void *>(addr);
        return reinterpret_cast<T *>(addr);
    }
private:
    void Load()
    {
        if (handle_ != nullptr) {
            HILOGD("%{public}s is already dlopened.", TRACE_LIB_PATH.c_str());
            return;
        }

        handle_ = dlopen(TRACE_LIB_PATH.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle_ == nullptr) {
            HILOGE("dlopen %{public}s failed.", TRACE_LIB_PATH.c_str());
            return;
        }
    }

    DEFINE_EH_HILOG_LABEL("EventHiTraceAdapter");
    void* handle_ = nullptr;
    std::mutex mutex_;
    std::map<std::string, void *> functionMap_;
};

static inline void StartTraceAdapter(const InnerEvent::Pointer &event)
{
    auto IsTagEnabledFunc = TraceAdapter::Instance()->GetFunction<IsTagEnabledType>("IsTagEnabled");
    auto StartTraceFunc = TraceAdapter::Instance()->GetFunction<StartTraceType>("StartTrace");
    if (IsTagEnabledFunc && StartTraceFunc) {
        if (IsTagEnabledFunc(HITRACE_TAG_NOTIFICATION)) {
            StartTraceFunc(HITRACE_TAG_NOTIFICATION, event->TraceInfo(), -1);
        }
    }

}

static inline void FinishTraceAdapter()
{
    auto FinishTraceFunc = TraceAdapter::Instance()->GetFunction<FinishTraceType>("FinishTrace");
    if (FinishTraceFunc) {
        FinishTraceFunc(HITRACE_TAG_NOTIFICATION);
    }
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
