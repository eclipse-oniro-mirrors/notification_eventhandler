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
#include <memory>
#include <array>
#include <mutex>
#include <map>
#include <thread>
#include "event_logger.h"
#include "event_handler.h"

namespace OHOS {
namespace AppExecFwk {

namespace {
    DEFINE_EH_HILOG_LABEL("EventsEmitter");
    constexpr const char* EVENT_DATA = "eventData";
    constexpr const char* GENERIC_EVENT_DATA = "genericEventData";
}

static std::mutex g_eventsEmitterInsMutex;
static std::map<InnerEvent::EventId, std::unordered_set<std::shared_ptr<AniAsyncCallbackInfo>>> eventsEmitterInstances;
std::shared_ptr<EventsEmitterInstance> eventHandler;

AniAsyncCallbackInfo::~AniAsyncCallbackInfo()
{
    vm = nullptr;
}

EventsEmitterInstance::EventsEmitterInstance(const std::shared_ptr<EventRunner>& runner): EventHandler(runner)
{
    HILOGD("EventHandlerInstance constructed");
}
EventsEmitterInstance::~EventsEmitterInstance()
{
}

std::shared_ptr<EventsEmitterInstance> EventsEmitterInstance::GetInstance()
{
    static auto runner = EventRunner::Create("OS_eventsEmtr", ThreadMode::FFRT);
    if (runner.get() == nullptr) {
        HILOGE("failed to create EventRunner events_emitter");
        return nullptr;
    }
    static auto instance = std::make_shared<EventsEmitterInstance>(runner);
    return instance;
}

void EventsEmitterInstance::ProcessEvent(const InnerEvent::Pointer& event)
{
    InnerEvent::EventId eventId = event->GetInnerEventIdEx();
    auto callbackInfos = GetAsyncCallbackInfo(eventId);
    if (callbackInfos.size() <= 0) {
        HILOGW("ProcessEvent has no valid callback");
        return;
    }
    for (auto it = callbackInfos.begin(); it != callbackInfos.end(); ++it) {
        HILOGD("callbackInfo begin\n");
        if (*it == nullptr) {
            HILOGE("*it is empty\n");
            continue;
        }

        if ((*it)->vm == nullptr) {
            continue;
        }
        ani_boolean wasReleased = ANI_FALSE;
        ani_ref refa {};
        ani_env *env;
        ani_option interopEnabled {"--interop=disable", nullptr};
        ani_options aniArgs {1, &interopEnabled};
        if (ANI_OK != (*it)->vm->AttachCurrentThread(&aniArgs, ANI_VERSION_1, &env)) {
            HILOGE("vm GetEnv error");
            continue;
        }
        if (env->WeakReference_GetReference((*it)->callback, &wasReleased, &refa) != ANI_OK) {
            HILOGE("WeakReference_GetReference error");
            continue;
        }
        if (wasReleased) {
            HILOGD("callback is released");
            continue;
        }

        auto t = std::thread(EventsEmitter::ThreadFunction, env, refa, (*it)->data, (*it)->dataType);
        t.join();
        HILOGD("callbackInfo end\n");
    }
}

std::unordered_set<std::shared_ptr<AniAsyncCallbackInfo>> EventsEmitterInstance::GetAsyncCallbackInfo(
    const InnerEvent::EventId &eventId)
{
    std::lock_guard<std::mutex> lock(g_eventsEmitterInsMutex);
    auto iter = eventsEmitterInstances.find(eventId);
    if (iter == eventsEmitterInstances.end()) {
        std::unordered_set<std::shared_ptr<AniAsyncCallbackInfo>> result;
        HILOGW("ProcessEvent has no callback");
        return result;
    }
    for (auto it = iter->second.begin(); it != iter->second.end();) {
        if ((*it)->isDeleted == true || (*it)->vm == nullptr) {
            it = iter->second.erase(it);
            continue;
        }
        ++it;
    }
    return iter->second;
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
        ani_boolean wasReleased = ANI_FALSE;
        ani_ref refa {};
        if (env->WeakReference_GetReference(callbackInfo->callback, &wasReleased, &refa) != ANI_OK) {
            continue;
        }
        if (wasReleased) {
            continue;
        }
        ani_boolean isEq = false;
        env->Reference_StrictEquals(callback, refa, &isEq);
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
        ani_env *MyEnv;
        if (ANI_OK != callbackInfo->vm->GetEnv(ANI_VERSION_1, &MyEnv)) {
            return;
        }
        MyEnv->WeakReference_Delete(callbackInfo->callback);
        MyEnv->GlobalReference_Delete(callbackInfo->data);
        delete callbackInfo;
        callbackInfo = nullptr;
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
        HILOGD("DeleteCallbackInfo env equal");
        ani_boolean wasReleased = ANI_FALSE;
        ani_ref refa {};
        if (env->WeakReference_GetReference((*callbackInfo)->callback, &wasReleased, &refa) != ANI_OK) {
            ++callbackInfo;
            continue;
        }
        if (wasReleased) {
            continue;
        }
        ani_boolean isEq = false;
        env->Reference_StrictEquals(callback, refa, &isEq);
        if (!isEq) {
            ++callbackInfo;
            continue;
        }
        HILOGD("DeleteCallbackInfo callback equal");
        (*callbackInfo)->isDeleted = true;
        callbackInfo = iter->second.erase(callbackInfo);
        HILOGD("DeleteCallbackInfo success");
        return;
    }
}

void EventsEmitter::OffEmitterInstances(InnerEvent::EventId eventIdValue)
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

void EventsEmitter::OnOrOnce(
    ani_env *env, InnerEvent::EventId eventId, bool once, ani_ref callback, ani_string dataType)
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
        env->GetVM(&callbackInfo->vm);
        callbackInfo->once = once;
        callbackInfo->eventId = eventId;
        env->WeakReference_Create(callback, &callbackInfo->callback);
        callbackInfo->dataType = EventsEmitter::GetStdString(env, dataType);
        eventsEmitterInstances[eventId].insert(callbackInfo);
        HILOGD("onOrOnceEnd\n");
    }
}

