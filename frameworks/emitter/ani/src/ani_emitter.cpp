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

#include "ani_emitter.h"

#include "event_logger.h"
#include "aync_callback_manager.h"
#include "interops.h"
#include "napi_agent.h"
#include "interop_js/hybridgref_ani.h"
#include "interop_js/arkts_interop_js_api.h"

namespace OHOS {
namespace AppExecFwk {

namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
ani_vm* g_AniVm = nullptr;
}

ani_vm* GetGlobalAniVm()
{
    return g_AniVm;
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

void EventsEmitter::OnOrOnce(
    ani_env *env, InnerEvent::EventId eventId, bool once, ani_ref callback, ani_string dataType)
{
    AsyncCallbackManager::GetInstance().InsertCallbackInfo(env, eventId, once, callback, dataType);
}

void EventsEmitter::OffEmitterInstances(InnerEvent::EventId eventIdValue)
{
    AsyncCallbackManager::GetInstance().DeleteCallbackInfoByEventId(eventIdValue);
}

void EventsEmitter::ReleaseAniData(ani_env *env, SerializeData* data)
{
    if (data == nullptr || env == nullptr) {
        HILOGE("ReleaseAniData data or env is null");
        return;
    }
    if (std::holds_alternative<ani_ref>(data->peerData)) {
        env->GlobalReference_Delete(std::get<ani_ref>(data->peerData));
    }
}

void EventsEmitter::ReleaseNapiData(SerializeData* data)
{
    if (data == nullptr || data->env == nullptr) {
        HILOGE("ReleaseNapiData data or env is null");
        return;
    }
    if (napi_delete_serialization_data(data->env, data->crossData) != napi_ok) {
        HILOGW("ReleaseNapiData failed to napi_delete_serialization_data");
    }
}

std::shared_ptr<SerializeData> EventsEmitter::GetSharedSerializeData(ani_env *env)
{
    auto serializeDataPtr = new (std::nothrow) SerializeData();
    if (serializeDataPtr == nullptr) {
        HILOGE("memory allocation failed");
        return nullptr;
    }
    ani_vm* vm = nullptr;
    auto status = env->GetVM(&vm);
    if (vm == nullptr) {
        HILOGE("Get vm failed. status: %{public}d", status);
        return nullptr;
    }
    std::shared_ptr<SerializeData> serializeData(serializeDataPtr, [vm](SerializeData* data) {
        if (data == nullptr) {
            HILOGE("GetSharedSerializeData data is null");
            return;
        }
        ani_env *env;
        ani_status status = vm->GetEnv(ANI_VERSION_1, &env);
        if (status == ANI_OK) {
            ReleaseAniData(env, data);
            ReleaseNapiData(data);
            delete data;
            data = nullptr;
            return;
        }
        ani_option interopEnabled {"--interop=disable", nullptr};
        ani_options aniArgs {1, &interopEnabled};
        status = vm->AttachCurrentThread(&aniArgs, ANI_VERSION_1, &env);
        if (status != ANI_OK) {
            HILOGE("attach thread failed");
            delete data;
            data = nullptr;
            return;
        }
        ReleaseAniData(env, data);
        ReleaseNapiData(data);
        vm->DetachCurrentThread();
        delete data;
        data = nullptr;
    });
    serializeData->envType = EnvType::ANI;
    return serializeData;
}

void EventsEmitter::EmitWithEventId(ani_env *env, ani_object InnerEvent, ani_object eventData)
{
    ani_long eventId = 0;
    ani_status status = ANI_ERROR;
    if ((status = env->Object_GetPropertyByName_Long(InnerEvent, "eventId", &eventId)) != ANI_OK) {
        HILOGE("eventId not find");
        return;
    }
    InnerEvent::EventId id = static_cast<uint32_t>(eventId);
    if (!AsyncCallbackManager::GetInstance().IsExistValidCallback(id)) {
        HILOGI("Emit has no callback");
        return;
    }
    ani_ref obj;
    ani_boolean isUndefined = true;
    status = ANI_ERROR;
    Priority priority = Priority::LOW;
    if ((status = env->Object_GetPropertyByName_Ref(InnerEvent, "priority", &obj)) == ANI_OK) {
        if ((status = env->Reference_IsUndefined(obj, &isUndefined)) == ANI_OK) {
            if (!isUndefined) {
                ani_int res;
                env->EnumItem_GetValue_Int(reinterpret_cast<ani_enum_item>(obj), &res);
                priority = static_cast<Priority>(res);
            }
        }
    }
    auto serializeData = EventsEmitter::GetSharedSerializeData(env);
    if (serializeData == nullptr) {
        HILOGE("GetSharedSerializeData serializeData is null");
        return;
    }
    if (!AniSerialize::PeerSerialize(env, eventData, serializeData)) {
        HILOGE("GetSharedSerializeData PeerSerialize failed");
        return;
    }
    if (AsyncCallbackManager::GetInstance().IsCrossRuntime(id, EnvType::ANI)) {
        serializeData->isCrossRuntime = true;
        if (!AniSerialize::CrossSerialize(env, eventData, serializeData)) {
            HILOGE("GetSharedSerializeData CrossSerialize failed");
            return;
        }
    }
    auto event = InnerEvent::Get(id, serializeData);
    event->SetIsEnhanced(true);
    EventHandlerInstance::GetInstance()->SendEvent(event, 0, priority);
}

void EventsEmitter::EmitWithEventIdString(
    ani_env *env, ani_string eventId, ani_object eventData, ani_enum_item enumItem)
{
    InnerEvent::EventId id = GetStdString(env, eventId);
    if (!AsyncCallbackManager::GetInstance().IsExistValidCallback(id)) {
        HILOGI("Emit has no callback");
        return;
    }
    Priority priority = Priority::LOW;
    if (enumItem != nullptr) {
        ani_int res;
        env->EnumItem_GetValue_Int(enumItem, &res);
        priority = static_cast<Priority>(res);
    }
    auto serializeData = EventsEmitter::GetSharedSerializeData(env);
    if (serializeData == nullptr) {
        return;
    }
    if (!AniSerialize::PeerSerialize(env, eventData, serializeData)) {
        return;
    }
    if (AsyncCallbackManager::GetInstance().IsCrossRuntime(id, EnvType::ANI)) {
        serializeData->isCrossRuntime = true;
        if (!AniSerialize::CrossSerialize(env, eventData, serializeData)) {
            return;
        }
    }
    auto event = InnerEvent::Get(id, serializeData);
    event->SetIsEnhanced(true);
    EventHandlerInstance::GetInstance()->SendEvent(event, 0, priority);
}

ani_long EventsEmitter::GetListenerCount(InnerEvent::EventId eventId)
{
    return static_cast<int64_t>(AsyncCallbackManager::GetInstance().GetListenerCountByEventId(eventId));
}

static void OnOrOnceSync(ani_env *env, ani_long eventId, ani_boolean once, ani_ref callback, ani_string dataType)
{
    InnerEvent::EventId id = static_cast<uint32_t>(eventId);
    EventsEmitter::OnOrOnce(env, id, once, callback, dataType);
}

static void OnOrOnceStringSync(
    ani_env *env, ani_string eventId, ani_boolean once, ani_ref callback, ani_string dataType)
{
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::OnOrOnce(env, id, once, callback, dataType);
}

static void OnOrOnceGenericEventSync(
    ani_env *env, ani_string eventId, ani_boolean once, ani_ref callback, ani_string dataType)
{
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::OnOrOnce(env, id, once, callback, dataType);
}

static void OffStringIdSync(ani_env *env, ani_string eventId)
{
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::OffEmitterInstances(id);
}

static void OffStringSync(ani_env *env, ani_string eventId, ani_ref callback)
{
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    AsyncCallbackManager::GetInstance().DeleteCallbackInfo(env, id, callback);
}

static void OffGenericEventSync(ani_env *env, ani_string eventId, ani_ref callback)
{
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    AsyncCallbackManager::GetInstance().DeleteCallbackInfo(env, id, callback);
}

static void OffNumberSync(ani_env *env, ani_long eventId)
{
    InnerEvent::EventId id = static_cast<uint32_t>(eventId);
    EventsEmitter::OffEmitterInstances(id);
}

static void OffNumberCallbackSync(ani_env *env, ani_long eventId, ani_ref callback)
{
    InnerEvent::EventId id = static_cast<uint32_t>(eventId);
    AsyncCallbackManager::GetInstance().DeleteCallbackInfo(env, id, callback);
}

static ani_long getListenerCountNumber(ani_env *env, ani_long eventId)
{
    InnerEvent::EventId id = static_cast<uint32_t>(eventId);
    return EventsEmitter::GetListenerCount(id);
}

static ani_long getListenerCountString(ani_env *env, ani_string eventId)
{
    InnerEvent::EventId id = EventsEmitter::GetStdString(env, eventId);
    return EventsEmitter::GetListenerCount(id);
}

static void EmitStringSync(ani_env *env, ani_string eventId)
{
    EventsEmitter::EmitWithEventIdString(env, eventId, nullptr, nullptr);
}

static void EmitStringDataSync(ani_env *env, ani_string eventId, ani_string EventData)
{
    EventsEmitter::EmitWithEventIdString(env, eventId, EventData, nullptr);
}

static void EmitStringGenericSync(ani_env *env, ani_string eventId, ani_object GenericEventData)
{
    EventsEmitter::EmitWithEventIdString(env, eventId, GenericEventData, nullptr);
}

static void EmitInnerEventSync(ani_env *env, ani_object InnerEvent)
{
    EventsEmitter::EmitWithEventId(env, InnerEvent, nullptr);
}

static void EmitInnerEventDataSync(ani_env *env, ani_object InnerEvent, ani_object EventData)
{
    EventsEmitter::EmitWithEventId(env, InnerEvent, EventData);
}

static ani_status GetPriority(ani_env *env, ani_object options, ani_enum_item &priority)
{
    ani_ref obj;
    ani_boolean isUndefined = true;
    ani_status status = env->Object_GetPropertyByName_Ref(options, "priority", &obj);
    if (status == ANI_OK) {
        status = env->Reference_IsUndefined(obj, &isUndefined);
        if (status == ANI_OK) {
            if (!isUndefined) {
                priority = reinterpret_cast<ani_enum_item>(obj);
            }
        }
    }
    return status;
}

static void EmitStringOptionsSync(ani_env *env, ani_string eventId, ani_object options)
{
    ani_enum_item priority = nullptr;
    GetPriority(env, options, priority);
    EventsEmitter::EmitWithEventIdString(env, eventId, nullptr, priority);
}

static void EmitStringOptionsGenericSync(ani_env *env,
    ani_string eventId, ani_object options, ani_object GenericEventData)
{
    ani_enum_item priority = nullptr;
    GetPriority(env, options, priority);
    EventsEmitter::EmitWithEventIdString(env, eventId, GenericEventData, priority);
}

static void EmitStringOptionsDataSync(ani_env *env,
    ani_string eventId, ani_object options, ani_object EventData)
{
    ani_enum_item priority = nullptr;
    GetPriority(env, options, priority);
    EventsEmitter::EmitWithEventIdString(env, eventId, EventData, priority);
}

ani_status init(ani_env *env, ani_namespace kitNs)
{
    std::array methods = {
        ani_native_function{"OnOrOnceSync", nullptr, reinterpret_cast<void *>(OnOrOnceSync)},
        ani_native_function{"OnOrOnceStringSync", nullptr, reinterpret_cast<void *>(OnOrOnceStringSync)},
        ani_native_function{"OnOrOnceGenericEventSync", nullptr, reinterpret_cast<void *>(OnOrOnceGenericEventSync)},
        ani_native_function{"OffStringIdSync", "Lstd/core/String;:V", reinterpret_cast<void *>(OffStringIdSync)},
        ani_native_function{"OffStringSync", nullptr, reinterpret_cast<void *>(OffStringSync)},
        ani_native_function{"OffGenericEventSync", nullptr, reinterpret_cast<void *>(OffGenericEventSync)},
        ani_native_function{"OffNumberSync", "J:V", reinterpret_cast<void *>(OffNumberSync)},
        ani_native_function{"OffNumberCallbackSync", nullptr, reinterpret_cast<void *>(OffNumberCallbackSync)},
        ani_native_function{"getListenerCountSync", "J:J", reinterpret_cast<void *>(getListenerCountNumber)},
        ani_native_function{"getListenerCountStringSync",
            "Lstd/core/String;:J", reinterpret_cast<void *>(getListenerCountString)},
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
    AgentInit();
    return env->Namespace_BindNativeFunctions(kitNs, methods.data(), methods.size());
}

extern "C" {
ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    HILOGD("ANI_Constructor begin");
    g_AniVm = vm;
    ani_status status = ANI_ERROR;
    ani_env *env;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        HILOGE("Unsupported ANI_VERSION_1.");
        return status;
    }

    ani_namespace kitNs;
    status = env->FindNamespace("L@ohos/events/emitter/emitter;", &kitNs);
    if (status != ANI_OK) {
        HILOGE("Not found ani_namespace L@ohos/events/emitter/emitter");
        return status;
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