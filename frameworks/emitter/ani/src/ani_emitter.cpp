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
#include "interop_js/arkts_esvalue.h"
#include "interop_js/arkts_interop_js_api.h"
#include "interop_js/hybridgref_ani.h"

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
    ani_env *env, CompositeEventId compositeId, bool once, ani_ref callback, ani_string dataType)
{
    AsyncCallbackManager::GetInstance().InsertCallbackInfo(env, compositeId, once, callback, dataType);
}

void EventsEmitter::OffEmitterInstances(CompositeEventId compositeId)
{
    AsyncCallbackManager::GetInstance().DeleteCallbackInfoByEventId(compositeId);
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
    CompositeEventId id = static_cast<uint32_t>(eventId);
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
    auto event = InnerEvent::Get(id.eventId, serializeData);
    event->SetIsEnhanced(true);
    EventHandlerInstance::GetInstance()->SendEvent(event, 0, priority);
}

void EventsEmitter::EmitWithEventIdString(
    ani_env *env, ani_string eventId, ani_object eventData, ani_enum_item enumItem, uint32_t emitterId)
{
    CompositeEventId id;
    id.eventId = GetStdString(env, eventId);
    id.emitterId = emitterId;
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
    auto event = InnerEvent::Get(id.eventId, serializeData);
    event->SetEmitterId(emitterId);
    event->SetIsEnhanced(true);
    EventHandlerInstance::GetInstance()->SendEvent(event, 0, priority);
}

ani_long EventsEmitter::GetListenerCount(CompositeEventId compositeId)
{
    return static_cast<int64_t>(AsyncCallbackManager::GetInstance().GetListenerCountByEventId(compositeId));
}

void EventsEmitter::EmitterConstructor(ani_env *env, ani_object emitter)
{
    ani_class emitterClass;
    ani_status status = env->FindClass("@ohos.events.emitter.emitter.Emitter", &emitterClass);
    if (status != ANI_OK) {
        HILOGE("Failed to find Emitter class");
        return ;
    }

    uint32_t instanceId = GetNextEmitterInstanceId();
    ani_long emitterIdValue = static_cast<ani_long>(instanceId);
    env->Object_SetPropertyByName_Long(emitter, "emitterId", emitterIdValue);
}

void EventsEmitter::EmitterOnOrOnce(
    ani_env *env, ani_object emitter, ani_string eventId, ani_boolean once, ani_ref callback, ani_string dataType)
{
    ani_long emitterId = 0;
    ani_status status = env->Object_GetPropertyByName_Long(emitter, "emitterId", &emitterId);
    if (status != ANI_OK) {
        HILOGE("Failed to get emitterId");
        return;
    }

    CompositeEventId id;
    id.eventId = GetStdString(env, eventId);
    id.emitterId = static_cast<uint32_t>(emitterId);
    EventsEmitter::OnOrOnce(env, id, once, callback, dataType);
}

void EventsEmitter::EmitterOff(ani_env *env, ani_object emitter, ani_string eventId, ani_ref callback)
{
    ani_long emitterId = 0;
    ani_status status = env->Object_GetPropertyByName_Long(emitter, "emitterId", &emitterId);
    if (status != ANI_OK) {
        HILOGE("Failed to get emitterId");
        return;
    }

    CompositeEventId id;
    id.eventId = GetStdString(env, eventId);
    id.emitterId = static_cast<uint32_t>(emitterId);

    if (callback == nullptr) {
        EventsEmitter::OffEmitterInstances(id);
    } else {
        AsyncCallbackManager::GetInstance().DeleteCallbackInfo(env, id, callback);
    }
}

ani_long EventsEmitter::EmitterGetListenerCount(ani_env *env, ani_object emitter, ani_string eventId)
{
    ani_long emitterId = 0;
    ani_status status = env->Object_GetPropertyByName_Long(emitter, "emitterId", &emitterId);
    if (status != ANI_OK) {
        HILOGE("Failed to get emitterId");
        return 0;
    }

    CompositeEventId id;
    id.eventId = GetStdString(env, eventId);
    id.emitterId = static_cast<uint32_t>(emitterId);
    return EventsEmitter::GetListenerCount(id);
}

