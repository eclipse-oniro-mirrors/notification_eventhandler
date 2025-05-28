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

#include "aync_callback_manager.h"
#include "event_logger.h"

namespace OHOS {
namespace AppExecFwk {
AsyncCallbackManager& AsyncCallbackManager::GetInstance()
{
    static AsyncCallbackManager instance;
    return instance;
}

void AsyncCallbackManager::DeleteCallbackInfoByEventId(const InnerEvent::EventId &eventIdValue)
{
    aniAsyncCallbackManager_.AniDeleteCallbackInfoByEventId(eventIdValue);
    napiAsyncCallbackManager_.NapiDeleteCallbackInfoByEventId(eventIdValue);
}

uint32_t AsyncCallbackManager::GetListenerCountByEventId(const InnerEvent::EventId &eventId)
{
    uint32_t cnt = 0u;
    cnt += aniAsyncCallbackManager_.AniGetListenerCountByEventId(eventId);
    cnt += napiAsyncCallbackManager_.NapiGetListenerCountByEventId(eventId);
    return cnt;
}

bool AsyncCallbackManager::IsExistValidCallback(const InnerEvent::EventId &eventId)
{
    return napiAsyncCallbackManager_.NapiIsExistValidCallback(eventId) ||
        aniAsyncCallbackManager_.AniIsExistValidCallback(eventId);
}

napi_value AsyncCallbackManager::InsertCallbackInfo(
    napi_env env, const InnerEvent::EventId &eventIdValue, napi_value argv, bool once)
{
    return napiAsyncCallbackManager_.NapiInsertCallbackInfo(env, eventIdValue, argv, once);
}

void AsyncCallbackManager::InsertCallbackInfo(
    ani_env *env, InnerEvent::EventId eventId, bool once, ani_ref callback, ani_string dataType)
{
    aniAsyncCallbackManager_.AniInsertCallbackInfo(env, eventId, once, callback, dataType);
}

void AsyncCallbackManager::DeleteCallbackInfo(
    napi_env env, const InnerEvent::EventId &eventIdValue, napi_value argv)
{
    napiAsyncCallbackManager_.NapiDeleteCallbackInfo(env, eventIdValue, argv);
}

void AsyncCallbackManager::DeleteCallbackInfo(
    ani_env *env, const InnerEvent::EventId &eventIdValue, ani_ref callback)
{
    aniAsyncCallbackManager_.AniDeleteCallbackInfo(env, eventIdValue, callback);
}

void AsyncCallbackManager::DoCallback(const InnerEvent::Pointer& event)
{
    napiAsyncCallbackManager_.NapiDoCallback(event);
    aniAsyncCallbackManager_.AniDoCallback(event);
}

bool AsyncCallbackManager::IsCrossRuntime(const InnerEvent::EventId &eventId, EnvType envType)
{
    if (envType == EnvType::NAPI) {
        return aniAsyncCallbackManager_.AniIsExistValidCallback(eventId);
    }

    if (envType == EnvType::ANI) {
        return napiAsyncCallbackManager_.NapiIsExistValidCallback(eventId);
    }
    return false;
}
}
}