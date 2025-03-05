/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <mutex>
#include <unordered_set>
#include <map>
#include <memory>

#include "emitter_log.h"
#include "inner_event.h"
#include "event_handler_impl.h"
#include "cj_fn_invoker.h"
#include "emitter.h"

using InnerEvent = OHOS::AppExecFwk::InnerEvent;
using Priority = OHOS::AppExecFwk::EventQueue::Priority;

namespace OHOS::EventsEmitter {
    const int32_t SUCCESS = 0;

    static std::mutex g_emitterInsMutex;
    static std::map<InnerEvent::EventId, std::unordered_set<std::shared_ptr<CallbackInfo>>> g_emitterImpls;
    std::shared_ptr<EventHandlerImpl> eventHandler = EventHandlerImpl::GetEventHandler();

    CallbackImpl::CallbackImpl(std::string name, std::function<void(CEventData)> callback)
        : name(name), callback(callback)
    {}

    CallbackInfo::~CallbackInfo()
    {
        callbackImpl = nullptr;
    }

    bool IsExistValidCallback(const InnerEvent::EventId &eventId)
    {
        std::lock_guard<std::mutex> lock(g_emitterInsMutex);
        auto subscribe = g_emitterImpls.find(eventId);
        if (subscribe == g_emitterImpls.end()) {
            LOGW("emit has no callback");
            return false;
        }
        return true;
    }

    void EmitWithEventData(InnerEvent::EventId eventId, uint32_t priority, CEventData data)
    {
        if (!IsExistValidCallback(eventId)) {
            LOGE("Invalid callback");
            return;
        }
        std::unique_ptr<CEventData> dataPtr;
        if (data.size == 0) {
            dataPtr = std::make_unique<CEventData>();
        } else {
            dataPtr = std::make_unique<CEventData>(data);
        }
        auto event = InnerEvent::Get(eventId, dataPtr);
        eventHandler->SendEvent(event, 0, static_cast<Priority>(priority));
    }

    std::shared_ptr<CallbackInfo> SearchCallbackInfo(const InnerEvent::EventId &eventId,
        const std::string &callbackName, bool erase)
    {
        auto subscribe = g_emitterImpls.find(eventId);
        if (subscribe == g_emitterImpls.end()) {
            return nullptr;
        }
        for (auto callbackInfo : subscribe->second) {
            if (callbackInfo->callbackImpl->name == callbackName) {
                LOGD("Callback found.")
                if (erase) {
                    subscribe->second.erase(callbackInfo);
                    break;
                }
                return callbackInfo;
            }
        }
        if (subscribe->second.empty()) {
            g_emitterImpls.erase(eventId);
        } else {
            LOGD("Callback not found.");
        }
        return nullptr;
    }

    void UpdateOnceFlag(std::shared_ptr<CallbackInfo> &callbackInfo, bool once)
    {
        if (!once) {
            if (callbackInfo->once) {
                LOGD("On change once to on");
                callbackInfo->once = false;
            } else {
                LOGD("On already on");
            }
        } else {
            if (callbackInfo->once) {
                LOGD("Once already once");
            } else {
                LOGD("Once change on to once");
                callbackInfo->once = true;
            }
        }
    }

    void OutPutEventIdLog(const InnerEvent::EventId &eventId)
    {
        if (eventId.index() == OHOS::AppExecFwk::TYPE_U32_INDEX) {
            LOGD("Event id value: %{public}u", std::get<uint32_t>(eventId));
        } else {
            LOGD("Event id value: %{public}s", std::get<std::string>(eventId).c_str());
        }
    }

    int32_t OnOrOnce(InnerEvent::EventId eventId, std::shared_ptr<CallbackImpl> &callbackImpl, bool once)
    {
        OutPutEventIdLog(eventId);
        std::lock_guard<std::mutex> lock(g_emitterInsMutex);
        auto callbackInfo = SearchCallbackInfo(eventId, callbackImpl->name, false);
        if (callbackInfo != nullptr) {
            UpdateOnceFlag(callbackInfo, once);
            return SUCCESS;
        }
        callbackInfo = std::make_shared<CallbackInfo>();
        if (!callbackInfo) {
            LOGE("new callbackInfo failed");
            return MEMORY_ERROR;
        }
        callbackInfo->callbackImpl = callbackImpl;
        callbackInfo->once = once;
        g_emitterImpls[eventId].insert(callbackInfo);
        return SUCCESS;
    }

    int32_t Emitter::On(uint32_t eventId, std::shared_ptr<CallbackImpl> &callback)
    {
        InnerEvent::EventId id = eventId;
        return OnOrOnce(id, callback, false);
    }

    int32_t Emitter::On(char* eventId, std::shared_ptr<CallbackImpl> &callback)
    {
        InnerEvent::EventId id = std::string(eventId);
        return OnOrOnce(id, callback, false);
    }