void EventsEmitter::EmitterClean(ani_env *env, ani_object cleaner)
{
    ani_long emitterId = 0;
    ani_status status = env->Object_GetPropertyByName_Long(cleaner, "emitterId", &emitterId);
    if (status != ANI_OK) {
        HILOGE("Failed to get emitterId");
        return;
    }
    CompositeEventId id;
    id.emitterId = static_cast<uint32_t>(emitterId);
    if (id.emitterId > 0) {
        AsyncCallbackManager::GetInstance().CleanCallbackInfo(id);
    }
}

static void OnOrOnceSync(ani_env *env, ani_long eventId, ani_boolean once, ani_ref callback, ani_string dataType)
{
    CompositeEventId id = static_cast<uint32_t>(eventId);
    EventsEmitter::OnOrOnce(env, id, once, callback, dataType);
}

static void OnOrOnceStringSync(
    ani_env *env, ani_string eventId, ani_boolean once, ani_ref callback, ani_string dataType)
{
    CompositeEventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::OnOrOnce(env, id, once, callback, dataType);
}

static void OnOrOnceGenericEventSync(
    ani_env *env, ani_string eventId, ani_boolean once, ani_ref callback, ani_string dataType)
{
    CompositeEventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::OnOrOnce(env, id, once, callback, dataType);
}

static void OffStringIdSync(ani_env *env, ani_string eventId)
{
    CompositeEventId id = EventsEmitter::GetStdString(env, eventId);
    EventsEmitter::OffEmitterInstances(id);
}

static void OffStringSync(ani_env *env, ani_string eventId, ani_ref callback)
{
    CompositeEventId id = EventsEmitter::GetStdString(env, eventId);
    AsyncCallbackManager::GetInstance().DeleteCallbackInfo(env, id, callback);
}

static void OffGenericEventSync(ani_env *env, ani_string eventId, ani_ref callback)
{
    CompositeEventId id = EventsEmitter::GetStdString(env, eventId);
    AsyncCallbackManager::GetInstance().DeleteCallbackInfo(env, id, callback);
}

static void OffNumberSync(ani_env *env, ani_long eventId)
{
    CompositeEventId id = static_cast<uint32_t>(eventId);
    EventsEmitter::OffEmitterInstances(id);
}

static void OffNumberCallbackSync(ani_env *env, ani_long eventId, ani_ref callback)
{
    CompositeEventId id = static_cast<uint32_t>(eventId);
    AsyncCallbackManager::GetInstance().DeleteCallbackInfo(env, id, callback);
}

static ani_long getListenerCountNumber(ani_env *env, ani_long eventId)
{
    CompositeEventId id = static_cast<uint32_t>(eventId);
    return EventsEmitter::GetListenerCount(id);
}

