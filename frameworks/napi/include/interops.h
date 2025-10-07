/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#ifndef BASE_EVENTHANDLER_FRAMEWORKS_INTEROPS_H
#define BASE_EVENTHANDLER_FRAMEWORKS_INTEROPS_H

#include <map>
#include <memory>
#include <atomic>
#include <unordered_set>

#include "composite_event.h"
#include "inner_event.h"
#include "event_handler.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AppExecFwk {

struct AsyncCallbackInfo {
    std::atomic<napi_env> env;
    std::atomic<bool> once = false;
    std::atomic<bool> isDeleted = false;
    napi_ref callback = 0;
    napi_threadsafe_function tsfn = nullptr;
    InnerEvent::EventId eventId;
    uint32_t emitterId = 0;
    ~AsyncCallbackInfo();
};

class EventHandlerInstance : public EventHandler {
public:
    EventHandlerInstance(const std::shared_ptr<EventRunner>& runner);
    static std::shared_ptr<EventHandlerInstance> GetInstance();
    ~EventHandlerInstance();
    void ProcessEvent(const InnerEvent::Pointer& event) override;
    void ProcessEventCallbacks(
        const std::unordered_set<std::shared_ptr<AsyncCallbackInfo>>& callbackInfos,
        std::shared_ptr<napi_value> eventData, size_t callbackSize);
    std::unordered_set<std::shared_ptr<AsyncCallbackInfo>> GetAsyncCallbackInfo(const CompositeEventId &compositeId);
    napi_env deleteEnv = nullptr;
};

static std::atomic<uint32_t> nextEmitterInstanceId {1};
uint32_t GetNextEmitterInstanceId();

using AsyncCallbackInfoContainer =
    std::map<CompositeEventId, std::unordered_set<std::shared_ptr<AsyncCallbackInfo>>>;
AsyncCallbackInfoContainer& GetAsyncCallbackInfoContainer();
std::mutex& GetAsyncCallbackInfoContainerMutex();

using EventData = std::shared_ptr<napi_value>;
struct AsyncCallbackInfo;
struct EventDataWorker {
    EventData data{nullptr};
    std::shared_ptr<AsyncCallbackInfo> callbackInfo{nullptr};
    std::shared_ptr<void> serializePtr {nullptr};
    bool isCrossRuntime{false};
    void* enhancedData {nullptr};
    bool IsEnhanced{false};
};

struct EmitterEnhancedApi {
    napi_value (*JS_Off)(napi_env env, napi_callback_info cbinfo) = nullptr;
    napi_value (*JS_Emit)(napi_env env, napi_callback_info cbinfo) = nullptr;
    napi_value (*JS_GetListenerCount)(napi_env env, napi_callback_info cbinfo) = nullptr;
    void (*ProcessEvent)(const InnerEvent::Pointer& event) = nullptr;
    void (*ProcessCallbackEnhanced)(const EventDataWorker* eventDataInner) = nullptr;

    napi_value (*JS_EmitterConstructor)(napi_env env, napi_callback_info cbinfo) = nullptr;
    napi_value (*JS_EmitterOn)(napi_env env, napi_callback_info cbinfo) = nullptr;
    napi_value (*JS_EmitterOnce)(napi_env env, napi_callback_info cbinfo) = nullptr;
    napi_value (*JS_EmitterOff)(napi_env env, napi_callback_info cbinfo) = nullptr;
    napi_value (*JS_EmitterEmit)(napi_env env, napi_callback_info cbinfo) = nullptr;
    napi_value (*JS_EmitterGetListenerCount)(napi_env env, napi_callback_info cbinfo) = nullptr;
};

class EmitterEnhancedApiRegister {
public:
    EmitterEnhancedApiRegister();
    void Register(const EmitterEnhancedApi& api);
    bool IsInit() const;
    std::shared_ptr<EmitterEnhancedApi> GetEnhancedApi() const;

private:
    std::shared_ptr<EmitterEnhancedApi> enhancedApi_;
    std::atomic<bool> isInit_{false};
};

EmitterEnhancedApiRegister& GetEmitterEnhancedApiRegister();
}
}
#endif  // BASE_EVENTHANDLER_FRAMEWORKS_INTEROPS_H