ani_double EventsEmitter::GetListenerCount(ani_env *env, InnerEvent::EventId eventId)
{
    ani_double cnt = 0;
    std::lock_guard<std::mutex> lock(g_eventsEmitterInsMutex);
    auto subscribe = eventsEmitterInstances.find(eventId);
    if (subscribe != eventsEmitterInstances.end()) {
        for (auto it = subscribe->second.begin(); it != subscribe->second.end();) {
            if ((*it)->isDeleted == true || (*it)->vm == nullptr) {
                it = subscribe->second.erase(it);
                continue;
            }
            ani_boolean wasReleased = ANI_FALSE;
            ani_ref refa {};
            if (env->WeakReference_GetReference((*it)->callback, &wasReleased, &refa) != ANI_OK) {
                ++it;
                continue;
            }
            if (wasReleased) {
                it = subscribe->second.erase(it);
                continue;
            }
            ++it;
            ++cnt;
        }
    }
    return cnt;
}

bool EventsEmitter::IsExistValidCallback(const InnerEvent::EventId &eventId, ani_object eventData)
{
    HILOGD("IsExistValidCallback begin");
    std::lock_guard<std::mutex> lock(g_eventsEmitterInsMutex);
    auto subscribe = eventsEmitterInstances.find(eventId);
    if (subscribe == eventsEmitterInstances.end()) {
        HILOGW("IsExistValidCallback, Emit has no callback");
        return false;
    }
    if (subscribe->second.size() != 0) {
        for (auto it = subscribe->second.begin(); it != subscribe->second.end(); ++it) {
            if (*it == nullptr && eventData == nullptr) {
                continue;
            }
            ani_env *env;
            if ((*it)->vm == nullptr || (*it)->vm->GetEnv(ANI_VERSION_1, &env) != ANI_OK) {
                continue;
            }
            env->GlobalReference_Create(eventData, reinterpret_cast<ani_ref*>(&(*it)->data));
        }
        return true;
    }
    return false;
}