static ani_long getListenerCountString(ani_env *env, ani_string eventId)
{
    CompositeEventId id = EventsEmitter::GetStdString(env, eventId);
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

void EventsEmitter::EmitterEmit(
    ani_env *env, ani_object emitter, ani_string eventId, ani_object eventData, ani_object options)
{
    ani_long emitterId = 0;
    ani_status status = env->Object_GetPropertyByName_Long(emitter, "emitterId", &emitterId);
    if (status != ANI_OK) {
        HILOGE("Failed to get emitterId");
        return;
    }

    ani_enum_item priority = nullptr;
    if (options != nullptr) {
        GetPriority(env, options, priority);
    }
    EventsEmitter::EmitWithEventIdString(env, eventId, eventData, priority, static_cast<uint32_t>(emitterId));
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

static void EmitterConstructor(ani_env *env, ani_object emitter)
{
    EventsEmitter::EmitterConstructor(env, emitter);
}

static void EmitterOnDataSync(
    ani_env *env, ani_object emitter, ani_string eventId, ani_ref callback, ani_string dataType)
{
    EventsEmitter::EmitterOnOrOnce(env, emitter, eventId, false, callback, dataType);
}

static void EmitterOnceDataSync(
    ani_env *env, ani_object emitter, ani_string eventId, ani_ref callback, ani_string dataType)
{
    EventsEmitter::EmitterOnOrOnce(env, emitter, eventId, true, callback, dataType);
}

static void EmitterOnGenericSync(
    ani_env *env, ani_object emitter, ani_string eventId, ani_ref callback, ani_string dataType)
{
    EventsEmitter::EmitterOnOrOnce(env, emitter, eventId, false, callback, dataType);
}

static void EmitterOnceGenericSync(
    ani_env *env, ani_object emitter, ani_string eventId, ani_ref callback, ani_string dataType)
{
    EventsEmitter::EmitterOnOrOnce(env, emitter, eventId, true, callback, dataType);
}

static void EmitterOffSync(ani_env *env, ani_object emitter, ani_string eventId)
{
    EventsEmitter::EmitterOff(env, emitter, eventId);
}

static void EmitterOffCallbackSync(ani_env *env, ani_object emitter, ani_string eventId, ani_ref callback)
{
    EventsEmitter::EmitterOff(env, emitter, eventId, callback);
}

static void EmitterEmitSync(ani_env *env, ani_object emitter, ani_string eventId)
{
    EventsEmitter::EmitterEmit(env, emitter, eventId);
}

static void EmitterEmitDataSync(ani_env *env, ani_object emitter, ani_string eventId, ani_object EventData)
{
    EventsEmitter::EmitterEmit(env, emitter, eventId, EventData);
}

static void EmitterEmitGenericSync(ani_env *env, ani_object emitter, ani_string eventId, ani_object GenericEventData)
{
    EventsEmitter::EmitterEmit(env, emitter, eventId, GenericEventData);
}

static void EmitterEmitOptionsSync(ani_env *env, ani_object emitter, ani_string eventId, ani_object options)
{
    EventsEmitter::EmitterEmit(env, emitter, eventId, nullptr, options);
}

static void EmitterEmitOptionsDataSync(
    ani_env *env, ani_object emitter, ani_string eventId, ani_object options, ani_object EventData)
{
    EventsEmitter::EmitterEmit(env, emitter, eventId, EventData, options);
}

static void EmitterEmitOptionsGenericSync(
    ani_env *env, ani_object emitter, ani_string eventId, ani_object options, ani_object GenericEventData)
{
    EventsEmitter::EmitterEmit(env, emitter, eventId, GenericEventData, options);
}

static ani_long EmitterGetListenerCountSync(ani_env *env, ani_object emitter, ani_string eventId)
{
    return EventsEmitter::EmitterGetListenerCount(env, emitter, eventId);
}

static ani_ref EmitterTransferToDynamic(ani_env *env, ani_object input)
{
    ani_ref undefinedRef {};
    env->GetUndefined(&undefinedRef);
    ani_long emitterId = 0;
    ani_status status = env->Object_GetPropertyByName_Long(input, "emitterId", &emitterId);
    if (status != ANI_OK || emitterId == 0) {
        HILOGE("Failed to get emitterId");
        return undefinedRef;
    }
    napi_env jsEnv;
    uint32_t id = static_cast<uint32_t>(emitterId);
    HILOGD("emitterId: %{public}d", id);
    arkts_napi_scope_open(env, &jsEnv);
    napi_value object = nullptr;
    napi_create_object(jsEnv, &object);
    napi_wrap(
        jsEnv, object, reinterpret_cast<void*>(id), [](napi_env env, void* data, void* hint) {}, nullptr, nullptr);
    ani_ref result {};
    arkts_napi_scope_close_n(jsEnv, 1, &object, &result);
    return result;
}

static ani_ref EmitterTransferToStatic(ani_env *env, ani_object input)
{
    ani_ref undefinedRef {};
    env->GetUndefined(&undefinedRef);
    uint32_t emitterId = 0;
    arkts_esvalue_unwrap(env, input, reinterpret_cast<void**>(&emitterId));
    if (emitterId == 0) {
        HILOGE("Failed to get emitterId");
        return undefinedRef;
    }
    HILOGD("emitterId: %{public}d", emitterId);
    ani_class cls;
    ani_status status = env->FindClass("@ohos.events.emitter.emitter.Emitter", &cls);
    if (status != ANI_OK) {
        HILOGE("Failed to find Emitter class");
        return undefinedRef;
    }
    ani_method ctor = nullptr;
    status = env->Class_FindMethod(cls, "<ctor>", ":", &ctor);
    if (status != ANI_OK) {
        HILOGE("Class_FindMethod error. result: %{public}d.", status);
        return undefinedRef;
    }
    ani_object ani_data;
    status = env->Object_New(cls, ctor, &ani_data);
    if (status != ANI_OK) {
        HILOGE("Object_New error. result: %{public}d.", status);
        return undefinedRef;
    }
    ani_long emitterIdValue = static_cast<ani_long>(emitterId);
    env->Object_SetPropertyByName_Long(ani_data, "emitterId", emitterIdValue);
    ani_ref result = nullptr;
    if (env->GlobalReference_Create(ani_data, &result) != ANI_OK) {
        HILOGE("GlobalReference_Create failed");
        return undefinedRef;
    }
    return result;
}

static void Clean(ani_env *env, ani_object cleaner)
{
    EventsEmitter::EmitterClean(env, cleaner);
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
        ani_native_function{"EmitterConstructor", nullptr, reinterpret_cast<void *>(EmitterConstructor)},
        ani_native_function{"EmitterOnDataSync", nullptr, reinterpret_cast<void *>(EmitterOnDataSync)},
        ani_native_function{"EmitterOnceDataSync", nullptr, reinterpret_cast<void *>(EmitterOnceDataSync)},
        ani_native_function{"EmitterOnGenericSync", nullptr, reinterpret_cast<void *>(EmitterOnGenericSync)},
        ani_native_function{"EmitterOnceGenericSync", nullptr, reinterpret_cast<void *>(EmitterOnceGenericSync)},
        ani_native_function{"EmitterOffSync", nullptr, reinterpret_cast<void *>(EmitterOffSync)},
        ani_native_function{"EmitterOffDataSync", nullptr, reinterpret_cast<void *>(EmitterOffCallbackSync)},
        ani_native_function{"EmitterOffGenericSync", nullptr, reinterpret_cast<void *>(EmitterOffCallbackSync)},
        ani_native_function{"EmitterEmitSync", nullptr, reinterpret_cast<void *>(EmitterEmitSync)},
        ani_native_function{"EmitterEmitDataSync", nullptr, reinterpret_cast<void *>(EmitterEmitDataSync)},
        ani_native_function{"EmitterEmitGenericSync", nullptr, reinterpret_cast<void *>(EmitterEmitGenericSync)},
        ani_native_function{"EmitterEmitOptionsSync",  nullptr, reinterpret_cast<void *>(EmitterEmitOptionsSync)},
        ani_native_function{"EmitterEmitOptionsDataSync",
            nullptr, reinterpret_cast<void *>(EmitterEmitOptionsDataSync)},
        ani_native_function{"EmitterEmitOptionsGenericSync",
            nullptr, reinterpret_cast<void *>(EmitterEmitOptionsGenericSync)},
        ani_native_function{"EmitterGetListenerCountSync",
            nullptr, reinterpret_cast<void *>(EmitterGetListenerCountSync)},
        ani_native_function{"EmitterTransferToDynamic", nullptr, reinterpret_cast<void *>(EmitterTransferToDynamic)},
        ani_native_function{"EmitterTransferToStatic", nullptr, reinterpret_cast<void *>(EmitterTransferToStatic)},
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
    ani_class cls;
    status = env->FindClass("@ohos.events.emitter.emitter.Cleaner", &cls);
    if (status != ANI_OK) {
        HILOGE("Not found @ohos.events.emitter.emitter.Cleaner");
        return ANI_INVALID_ARGS;
    }
    std::array cleanMethod = {
    ani_native_function{"clean", nullptr, reinterpret_cast<void *>(Clean)}};
    status = env->Class_BindNativeMethods(cls, cleanMethod.data(), cleanMethod.size());
    if (status != ANI_OK) {
        HILOGE("Cannot bind native methods to @ohos.events.emitter.emitter");
        return ANI_INVALID_TYPE;
    }
    *result = ANI_VERSION_1;
    return ANI_OK;
}
}
}  // namespace AppExecFwk
}  // namespace OHOS