    int32_t Emitter::Once(uint32_t eventId, std::shared_ptr<CallbackImpl> &callback)
    {
        InnerEvent::EventId id = eventId;
        return OnOrOnce(id, callback, true);
    }

    int32_t Emitter::Once(char* eventId, std::shared_ptr<CallbackImpl> &callback)
    {
        InnerEvent::EventId id = std::string(eventId);
        return OnOrOnce(id, callback, true);
    }

    void Unsubscribe(InnerEvent::EventId eventId)
    {
        std::lock_guard<std::mutex> lock(g_emitterInsMutex);
        auto subscribe = g_emitterImpls.find(eventId);
        if (subscribe != g_emitterImpls.end()) {
            g_emitterImpls.erase(eventId);
        }
    }

    void Unsubscribe(InnerEvent::EventId eventId, std::shared_ptr<CallbackImpl> &callback)
    {
        std::lock_guard<std::mutex> lock(g_emitterInsMutex);
        (void)SearchCallbackInfo(eventId, callback->name, true);
    }

    void Emitter::Off(uint32_t eventId)
    {
        InnerEvent::EventId id = eventId;
        Unsubscribe(id);
    }

    void Emitter::Off(char* eventId)
    {
        InnerEvent::EventId id = std::string(eventId);
        Unsubscribe(id);
    }

    void Emitter::Off(uint32_t eventId, std::shared_ptr<CallbackImpl> &callback)
    {
        InnerEvent::EventId id = eventId;
        Unsubscribe(id, callback);
    }

    void Emitter::Off(char* eventId, std::shared_ptr<CallbackImpl> &callback)
    {
        InnerEvent::EventId id = std::string(eventId);
        Unsubscribe(id, callback);
    }

    void Emitter::Emit(uint32_t eventId, uint32_t priority, CEventData data)
    {
        InnerEvent::EventId id = eventId;
        EmitWithEventData(id, priority, data);
    }

    void Emitter::Emit(char* eventId, uint32_t priority, CEventData data)
    {
        InnerEvent::EventId id = std::string(eventId);
        EmitWithEventData(id, priority, data);
    }

    uint32_t GetListenerCountByEventId(InnerEvent::EventId eventId)
    {
        std::lock_guard<std::mutex> lock(g_emitterInsMutex);
        auto subscribe = g_emitterImpls.find(eventId);
        return static_cast<uint32_t>(subscribe->second.size());
    }

    uint32_t Emitter::GetListenerCount(uint32_t eventId)
    {
        InnerEvent::EventId id = eventId;
        return GetListenerCountByEventId(id);
    }

    uint32_t Emitter::GetListenerCount(std::string eventId)
    {
        InnerEvent::EventId id = eventId;
        return GetListenerCountByEventId(id);
    }

    void FreeCEventData(CEventData &eventData)
    {
        auto params = reinterpret_cast<CParameter *>(eventData.parameters);
        for (int i = 0; i < eventData.size; i++) {
            free(params[i].key);
            free(params[i].value);
            params[i].key = nullptr;
            params[i].value = nullptr;
        }
        free(params);
        params = nullptr;
    }

    void ProcessCallback(const InnerEvent::Pointer& event,
        std::unordered_set<std::shared_ptr<CallbackInfo>>& callbackInfos)
    {
        auto value = event->GetUniqueObject<CEventData>();
        CEventData eventData = { .parameters = nullptr, .size = 0};
        if (value != nullptr) {
            eventData = *value;
        }
        for (auto callback : callbackInfos) {
            callback->callbackImpl->callback(eventData);
        }
        FreeCEventData(eventData);
    }

    std::unordered_set<std::shared_ptr<CallbackInfo>> GetEventCallbacks(InnerEvent::EventId eventId)
    {
        std::lock_guard<std::mutex> lock(g_emitterInsMutex);
        std::unordered_set<std::shared_ptr<CallbackInfo>> callbackInfos;
        auto subscribe = g_emitterImpls.find(eventId);
        if (subscribe == g_emitterImpls.end()) {
            LOGW("no callback");
            return callbackInfos;
        }
        for (auto iter = subscribe->second.begin(); iter != subscribe->second.end();) {
            callbackInfos.insert(*iter);
            if ((*iter)->once) {
                iter = subscribe->second.erase(iter);
            } else {
                ++iter;
            }
        }
        if (subscribe->second.empty()) {
            LOGD("delete the last callback");
            g_emitterImpls.erase(eventId);
        }
        return callbackInfos;
    }

    void EventHandlerImpl::ProcessEvent(const InnerEvent::Pointer& event)
    {
        LOGI("ProcessEvent");
        InnerEvent::EventId eventId = event->GetInnerEventIdEx();
        OutPutEventIdLog(eventId);
        auto callbackInfos = GetEventCallbacks(eventId);
        if (callbackInfos.size() <= 0) {
            return;
        }
        LOGD("size = %{public}zu", callbackInfos.size());
        ProcessCallback(event, callbackInfos);
    }
}