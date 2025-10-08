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
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
}
AsyncCallbackManager& AsyncCallbackManager::GetInstance()
{
    static AsyncCallbackManager instance;
    return instance;
}

void AsyncCallbackManager::DeleteCallbackInfoByEventId(const CompositeEventId &compositeId)
{
    aniAsyncCallbackManager_.AniDeleteCallbackInfoByEventId(compositeId);
    napiAsyncCallbackManager_.NapiDeleteCallbackInfoByEventId(compositeId);
}

uint32_t AsyncCallbackManager::GetListenerCountByEventId(const CompositeEventId &compositeId)
{
    uint32_t cnt = 0u;
    cnt += aniAsyncCallbackManager_.AniGetListenerCountByEventId(compositeId);
    cnt += napiAsyncCallbackManager_.NapiGetListenerCountByEventId(compositeId);
    return cnt;
}

bool AsyncCallbackManager::IsExistValidCallback(const CompositeEventId &compositeId)
{
    auto ret = napiAsyncCallbackManager_.NapiIsExistValidCallback(compositeId) ||
        aniAsyncCallbackManager_.AniIsExistValidCallback(compositeId);
    if (!ret) {
        if (compositeId.eventId.index() == OHOS::AppExecFwk::TYPE_U32_INDEX) {
            HILOGE("Event id: %{public}u has no callback", std::get<uint32_t>(compositeId.eventId));
        } else {
            HILOGE("Event id: %{public}s has no callback", std::get<std::string>(compositeId.eventId).c_str());
        }
    }
    return ret;
}

void AsyncCallbackManager::InsertCallbackInfo(
    ani_env *env, CompositeEventId compositeId, bool once, ani_ref callback, ani_string dataType)
{
    aniAsyncCallbackManager_.AniInsertCallbackInfo(env, compositeId, once, callback, dataType);
}

void AsyncCallbackManager::DeleteCallbackInfo(
    napi_env env, const CompositeEventId &compositeId, napi_value argv)
{
    napiAsyncCallbackManager_.NapiDeleteCallbackInfo(env, compositeId, argv);
}

void AsyncCallbackManager::DeleteCallbackInfo(
    ani_env *env, const CompositeEventId &compositeId, ani_ref callback)
{
    aniAsyncCallbackManager_.AniDeleteCallbackInfo(env, compositeId, callback);
}

void AsyncCallbackManager::DoCallback(const InnerEvent::Pointer& event)
{
    napiAsyncCallbackManager_.NapiDoCallback(event);
    aniAsyncCallbackManager_.AniDoCallback(event);
}

bool AsyncCallbackManager::IsCrossRuntime(const CompositeEventId &compositeId, EnvType envType)
{
    if (envType == EnvType::NAPI) {
        return aniAsyncCallbackManager_.AniIsExistValidCallback(compositeId);
    }

    if (envType == EnvType::ANI) {
        return napiAsyncCallbackManager_.NapiIsExistValidCallback(compositeId);
    }
    return false;
}

void AsyncCallbackManager::CleanCallbackInfo(const CompositeEventId &compositeId)
{
    aniAsyncCallbackManager_.AniCleanCallbackInfo(compositeId);
}
}
}
