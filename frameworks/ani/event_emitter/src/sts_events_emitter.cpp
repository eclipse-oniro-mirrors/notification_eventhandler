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

using namespace OHOS::AppExecFwk;

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
    HILOGD("offeventsEmitterInstances begin");
    std::lock_guard<std::mutex> lock(g_eventsEmitterInsMutex);
    auto subscribe = eventsEmitterInstances.find(eventIdValue);
    if (subscribe != eventsEmitterInstances.end()) {
        HILOGD("offeventsEmitterInstances_find");
        for (auto callbackInfo : subscribe->second) {
            callbackInfo->isDeleted = true;
        }
    }
    eventsEmitterInstances.erase(eventIdValue);
}

void EventsEmitter::AniWrap(ani_env *env, ani_ref callback)
{
    static const char *className = "L@ohos/events/emitter/EventsEmitter;";
    ani_class cls;
    if (ANI_OK != env->FindClass(className, &cls)) {
        HILOGE("Not found '%{public}s'", className);
        return;
    }
    ani_method ctor;
    if (ANI_OK != env->Class_FindMethod(cls, "<ctor>", "J:V", &ctor)) {
        HILOGE("Not found ctor");
        return;
    }
    ani_object emitter_object;
    if (ANI_OK !=env->Object_New(cls, ctor, &emitter_object, reinterpret_cast<ani_long>(callback))) {
        HILOGE("Failed to create object");
    }
}

void EventsEmitter::onOrOnce(ani_env *env, InnerEvent::EventId eventId, bool once, ani_ref callback)
{
    HILOGD("onOrOncebegin\n");
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
        HILOGD("onOrOnceEnd\n");
    }
}

static void OnOrOnceSync(ani_env *env, ani_object obj, ani_double eventId, ani_boolean once, ani_ref callback)
{
    HILOGD("OnOrOnceSync begin");
    InnerEvent::EventId id = (uint32_t)eventId;
    EventsEmitter::onOrOnce(env, id, once, callback);
}

static void OnOrOnceStringSync(ani_env *env, ani_object obj, ani_string eventId, ani_boolean once, ani_ref callback)
{
    HILOGD("OnOrOnceStringSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::onOrOnce(env, id, once, callback);
}

static void OnOrOnceGenericEventSync(
    ani_env *env, ani_object obj, ani_string eventId, ani_boolean once, ani_ref callback)
{
    HILOGD("OnOrOnceGenericEventSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::onOrOnce(env, id, once, callback);
}

static void OffStringSync(ani_env *env, ani_object obj, ani_string eventId, ani_boolean once, ani_ref callback)
{
    HILOGD("OffStringSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::DeleteCallbackInfo(env, id, callback);
    EventsEmitter::offEmitterInstances(id);
}

static void OffGenericEventSync(ani_env *env, ani_object obj, ani_string eventId, ani_boolean once, ani_ref callback)
{
    HILOGD("OffGenericEventSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::DeleteCallbackInfo(env, id, callback);
    EventsEmitter::offEmitterInstances(id);
}

ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    HILOGD("ANI_Constructor begin");
    ani_status status = ANI_ERROR;
    ani_env *env;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        HILOGE("Unsupported ANI_VERSION_1.");
        return ANI_ERROR;
    }

    ani_namespace kitNs;
    status = env->FindNamespace("L@ohos/event/ets/@ohos/events/emitter/emitter;", &kitNs);
    if (status != ANI_OK) {
        HILOGE("Not found ani_namespace L@ohos/event/ets/@ohos/events/emitter/emitter");
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
        HILOGE("Cannot bind native methods to L@ohos/event/ets/@ohos/events/emitter/emitter");
        return ANI_INVALID_TYPE;
    }

    *result = ANI_VERSION_1;
    return ANI_OK;
}