void EventsEmitter::EmitWithEventId(ani_env *env, ani_object InnerEvent, ani_object eventData)
{
    HILOGD("EmitWithEventId begin");
    ani_double eventId = 0;
    ani_status status = ANI_ERROR;
    if ((status = env->Object_GetPropertyByName_Double(InnerEvent, "eventId", &eventId)) != ANI_OK) {
        HILOGE("eventId not find");
        return;
    }
    HILOGD("EmitWithEventId eventId: %f\n", eventId);
    InnerEvent::EventId id = (uint32_t)eventId;
    if (!IsExistValidCallback(id, eventData)) {
        HILOGI("EmitWithEventId, Emit has no callback");
        return;
    }

    ani_ref obj;
    ani_boolean isUndefined = true;
    status = ANI_ERROR;
    Priority priority = Priority::LOW;
    if ((status = env->Object_GetPropertyByName_Ref(InnerEvent, "priority", &obj)) == ANI_OK) {
        HILOGD("get priority");
        if ((status = env->Reference_IsUndefined(obj, &isUndefined)) == ANI_OK) {
            HILOGD("get priority isUndefined success");
            if (!isUndefined) {
                HILOGD("get priority not undefined");
                ani_int res;
                env->EnumItem_GetValue_Int(reinterpret_cast<ani_enum_item>(obj), &res);
                HILOGD("priority is %{public}d", res);
                priority = static_cast<Priority>(res);
            }
        }
    }
    HILOGD("get priority end");
    auto event = InnerEvent::Get(id, std::make_unique<EventDataAni>());
    eventHandler->SendEvent(event, 0, priority);
    HILOGD("EmitWithEventId end");
}

void EventsEmitter::EmitWithEventIdString(
    ani_env *env, ani_string eventId, ani_object eventData, ani_enum_item enumItem)
{
    InnerEvent::EventId id = GetStdString(env, eventId);
    if (!IsExistValidCallback(id, eventData)) {
        HILOGI("EmitWithEventIdString, Emit has no callback");
        return;
    }
    Priority priority = Priority::LOW;

    if (enumItem !=nullptr) {
        ani_int res;
        env->EnumItem_GetValue_Int(enumItem, &res);
        HILOGD("priority is %{public}d", res);
        priority = static_cast<Priority>(res);
        return;
    }

    auto event = InnerEvent::Get(id, std::make_unique<EventDataAni>());
    eventHandler->SendEvent(event, 0, priority);
}

void EventsEmitter::ThreadFunction(ani_env *env, ani_ref callback, ani_object data, std::string dataType)
{
    HILOGD("threadFunciton begin");
    ani_vm *etsVm;
    ani_env *etsEnv;
    [[maybe_unused]] int res = env->GetVM(&etsVm);
    if (res != ANI_OK) {
        return;
    }
    HILOGD("threadFunciton GetVM success");
    ani_option interopEnabled {"--interop=disable", nullptr};
    ani_options aniArgs {1, &interopEnabled};
    if (ANI_OK != etsVm->AttachCurrentThread(&aniArgs, ANI_VERSION_1, &etsEnv)) {
        return;
    }
    HILOGD("threadFunciton AttachCurrentThread success");
    auto fnObj = reinterpret_cast<ani_fn_object>(callback);
    if (fnObj == nullptr) {
        HILOGE("threadFunciton fnObj is nullptr");
        return;
    }
    std::vector<ani_ref> args;
    if (data == nullptr) {
        HILOGD("threadFunciton data is nullptr");
        ani_class cls;
        ani_status status = ANI_ERROR;
        if (dataType == EVENT_DATA) {
            status = etsEnv->FindClass("L@ohos/events/emitter/emitter/EventDataInner;", &cls);
        } else if (dataType == GENERIC_EVENT_DATA) {
            status = etsEnv->FindClass("L@ohos/events/emitter/emitter/GenericEventDataInner;", &cls);
        }
        if (status != ANI_OK) {
            HILOGE("threadFunciton FindClass error%{public}d", status);
            return;
        }
        ani_method ctor1;
        status = etsEnv->Class_FindMethod(cls, "<ctor>", ":V", &ctor1);
        if (status != ANI_OK) {
            HILOGE("threadFunciton Class_FindMethod error%{public}d", status);
            return;
        }
        ani_object obj1;
        status = etsEnv->Object_New(cls, ctor1, &obj1);
        if (status != ANI_OK) {
            HILOGE("threadFunciton Object_New error%{public}d", status);
            return;
        }
        HILOGD("threadFunciton Object_New create");
        args.push_back(reinterpret_cast<ani_ref>(obj1));
    } else {
        args.push_back(reinterpret_cast<ani_ref>(data));
    }
    ani_ref result;
    if (ANI_OK != etsEnv->FunctionalObject_Call(fnObj, args.size(), args.data(), &result)) {
        return;
    }

    HILOGD("hello thread");
    if (ANI_OK != etsVm->DetachCurrentThread()) {
        return;
    }
}

