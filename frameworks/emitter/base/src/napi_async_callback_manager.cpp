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

void NapiAsyncCallbackManager::NapiDeleteCallbackInfoByEventId(const CompositeEventId &compositeId)
{
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto iter = napiAsyncCallbackContainer_.find(compositeId);
    if (iter != napiAsyncCallbackContainer_.end()) {
        for (auto callbackInfo : iter->second) {
            callbackInfo->isDeleted = true;
        }
    }
    napiAsyncCallbackContainer_.erase(compositeId);
}

uint32_t NapiAsyncCallbackManager::NapiGetListenerCountByEventId(const CompositeEventId &compositeId)
{
    uint32_t cnt = 0u;
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto subscribe = napiAsyncCallbackContainer_.find(compositeId);
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

bool NapiAsyncCallbackManager::NapiIsExistValidCallback(const CompositeEventId &compositeId)
{
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto subscribe = napiAsyncCallbackContainer_.find(compositeId);
    if (subscribe == napiAsyncCallbackContainer_.end()) {
        return false;
    }
    if (subscribe->second.size() != 0) {
        return true;
    }
    return false;
}

void NapiAsyncCallbackManager::NapiDeleteCallbackInfo(
    napi_env env, const CompositeEventId &compositeId, napi_value argv)
{
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto iter = napiAsyncCallbackContainer_.find(compositeId);
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
    const CompositeEventId &compositeId)
{
    std::lock_guard<std::mutex> lock(napiAsyncCallbackContainerMutex_);
    auto iter = napiAsyncCallbackContainer_.find(compositeId);
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
    CompositeEventId compositeId;
    compositeId.eventId = event->GetInnerEventIdEx();
    compositeId.emitterId = event->GetEmitterId();
    auto napiCallbackInfos = NapiGetAsyncCallbackInfo(compositeId);
    auto serializeData = event->GetSharedObject<SerializeData>();
    if (!serializeData) {
        HILOGE("Get data failed");
        return;
    }
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
        eventDataWorker->callbackInfo = *it;
        eventDataWorker->IsEnhanced = true;
        eventDataWorker->serializePtr = serializeData;
        serializeData->env = (*it)->env;
        if (serializeData->envType == EnvType::NAPI) {
            if (std::holds_alternative<napi_value>(serializeData->peerData)) {
                eventDataWorker->enhancedData = std::get<napi_value>(serializeData->peerData);
            }
            eventDataWorker->isCrossRuntime = false;
        } else {
            eventDataWorker->enhancedData = serializeData->crossData;
            eventDataWorker->isCrossRuntime = true;
        }
        napi_acquire_threadsafe_function((*it)->tsfn);
        napi_call_threadsafe_function((*it)->tsfn, eventDataWorker, napi_tsfn_nonblocking);
        napi_release_threadsafe_function((*it)->tsfn, napi_tsfn_release);
    }
}
} // namespace AppExecFwk
} // namespace OHOS
