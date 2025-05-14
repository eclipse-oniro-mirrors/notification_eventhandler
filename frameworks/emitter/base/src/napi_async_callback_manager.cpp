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

#include "napi_async_callback_manager.h"
#include <uv.h>
#include "event_logger.h"
#include "serialize.h"
#include "napi_deserialize.h"

namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
}

NapiAsyncCallbackInfo::~NapiAsyncCallbackInfo()
{
    env = nullptr;
}

void NapiAsyncCallbackInfo::ProcessEvent([[maybe_unused]] const InnerEvent::Pointer& event)
{
    if (env == nullptr || isDeleted) {
        HILOGE("env is release");
        return;
    }
    EventDataWorker* eventDataWorker = new (std::nothrow) EventDataWorker();
    if (!eventDataWorker) {
        HILOGE("new object failed");
        return;
    }

    eventDataWorker->serializeData = event->GetSharedObject<SerializeData>();
    eventDataWorker->callbackInfo = shared_from_this();
    napi_acquire_threadsafe_function(tsfn);
    napi_call_threadsafe_function(tsfn, eventDataWorker, napi_tsfn_nonblocking);
    napi_release_threadsafe_function(tsfn, napi_tsfn_release);
}

void NapiAsyncCallbackManager::NapiDeleteCallbackInfoByEventId(const InnerEvent::EventId &eventIdValue)
{
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto iter = napiAsyncCallbackContainer_.find(eventIdValue);
    if (iter != napiAsyncCallbackContainer_.end()) {
        for (auto callbackInfo : iter->second) {
            callbackInfo->isDeleted = true;
        }
    }
    napiAsyncCallbackContainer_.erase(eventIdValue);
}

uint32_t NapiAsyncCallbackManager::NapiGetListenerCountByEventId(const InnerEvent::EventId &eventId)
{
    uint32_t cnt = 0u;
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto subscribe = napiAsyncCallbackContainer_.find(eventId);
    if (subscribe != napiAsyncCallbackContainer_.end()) {
        for (auto it = subscribe->second.begin(); it != subscribe->second.end();) {
            if ((*it)->isDeleted == true || (*it)->env == nullptr) {
                it = subscribe->second.erase(it);
                continue;
            }
            ++it;
            ++cnt;
        }
    }
    return cnt;
}

bool NapiAsyncCallbackManager::NapiIsExistValidCallback(const InnerEvent::EventId &eventId)
{
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto subscribe = napiAsyncCallbackContainer_.find(eventId);
    if (subscribe == napiAsyncCallbackContainer_.end()) {
        HILOGI("JS_Emit has no callback");
        return false;
    }
    if (subscribe->second.size() != 0) {
        return true;
    }
    return false;
}

napi_value NapiAsyncCallbackManager::NapiInsertCallbackInfo(
    napi_env env, const InnerEvent::EventId &eventIdValue, napi_value argv1, bool once)
{
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto subscriber = napiAsyncCallbackContainer_.find(eventIdValue);
    if (subscriber != napiAsyncCallbackContainer_.end()) {
        for (auto callbackInfo : subscriber->second) {
            napi_value callback = nullptr;
            if (callbackInfo->isDeleted) {
                continue;
            }
            if (callbackInfo->env != env) {
                continue;
            }
            napi_get_reference_value(callbackInfo->env, callbackInfo->callback, &callback);
            bool isEq = false;
            napi_strict_equals(env, argv1, callback, &isEq);
            if (!isEq) {
                continue;
            }
            callbackInfo->once = once;
            return nullptr;
        }
    }
    auto callbackInfoPtr = new (std::nothrow) NapiAsyncCallbackInfo();
    if (!callbackInfoPtr) {
        HILOGE("new object failed");
        return nullptr;
    }
    std::shared_ptr<NapiAsyncCallbackInfo> callbackInfo(callbackInfoPtr, [](NapiAsyncCallbackInfo* callbackInfo) {
        NapiReleaseCallbackInfo(callbackInfo);
    });
    callbackInfo->env = env;
    callbackInfo->once = once;
    callbackInfo->eventId = eventIdValue;
    napi_create_reference(env, argv1, 1, &callbackInfo->callback);
    napi_wrap(env, argv1, new (std::nothrow) std::weak_ptr<NapiAsyncCallbackInfo>(callbackInfo),
        [](napi_env env, void* data, void* hint) {
        auto callbackInfoPtr = static_cast<std::weak_ptr<NapiAsyncCallbackInfo>*>(data);
        if (callbackInfoPtr != nullptr && (*callbackInfoPtr).lock() != nullptr) {
            (*callbackInfoPtr).lock()->isDeleted = true;
            (*callbackInfoPtr).lock()->env = nullptr;
        }
    }, nullptr, nullptr);
    napi_value resourceName = nullptr;
    napi_create_string_utf8(env, "Call thread-safe function", NAPI_AUTO_LENGTH, &resourceName);
    napi_create_threadsafe_function(env, argv1, nullptr, resourceName, 0, 1, nullptr,
        NapiThreadFinished, nullptr, NapiThreadSafeCallback, &(callbackInfo->tsfn));
    napiAsyncCallbackContainer_[eventIdValue].insert(callbackInfo);
    return nullptr;
}

