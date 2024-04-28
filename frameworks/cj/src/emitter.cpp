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
#include <vector>
#include <map>
#include <memory>

#include "emitter.h"
#include "emitter_log.h"
#include "inner_event.h"
#include "event_handler_impl.h"
#include "cj_fn_invoker.h"

using InnerEvent = OHOS::AppExecFwk::InnerEvent;
using Priority = OHOS::AppExecFwk::EventQueue::Priority;

namespace OHOS::EventsEmitter {
    const int32_t SUCCESS = 0;
    struct EventDataWorker {
        CEventData data;
        InnerEvent::EventId eventId;
        CallbackInfo* callbackInfo;
    };

    static std::mutex emitterInsMutex;
    static std::map<InnerEvent::EventId, std::vector<CallbackInfo *>> emitterImpls;
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
        std::lock_guard<std::mutex> lock(emitterInsMutex);
        auto subscribe = emitterImpls.find(eventId);
        if (subscribe == emitterImpls.end()) {
            LOGW("emit has no callback");
            return false;
        }
        std::vector<CallbackInfo *> callbackInfo = subscribe->second;
        size_t callbackSize = callbackInfo.size();
        for (size_t i = 0; i < callbackSize; i++) {
            if (!callbackInfo[i]->isDeleted) {
                return true;
            }
        }
        return false;
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

    CallbackInfo *SearchCallbackInfo(const InnerEvent::EventId &eventIdValue, CallbackImpl *callback)
    {
        auto subscribe = emitterImpls.find(eventIdValue);
        if (subscribe == emitterImpls.end()) {
            return nullptr;
        }
        for (auto callbackInfo : subscribe->second) {
            if (callbackInfo->isDeleted) {
                continue;
            }
            if (callbackInfo->callbackImpl->name == callback->name) {
                LOGD("Callback found.")
                return callbackInfo;
            }
        }
        LOGD("Callback not found.")
        return nullptr;
    }

    void UpdateOnceFlag(CallbackInfo *callbackInfo, bool once)
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

    int32_t OnOrOnce(InnerEvent::EventId eventId, CallbackImpl *callback, bool once)
    {
        OutPutEventIdLog(eventId);
        std::lock_guard<std::mutex> lock(emitterInsMutex);
        auto callbackInfo = SearchCallbackInfo(eventId, callback);
        if (callbackInfo != nullptr) {
            UpdateOnceFlag(callbackInfo, once);
            return SUCCESS;
        }
        callbackInfo = new (std::nothrow) CallbackInfo();
        if (!callbackInfo) {
            LOGE("new callbackInfo failed");
            return MEMORY_ERROR;
        }
        callbackInfo->callbackImpl = callback;
        callbackInfo->once = once;
        emitterImpls[eventId].push_back(callbackInfo);
        return SUCCESS;
    }

    int32_t Emitter::On(uint32_t eventId, CallbackImpl *callback)
    {
        InnerEvent::EventId id = eventId;
        return OnOrOnce(id, callback, false);
    }

    int32_t Emitter::On(char* eventId, CallbackImpl *callback)
    {
        InnerEvent::EventId id = std::string(eventId);
        return OnOrOnce(id, callback, false);
    }

    int32_t Emitter::Once(uint32_t eventId, CallbackImpl *callback)
    {
        InnerEvent::EventId id = eventId;
        return OnOrOnce(id, callback, true);
    }

    int32_t Emitter::Once(char* eventId, CallbackImpl *callback)
    {
        InnerEvent::EventId id = std::string(eventId);
        return OnOrOnce(id, callback, true);
    }

    void Unsubscribe(InnerEvent::EventId eventId)
    {
        std::lock_guard<std::mutex> lock(emitterInsMutex);
        auto subscribe = emitterImpls.find(eventId);
        if (subscribe != emitterImpls.end()) {
            for (auto callbackInfo : subscribe->second) {
                callbackInfo->isDeleted = true;
            }
        }
    }

    void Unsubscribe(InnerEvent::EventId eventId, CallbackImpl *callback)
    {
        std::lock_guard<std::mutex> lock(emitterInsMutex);
        auto callbackInfo = SearchCallbackInfo(eventId, callback);
        if (callbackInfo != nullptr) {
            callbackInfo->isDeleted = true;
        }
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

    void Emitter::Off(uint32_t eventId, CallbackImpl *callback)
    {
        InnerEvent::EventId id = eventId;
        Unsubscribe(id, callback);
    }

    void Emitter::Off(char* eventId, CallbackImpl *callback)
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
        uint32_t count = 0;
        std::lock_guard<std::mutex> lock(emitterInsMutex);
        auto subscribe = emitterImpls.find(eventId);
        if (subscribe != emitterImpls.end()) {
            for (auto callbackInfo : subscribe->second) {
                if (!callbackInfo->isDeleted) {
                    ++count;
                }
            }
        }
        return count;
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

    void EventHandlerImpl::ProcessEvent([[maybe_unused]] const InnerEvent::Pointer& event)
    {
        LOGI("ProcessEvent");
        InnerEvent::EventId eventId = event->GetInnerEventIdEx();
        OutPutEventIdLog(eventId);
        std::vector<CallbackInfo *> callbackInfos;
        {
            std::lock_guard<std::mutex> lock(emitterInsMutex);
            auto subscribe = emitterImpls.find(eventId);
            if (subscribe == emitterImpls.end()) {
                LOGW("ProcessEvent has no callback");
                return;
            }
            callbackInfos = subscribe->second;
        }

        LOGD("size = %{public}zu", callbackInfos.size());
        auto value = event->GetUniqueObject<CEventData>();
        CEventData eventData = { .parameters = nullptr, .size = 0};
        if (value != nullptr) {
            eventData = *value;
        }
        for (auto iter = callbackInfos.begin(); iter != callbackInfos.end();) {
            CallbackInfo* callbackInfo = *iter;
            if (callbackInfo->once || callbackInfo->isDeleted) {
                LOGI("once callback or isDeleted callback");
                iter = callbackInfos.erase(iter);
                if (callbackInfo->processed) {
                    delete callbackInfo->callbackImpl;
                    delete callbackInfo;
                    callbackInfo = nullptr;
                    continue;
                }
            } else {
                ++iter;
            }
            if (!callbackInfo->isDeleted) {
                callbackInfo->callbackImpl->callback(eventData);
            } else {
                LOGD("callback is deleted.");
            }
            callbackInfo->processed = true;
        }
        if (callbackInfos.empty()) {
            emitterImpls.erase(eventId);
            LOGD("ProcessEvent delete the last callback");
        }
    }
}