static void OnOrOnceSync(ani_env *env, ani_double eventId, ani_boolean once, ani_ref callback, ani_string dataType)
{
    HILOGD("OnOrOnceSync begin");
    InnerEvent::EventId id = (uint32_t)eventId;
    EventsEmitter::OnOrOnce(env, id, once, callback, dataType);
}

static void OnOrOnceStringSync(
    ani_env *env, ani_string eventId, ani_boolean once, ani_ref callback, ani_string dataType)
{
    HILOGD("OnOrOnceStringSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::OnOrOnce(env, id, once, callback, dataType);
}

static void OnOrOnceGenericEventSync(
    ani_env *env, ani_string eventId, ani_boolean once, ani_ref callback, ani_string dataType)
{
    HILOGD("OnOrOnceGenericEventSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::OnOrOnce(env, id, once, callback, dataType);
}

static void OffStringIdSync(ani_env *env, ani_string eventId)
{
    HILOGD("OffStringIdSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::OffEmitterInstances(id);
    HILOGD("OffStringIdSync end");
}

static void OffStringSync(ani_env *env, ani_string eventId, ani_ref callback)
{
    HILOGD("OffStringSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::DeleteCallbackInfo(env, id, callback);
}

static void OffGenericEventSync(ani_env *env, ani_string eventId, ani_ref callback)
{
    HILOGD("OffGenericEventSync begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::DeleteCallbackInfo(env, id, callback);
}

static void OffNumberSync(ani_env *env, ani_double eventId)
{
    HILOGD("OffNumberSync begin");
    InnerEvent::EventId id = (uint32_t)eventId;
    EventsEmitter::OffEmitterInstances(id);
}

static void OffNumberCallbackSync(ani_env *env, ani_double eventId, ani_ref callback)
{
    HILOGD("OffNumberCallbackSync begin");
    InnerEvent::EventId id = (uint32_t)eventId;
    EventsEmitter::DeleteCallbackInfo(env, id, callback);
}

static ani_double getListenerCountNumber(ani_env *env, ani_double eventId)
{
    HILOGD("getListenerCountNumber begin");
    InnerEvent::EventId id = (uint32_t)eventId;
    return EventsEmitter::GetListenerCount(env, id);
}

static ani_double getListenerCountString(ani_env *env, ani_string eventId)
{
    HILOGD("getListenerCountString begin");
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    return EventsEmitter::GetListenerCount(env, id);
}

static void EmitStringSync(ani_env *env, ani_string eventId)
{
    HILOGD("EmitStringSync begin");
    EventsEmitter::EmitWithEventIdString(env, eventId, nullptr, nullptr);
}

static void EmitStringDataSync(ani_env *env, ani_string eventId, ani_string EventData)
{
    HILOGD("EmitStringDataSync begin");
    EventsEmitter::EmitWithEventIdString(env, eventId, EventData, nullptr);
}

static void EmitStringGenericSync(ani_env *env, ani_string eventId, ani_object GenericEventData)
{
    HILOGD("EmitStringGenericSync begin");
    EventsEmitter::EmitWithEventIdString(env, eventId, GenericEventData, nullptr);
}

static void EmitInnerEventSync(ani_env *env, ani_object InnerEvent)
{
    HILOGD("EmitInnerEventSync begin");
    EventsEmitter::EmitWithEventId(env, InnerEvent, nullptr);
}

static void EmitInnerEventDataSync(ani_env *env, ani_object InnerEvent, ani_object EventData)
{
    HILOGD("EmitInnerEventDataSync begin");
    EventsEmitter::EmitWithEventId(env, InnerEvent, EventData);
}

static ani_status getPriority(ani_env *env, ani_object options, ani_enum_item &priority)
{
    HILOGD("get priority");
    ani_ref obj;
    ani_boolean isUndefined = true;
    ani_status status = ANI_ERROR;
    if ((status = env->Object_GetPropertyByName_Ref(options, "priority", &obj)) == ANI_OK) {
        if ((status = env->Reference_IsUndefined(obj, &isUndefined)) == ANI_OK) {
            if (!isUndefined) {
                HILOGD("get priority not undefined");
                priority = reinterpret_cast<ani_enum_item>(obj);
            }
        }
    }
    return status;
}
static void EmitStringOptionsSync(ani_env *env, ani_string eventId, ani_object options)
{
    HILOGD("EmitStringOptionsSync begin");
    ani_enum_item priority = nullptr;
    getPriority(env, options, priority);
    EventsEmitter::EmitWithEventIdString(env, eventId, nullptr, priority);
    HILOGD("EmitStringOptionsSync end");
}

static void EmitStringOptionsGenericSync(ani_env *env,
    ani_string eventId, ani_object options, ani_object GenericEventData)
{
    HILOGD("EmitStringOptionsGenericSync begin");
    ani_enum_item priority = nullptr;
    getPriority(env, options, priority);
    EventsEmitter::EmitWithEventIdString(env, eventId, GenericEventData, priority);
    HILOGD("EmitStringOptionsGenericSync end");
}

static void EmitStringOptionsDataSync(ani_env *env,
    ani_string eventId, ani_object options, ani_object EventData)
{
    HILOGD("EmitStringOptionsDataSync begin");
    ani_enum_item priority = nullptr;
    getPriority(env, options, priority);
    EventsEmitter::EmitWithEventIdString(env, eventId, EventData, priority);
    HILOGD("EmitStringOptionsDataSync end");
}

ani_status init(ani_env *env, ani_namespace kitNs)
{
    std::array methods = {
        ani_native_function{"OnOrOnceSync", nullptr, reinterpret_cast<void *>(OnOrOnceSync)},
        ani_native_function{"OnOrOnceStringSync", nullptr, reinterpret_cast<void *>(OnOrOnceStringSync)},
        ani_native_function{"OnOrOnceGenericEventSync", nullptr, reinterpret_cast<void *>(OnOrOnceGenericEventSync)},
        ani_native_function{"OffStringIdSync", nullptr, reinterpret_cast<void *>(OffStringIdSync)},
        ani_native_function{"OffStringSync", nullptr, reinterpret_cast<void *>(OffStringSync)},
        ani_native_function{"OffGenericEventSync", nullptr, reinterpret_cast<void *>(OffGenericEventSync)},
        ani_native_function{"OffNumberSync", "D:V", reinterpret_cast<void *>(OffNumberSync)},
        ani_native_function{"OffNumberCallbackSync", nullptr, reinterpret_cast<void *>(OffNumberCallbackSync)},
        ani_native_function{"getListenerCountSync", "D:D", reinterpret_cast<void *>(getListenerCountNumber)},
        ani_native_function{"getListenerCountStringSync",
                            "Lstd/core/String;:D", reinterpret_cast<void *>(getListenerCountString)},
        ani_native_function{"EmitInnerEventSync", "L@ohos/events/emitter/emitter/InnerEvent;:V",
                            reinterpret_cast<void *>(EmitInnerEventSync)},
        ani_native_function{"EmitInnerEventDataSync",
            "L@ohos/events/emitter/emitter/InnerEvent;L@ohos/events/emitter/emitter/EventData;:V",
            reinterpret_cast<void *>(EmitInnerEventDataSync)},
        ani_native_function{"EmitStringSync", "Lstd/core/String;:V", reinterpret_cast<void *>(EmitStringSync)},
        ani_native_function{"EmitStringDataSync",
            "Lstd/core/String;L@ohos/events/emitter/emitter/EventData;:V",
            reinterpret_cast<void *>(EmitStringDataSync)},
        ani_native_function{"EmitStringGenericSync",
            "Lstd/core/String;L@ohos/events/emitter/emitter/GenericEventData;:V",
            reinterpret_cast<void *>(EmitStringGenericSync)},
        ani_native_function{"EmitStringOptionsSync",
            "Lstd/core/String;L@ohos/events/emitter/emitter/Options;:V",
            reinterpret_cast<void *>(EmitStringOptionsSync)},
        ani_native_function{"EmitStringOptionsGenericSync",
            "Lstd/core/String;L@ohos/events/emitter/emitter/Options;L@ohos/events/emitter/emitter/GenericEventData;:V",
            reinterpret_cast<void *>(EmitStringOptionsGenericSync)},
        ani_native_function{"EmitStringOptionsDataSync",
            "Lstd/core/String;L@ohos/events/emitter/emitter/Options;L@ohos/events/emitter/emitter/EventData;:V",
            reinterpret_cast<void *>(EmitStringOptionsDataSync)},
    };

    return env->Namespace_BindNativeFunctions(kitNs, methods.data(), methods.size());
}

extern "C" {
ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    HILOGD("ANI_Constructor begin");
    eventHandler = EventsEmitterInstance::GetInstance();
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
    status = init(env, kitNs);
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