void NapiAsyncCallbackManager::NapiDeleteCallbackInfo(
    napi_env env, const InnerEvent::EventId &eventIdValue, napi_value argv)
{
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto iter = napiAsyncCallbackContainer_.find(eventIdValue);
    if (iter == napiAsyncCallbackContainer_.end()) {
        return;
    }
    for (auto callbackInfo = iter->second.begin(); callbackInfo != iter->second.end();) {
        napi_value callback = nullptr;
        if ((*callbackInfo)->env != env) {
            ++callbackInfo;
            continue;
        }
        napi_get_reference_value((*callbackInfo)->env, (*callbackInfo)->callback, &callback);
        bool isEq = false;
        napi_strict_equals(env, argv, callback, &isEq);
        if (!isEq) {
            ++callbackInfo;
            continue;
        }
        (*callbackInfo)->isDeleted = true;
        callbackInfo = iter->second.erase(callbackInfo);
        return;
    }
    return;
}

std::unordered_set<std::shared_ptr<NapiAsyncCallbackInfo>> NapiAsyncCallbackManager::NapiGetAsyncCallbackInfo(
    const InnerEvent::EventId &eventId)
{
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto iter = napiAsyncCallbackContainer_.find(eventId);
    if (iter == napiAsyncCallbackContainer_.end()) {
        HILOGW("ProcessEvent has no callback");
        std::unordered_set<std::shared_ptr<NapiAsyncCallbackInfo>> result;
        return result;
    }
    for (auto it = iter->second.begin(); it != iter->second.end();) {
        if ((*it)->isDeleted == true || (*it)->env == nullptr) {
            it = iter->second.erase(it);
            continue;
        }
        ++it;
    }
    return iter->second;
}

void NapiAsyncCallbackManager::NapiDoCallback(const InnerEvent::Pointer& event)
{
    auto napiCallbackInfos = NapiGetAsyncCallbackInfo(event->GetInnerEventIdEx());
    for (auto it = napiCallbackInfos.begin(); it != napiCallbackInfos.end(); ++it) {
        if (*it == nullptr) {
            HILOGE("napiCallbackInfo is empty");
            continue;
        }
        (*it)->ProcessEvent(event);
    }
}

void NapiAsyncCallbackManager::NapiReleaseCallbackInfo(NapiAsyncCallbackInfo* callbackInfo)
{
    if (callbackInfo != nullptr) {
        uv_loop_s *loop = nullptr;
        if (napi_get_uv_event_loop(callbackInfo->env, &loop) != napi_ok) {
            delete callbackInfo;
            callbackInfo = nullptr;
            return;
        }
        uv_work_t *work = new (std::nothrow) uv_work_t;
        if (work == nullptr) {
            delete callbackInfo;
            callbackInfo = nullptr;
            return;
        }
        work->data = reinterpret_cast<void*>(callbackInfo);
        auto ret = uv_queue_work_with_qos(loop, work, [](uv_work_t* work) {},
        [](uv_work_t *work, int status) {
            NapiAsyncCallbackInfo* callbackInfo = reinterpret_cast<NapiAsyncCallbackInfo*>(work->data);
            if (napi_delete_reference(callbackInfo->env, callbackInfo->callback) != napi_ok) {
                HILOGE("napi_delete_reference fail");
            }
            napi_release_threadsafe_function(callbackInfo->tsfn, napi_tsfn_release);
            delete callbackInfo;
            callbackInfo = nullptr;
            delete work;
            work = nullptr;
        }, uv_qos_user_initiated);
        if (ret != napi_ok)  {
            delete callbackInfo;
            callbackInfo = nullptr;
            delete work;
            work = nullptr;
        }
    }
}

void NapiAsyncCallbackManager::NapiThreadFinished(napi_env env, void* data, [[maybe_unused]] void* context)
{
    HILOGD("ThreadFinished");
}

void NapiAsyncCallbackManager::NapiThreadSafeCallback(napi_env env, napi_value jsCallback, void* context, void* data)
{
    napi_handle_scope scope;
    EventDataWorker* eventDataInner = static_cast<EventDataWorker*>(data);
    if (eventDataInner != nullptr) {
        auto callbackInfoInner = eventDataInner->callbackInfo;
        if (callbackInfoInner && !(callbackInfoInner->isDeleted)) {
            napi_open_handle_scope(callbackInfoInner->env, &scope);
            if (scope == nullptr) {
                HILOGD("Scope is null");
                return;
            }
            NapiProcessCallback(eventDataInner);
            napi_close_handle_scope(callbackInfoInner->env, scope);
        }
    }
    delete eventDataInner;
    eventDataInner = nullptr;
    data = nullptr;
}

void NapiAsyncCallbackManager::NapiProcessCallback(const EventDataWorker* eventDataInner)
{
    std::shared_ptr<NapiAsyncCallbackInfo> callbackInner = eventDataInner->callbackInfo;
    napi_value resultData = nullptr;
    auto serializeData = eventDataInner->serializeData;
    bool isDeserializeSuccess = true;
    if (serializeData->envType == EnvType::NAPI) {
        isDeserializeSuccess = NapiDeserialize::PeerDeserialize(callbackInner->env, &resultData, serializeData);
    } else {
        isDeserializeSuccess = NapiDeserialize::CrossDeserialize(callbackInner->env, &resultData, serializeData);
    }
    if (!isDeserializeSuccess) {
        return;
    }
    napi_value event = nullptr;
    napi_create_object(callbackInner->env, &event);
    napi_set_named_property(callbackInner->env, event, "data", resultData);
    napi_value callback = nullptr;
    napi_value returnVal = nullptr;
    napi_get_reference_value(callbackInner->env, callbackInner->callback, &callback);
    napi_call_function(callbackInner->env, nullptr, callback, 1, &event, &returnVal);
    if (callbackInner->once) {
        HILOGD("ProcessEvent delete once");
        callbackInner->isDeleted = true;
    }
}

} // namespace AppExecFwk
} // namespace OHOS