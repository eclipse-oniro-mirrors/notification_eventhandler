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

#include "sts_events_emitter.h"

#include <iostream>
#include <array>
#include <mutex>
#include <map>
#include <unordered_set>
#include "event_logger.h"

namespace OHOS {
namespace AppExecFwk {

DEFINE_EH_HILOG_LABEL("EventsEmitter");
static std::mutex g_eventsEmitterInsMutex;
static std::map<InnerEvent::EventId, std::unordered_set<std::shared_ptr<AniAsyncCallbackInfo>>> eventsEmitterInstances;

AniAsyncCallbackInfo::~AniAsyncCallbackInfo()
{
    env = nullptr;
}

std::string EventsEmitter::GetStdString(ani_env *env, ani_string str)
{
    std::string result;
    ani_size sz {};
    env->String_GetUTF8Size(str, &sz);
    result.resize(sz + 1);
    env->String_GetUTF8SubString(str, 0, sz, result.data(), result.size(), &sz);
    result.resize(sz);
    return result;
}

std::shared_ptr<AniAsyncCallbackInfo> EventsEmitter::SearchCallbackInfo(
    ani_env *env, const InnerEvent::EventId &eventIdValue, ani_ref callback)
{
    auto subscribe = eventsEmitterInstances.find(eventIdValue);
    if (subscribe == eventsEmitterInstances.end()) {
        return nullptr;
    }
    for (auto callbackInfo : subscribe->second) {
        if (callbackInfo->isDeleted) {
            continue;
        }
        if (callbackInfo->env != env) {
            continue;
        }
        ani_boolean isEq = false;
        env->Reference_StrictEquals(callback, callbackInfo->callback, &isEq);
        if (!isEq) {
            continue;
        }
        return callbackInfo;
    }
    return nullptr;
}

void EventsEmitter::ReleaseCallbackInfo(ani_env *env, AniAsyncCallbackInfo* callbackInfo)
{
    if (callbackInfo != nullptr) {
        // TODO
    }
}

void EventsEmitter::UpdateOnceFlag(std::shared_ptr<AniAsyncCallbackInfo>callbackInfo, bool once)
{
    if (!once) {
        if (callbackInfo->once) {
            HILOGD("JS_On change once to on");
            callbackInfo->once = false;
        } else {
            HILOGD("JS_On already on");
        }
    } else {
        if (callbackInfo->once) {
            HILOGD("JS_Once already once");
        } else {
            HILOGD("JS_Once change on to once");
            callbackInfo->once = true;
        }
    }
}

void EventsEmitter::DeleteCallbackInfo(ani_env *env, const InnerEvent::EventId &eventIdValue, ani_ref callback)
{
    std::lock_guard<std::mutex> lock(g_eventsEmitterInsMutex);
    auto iter = eventsEmitterInstances.find(eventIdValue);
    if (iter == eventsEmitterInstances.end()) {
        return;
    }
    for (auto callbackInfo = iter->second.begin(); callbackInfo != iter->second.end();) {
        if ((*callbackInfo)->env != env) {
            ++callbackInfo;
            continue;
        }
        ani_boolean isEq = false;
        env->Reference_StrictEquals(callback, (*callbackInfo)->callback, &isEq);
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

void EventsEmitter::offEmitterInstances(InnerEvent::EventId eventIdValue)
{
    HILOGI("offeventsEmitterInstances begin");
    std::lock_guard<std::mutex> lock(g_eventsEmitterInsMutex);
    auto subscribe = eventsEmitterInstances.find(eventIdValue);
    if (subscribe != eventsEmitterInstances.end()) {
        HILOGI("offeventsEmitterInstances_find");
        for (auto callbackInfo : subscribe->second) {
            callbackInfo->isDeleted = true;
        }
    }
    eventsEmitterInstances.erase(eventIdValue);
}

void EventsEmitter::AniWrap(ani_env *env, ani_ref callback)
{
    static const char *nameSpace = "L@ohos/events/emitter/emitter;";
    ani_namespace cls;
    if (ANI_OK != env->FindNamespace(nameSpace, &cls)) {
        HILOGE("Not found '%{public}s'", nameSpace);
        return;
    }
}

void EventsEmitter::onOrOnce(ani_env *env, InnerEvent::EventId eventId, bool once, ani_ref callback)
{
    HILOGI("onOrOncebegin\n");
    std::lock_guard<std::mutex> lock(g_eventsEmitterInsMutex);
    auto callbackInfo = SearchCallbackInfo(env, eventId, callback);
    if (callbackInfo != nullptr) {
        UpdateOnceFlag(callbackInfo, once);
    } else {
        callbackInfo = std::shared_ptr<AniAsyncCallbackInfo>(new (std::nothrow) AniAsyncCallbackInfo(),
            [env](AniAsyncCallbackInfo* callbackInfo) {
                ReleaseCallbackInfo(env, callbackInfo);
        });
        if (!callbackInfo) {
            HILOGE("new object failed");
            return;
        }
        callbackInfo->env = env;
        callbackInfo->once = once;
        callbackInfo->eventId = eventId;
        env->GlobalReference_Create(callback, &callbackInfo->callback);
        AniWrap(env, callback);
        eventsEmitterInstances[eventId].insert(callbackInfo);
        HILOGI("onOrOnceEnd\n");
    }
}

static void OnOrOnceSync(ani_env *env, ani_double eventId, ani_boolean once, ani_ref callback)
{
    HILOGI("OnOrOnceSync begin");
    InnerEvent::EventId id = (uint32_t)eventId;
    EventsEmitter::onOrOnce(env, id, once, callback);
}

static void OnOrOnceStringSync(ani_env *env, ani_string eventId, ani_boolean once, ani_ref callback)
{
    HILOGI("OnOrOnceStringSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::onOrOnce(env, id, once, callback);
}

static void OnOrOnceGenericEventSync(
    ani_env *env, ani_string eventId, ani_boolean once, ani_ref callback)
{
    HILOGI("OnOrOnceGenericEventSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::onOrOnce(env, id, once, callback);
}

static void OffStringSync(ani_env *env, ani_string eventId, ani_ref callback)
{
    HILOGI("OffStringSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::DeleteCallbackInfo(env, id, callback);
    EventsEmitter::offEmitterInstances(id);
}

static void OffGenericEventSync(ani_env *env, ani_string eventId, ani_ref callback)
{
    HILOGI("OffGenericEventSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::DeleteCallbackInfo(env, id, callback);
    EventsEmitter::offEmitterInstances(id);
}

extern "C" {
ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    HILOGI("ANI_Constructor begin");
    ani_status status = ANI_ERROR;
    ani_env *env;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        HILOGE("Unsupported ANI_VERSION_1.");
        return ANI_ERROR;
    }

    ani_namespace kitNs;
    status = env->FindNamespace("L@ohos/events/emitter/emitter;", &kitNs);
    if (status != ANI_OK) {
        HILOGE("Not found ani_namespace L@ohos/events/emitter/emitter");
        return ANI_INVALID_ARGS;
    }

    std::array methods = {
        ani_native_function{"OnOrOnceSync", nullptr, reinterpret_cast<void *>(OnOrOnceSync)},
        ani_native_function{"OnOrOnceStringSync", nullptr, reinterpret_cast<void *>(OnOrOnceStringSync)},
        ani_native_function{"OnOrOnceGenericEventSync", nullptr, reinterpret_cast<void *>(OnOrOnceGenericEventSync)},
        ani_native_function{"OffStringSync", nullptr, reinterpret_cast<void *>(OffStringSync)},
        ani_native_function{"OffGenericEventSync", nullptr, reinterpret_cast<void *>(OffGenericEventSync)},
    };
    
    status = env->Namespace_BindNativeFunctions(kitNs, methods.data(), methods.size());
    if (status != ANI_OK) {
        HILOGE("Cannot bind native methods to L@ohos/events/emitter/emitter");
        return ANI_INVALID_TYPE;
    }

    *result = ANI_VERSION_1;
    return ANI_OK;
}
}

}  // namespace AppExecFwk
}  // namespace OHOS