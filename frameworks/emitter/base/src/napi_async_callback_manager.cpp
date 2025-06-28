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
namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
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
        return false;
    }
    if (subscribe->second.size() != 0) {
        return true;
    }
    return false;
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

std::unordered_set<std::shared_ptr<AsyncCallbackInfo>> NapiAsyncCallbackManager::NapiGetAsyncCallbackInfo(
    const InnerEvent::EventId &eventId)
{
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto iter = napiAsyncCallbackContainer_.find(eventId);
    if (iter == napiAsyncCallbackContainer_.end()) {
        std::unordered_set<std::shared_ptr<AsyncCallbackInfo>> result;
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
    auto serializeData = event->GetSharedObject<SerializeData>();
    auto deleter = [](napi_value* data) {
        if (data != nullptr) {
            delete data;
        }
    };
    for (auto it = napiCallbackInfos.begin(); it != napiCallbackInfos.end(); ++it) {
        if (*it == nullptr || (*it)->env == nullptr || (*it)->isDeleted) {
            HILOGE("napiCallbackInfo is unavailable");
            continue;
        }
        EventDataWorker* eventDataWorker = new (std::nothrow) EventDataWorker();
        if (!eventDataWorker) {
            HILOGE("new object failed");
            return;
        }
        if (serializeData->envType == EnvType::NAPI) {
            auto napiValue = std::get<napi_value>(serializeData->peerData);

            auto* heapValue = new napi_value(napiValue);
            eventDataWorker->data = std::shared_ptr<napi_value>(heapValue, deleter);

            eventDataWorker->isCrossRuntime = false;
            eventDataWorker->callbackInfo = *it;
        } else {
            eventDataWorker->crossRuntimeData = serializeData->crossData;
            eventDataWorker->isCrossRuntime = true;
            eventDataWorker->callbackInfo = *it;
        }
        napi_acquire_threadsafe_function((*it)->tsfn);
        napi_call_threadsafe_function((*it)->tsfn, eventDataWorker, napi_tsfn_nonblocking);
        napi_release_threadsafe_function((*it)->tsfn, napi_tsfn_release);
    }
}
} // namespace AppExecFwk
} // namespace OHOS