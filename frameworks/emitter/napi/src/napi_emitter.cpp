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

#include "napi_emitter.h"

#include <memory>
#include <string>
#include "event_logger.h"
#include "js_native_api_types.h"
#include "aync_callback_manager.h"
#include "napi_serialize.h"
#include "interops.h"
#include "napi_agent.h"

using namespace std;
namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
constexpr static uint32_t ARGC_ONE = 1u;
static const int32_t ARGC_NUM = 2;
static const int32_t NAPI_VALUE_STRING_LEN = 10240;
}

bool GetEventIdWithNumberOrString(
    napi_env env, napi_value argv, napi_valuetype eventValueType, InnerEvent::EventId &eventId)
{
    if (eventValueType == napi_string) {
        auto valueCStr = std::make_unique<char[]>(NAPI_VALUE_STRING_LEN + 1);
        size_t valueStrLength = 0;
        napi_get_value_string_utf8(env, argv, valueCStr.get(), NAPI_VALUE_STRING_LEN, &valueStrLength);
        std::string id(valueCStr.get(), valueStrLength);
        if (id.empty()) {
            return false;
        }
        eventId = id;
        HILOGD("Event id value:%{public}s", id.c_str());
    } else {
        uint32_t id = 0u;
        napi_get_value_uint32(env, argv, &id);
        eventId = id;
        HILOGD("Event id value:%{public}u", id);
    }
    return true;
}

napi_value JS_Off(napi_env env, napi_callback_info cbinfo)
{
    HILOGD("JS_Off enter");
    size_t argc = ARGC_NUM;
    napi_value argv[ARGC_NUM] = {0};
    NAPI_CALL(env, napi_get_cb_info(env, cbinfo, &argc, argv, NULL, NULL));
    if (argc < 1) {
        HILOGE("requires at least 1 parameter");
        return nullptr;
    }

    napi_valuetype eventValueType;
    napi_typeof(env, argv[0], &eventValueType);
    if (eventValueType != napi_number && eventValueType != napi_string) {
        HILOGE("type mismatch for parameter 1");
        return nullptr;
    }

    InnerEvent::EventId eventId = 0u;
    bool ret = GetEventIdWithNumberOrString(env, argv[0], eventValueType, eventId);
    if (!ret) {
        HILOGE("Event id is empty for parameter 1.");
        return nullptr;
    }

    if (argc == ARGC_NUM) {
        napi_valuetype eventHandleType;
        napi_typeof(env, argv[1], &eventHandleType);
        if (eventHandleType != napi_function) {
            HILOGE("type mismatch for parameter 2");
            return nullptr;
        }
        AsyncCallbackManager::GetInstance().DeleteCallbackInfo(env, eventId, argv[1]);
        return nullptr;
    }
    AsyncCallbackManager::GetInstance().DeleteCallbackInfoByEventId(eventId);
    return nullptr;
}

bool EmitWithEventData(napi_env env, napi_value argv, const InnerEvent::EventId &eventId, Priority priority)
{
    HILOGD("EmitWithEventData enter");
    napi_valuetype dataType;
    napi_typeof(env, argv, &dataType);
    if (dataType != napi_object) {
        HILOGE("type mismatch for parameter 2");
        return false;
    }

    auto serializeDataPtr = new (std::nothrow) SerializeData();
    if (serializeDataPtr == nullptr) {
        HILOGE("memory allocation failed");
        return false;
    }
    std::shared_ptr<SerializeData> serializeData(serializeDataPtr, [env](SerializeData* data) {
        if (env != nullptr && std::get<napi_value>(data->peerData)) {
            napi_delete_serialization_data(env, std::get<napi_value>(data->peerData));
        }
        delete data;
        data = nullptr;
    });
    serializeData->envType = EnvType::NAPI;
    if (!NapiSerialize::PeerSerialize(env, argv, serializeData)) {
        return false;
    }
    if (AsyncCallbackManager::GetInstance().IsCrossRuntime(eventId, EnvType::NAPI)) {
        serializeData->isCrossRuntime = true;
        if (!NapiSerialize::CrossSerialize(env, argv, serializeData)) {
            return false;
        }
    }
    auto event = InnerEvent::Get(eventId, serializeData);
    event->SetIsEnhanced(true);
    EventHandlerInstance::GetInstance()->SendEvent(event, 0, priority);
    return true;
}

void EmitWithDefaultData(InnerEvent::EventId eventId, Priority priority)
{
    auto serializeData = make_shared<SerializeData>();
    serializeData->envType = EnvType::NAPI;
    if (AsyncCallbackManager::GetInstance().IsCrossRuntime(eventId, EnvType::NAPI)) {
        serializeData->isCrossRuntime = true;
    }
    auto event = InnerEvent::Get(eventId, serializeData);
    event->SetIsEnhanced(true);
    EventHandlerInstance::GetInstance()->SendEvent(event, 0, priority);
}

napi_value EmitWithEventIdUint32(napi_env env, size_t argc, napi_value argv[])
{
    InnerEvent::EventId eventId = 0u;
    bool hasEventId = false;
    napi_value value = nullptr;
    napi_has_named_property(env, argv[0], "eventId", &hasEventId);
    if (hasEventId == false) {
        HILOGE("Wrong argument 1 does not have event id");
        return nullptr;
    }

    napi_get_named_property(env, argv[0], "eventId", &value);
    uint32_t id = 0u;
    napi_get_value_uint32(env, value, &id);
    eventId = id;
    HILOGD("Event id value:%{public}u", id);

    if (!AsyncCallbackManager::GetInstance().IsExistValidCallback(eventId)) {
        EH_LOGE_LIMIT("Invalid callback");
        return nullptr;
    }

    bool hasPriority = false;
    napi_has_named_property(env, argv[0], "priority", &hasPriority);
    Priority priority = Priority::LOW;
    if (hasPriority) {
        napi_get_named_property(env, argv[0], "priority", &value);
        uint32_t priorityValue = 0u;
        napi_get_value_uint32(env, value, &priorityValue);
        HILOGD("Event priority:%{public}d", priorityValue);
        priority = static_cast<Priority>(priorityValue);
    }

    if (argc == ARGC_NUM && EmitWithEventData(env, argv[1], eventId, priority)) {
        return nullptr;
    } else {
        EmitWithDefaultData(eventId, priority);
    }
    return nullptr;
}

napi_value EmitWithEventIdString(napi_env env, size_t argc, napi_value argv[])
{
    InnerEvent::EventId eventId = 0u;
    auto valueCStr = std::make_unique<char[]>(NAPI_VALUE_STRING_LEN + 1);
    size_t valueStrLength = 0;
    napi_get_value_string_utf8(env, argv[0], valueCStr.get(), NAPI_VALUE_STRING_LEN, &valueStrLength);
    std::string id(valueCStr.get(), valueStrLength);
    if (id.empty()) {
        HILOGE("Invalid event id:%{public}s", id.c_str());
        return nullptr;
    }
    eventId = id;
    HILOGD("Event id value:%{public}s", id.c_str());

    if (!AsyncCallbackManager::GetInstance().IsExistValidCallback(eventId)) {
        EH_LOGE_LIMIT("Invalid callback");
        return nullptr;
    }
    Priority priority = Priority::LOW;
    if (argc < ARGC_NUM) {
        EmitWithDefaultData(eventId, priority);
        return nullptr;
    }
    bool hasPriority = false;
    napi_value value = nullptr;
    napi_has_named_property(env, argv[1], "priority", &hasPriority);
    if (!hasPriority) {
        if (!EmitWithEventData(env, argv[1], eventId, priority)) {
            EmitWithDefaultData(eventId, priority);
        }
        return nullptr;
    }

    napi_get_named_property(env, argv[1], "priority", &value);
    uint32_t priorityValue = 0u;
    napi_get_value_uint32(env, value, &priorityValue);
    HILOGD("Event priority:%{public}d", priorityValue);
    priority = static_cast<Priority>(priorityValue);

    if (argc > ARGC_NUM && EmitWithEventData(env, argv[ARGC_NUM], eventId, priority)) {
        return nullptr;
    } else {
        EmitWithDefaultData(eventId, priority);
    }
    return nullptr;
}

napi_value JS_Emit(napi_env env, napi_callback_info cbinfo)
{
    HILOGD("JS_Emit enter");
    size_t argc = ARGC_NUM + ARGC_ONE;
    napi_value argv[ARGC_NUM + ARGC_ONE] = {0};
    NAPI_CALL(env, napi_get_cb_info(env, cbinfo, &argc, argv, NULL, NULL));
    if (argc < ARGC_ONE) {
        HILOGE("Requires more than 1 parameter");
        return nullptr;
    }
    napi_valuetype eventValueType;
    napi_typeof(env, argv[0], &eventValueType);
    if (eventValueType != napi_object && eventValueType != napi_string) {
        HILOGE("Type mismatch for parameter 1");
        return nullptr;
    }
    if (eventValueType == napi_string) {
        return EmitWithEventIdString(env, argc, argv);
    }
    return EmitWithEventIdUint32(env, argc, argv);
}

napi_value CreateJsUndefined(napi_env env)
{
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value CreateJsNumber(napi_env env, uint32_t value)
{
    napi_value result = nullptr;
    napi_create_uint32(env, value, &result);
    return result;
}

napi_value JS_GetListenerCount(napi_env env, napi_callback_info cbinfo)
{
    HILOGD("JS_GetListenerCount enter");
    size_t argc = ARGC_NUM;
    napi_value argv[ARGC_NUM] = {0};
    NAPI_CALL(env, napi_get_cb_info(env, cbinfo, &argc, argv, NULL, NULL));
    if (argc < ARGC_ONE) {
        HILOGE("Requires more than 1 parameter");
        return CreateJsUndefined(env);
    }

    napi_valuetype eventValueType;
    napi_typeof(env, argv[0], &eventValueType);
    if (eventValueType != napi_number && eventValueType != napi_string) {
        HILOGE("Type mismatch for parameter 1");
        return CreateJsUndefined(env);
    }

    InnerEvent::EventId eventId = 0u;
    bool ret = GetEventIdWithNumberOrString(env, argv[0], eventValueType, eventId);
    if (!ret) {
        HILOGE("Event id is empty for parameter 1");
        return CreateJsUndefined(env);
    }

    uint32_t cnt = AsyncCallbackManager::GetInstance().GetListenerCountByEventId(eventId);
    return CreateJsNumber(env, cnt);
}

napi_value EnumEventClassConstructor(napi_env env, napi_callback_info info)
{
    napi_value thisArg = nullptr;
    void *data = nullptr;

    napi_get_cb_info(env, info, nullptr, nullptr, &thisArg, &data);

    napi_value global = nullptr;
    napi_get_global(env, &global);

    return thisArg;
}

napi_value CreateEnumEventPriority(napi_env env, napi_value exports)
{
    napi_value immediate = nullptr;
    napi_value high = nullptr;
    napi_value low = nullptr;
    napi_value idle = nullptr;

    napi_create_uint32(env, static_cast<uint32_t>(Priority::IMMEDIATE), &immediate);
    napi_create_uint32(env, static_cast<uint32_t>(Priority::HIGH), &high);
    napi_create_uint32(env, static_cast<uint32_t>(Priority::LOW), &low);
    napi_create_uint32(env, static_cast<uint32_t>(Priority::IDLE), &idle);

    napi_property_descriptor desc[] = {
        DECLARE_NAPI_STATIC_PROPERTY("IMMEDIATE", immediate),
        DECLARE_NAPI_STATIC_PROPERTY("HIGH", high),
        DECLARE_NAPI_STATIC_PROPERTY("LOW", low),
        DECLARE_NAPI_STATIC_PROPERTY("IDLE", idle),
    };
    napi_value result = nullptr;
    napi_define_class(env, "EventPriority", NAPI_AUTO_LENGTH, EnumEventClassConstructor, nullptr,
        sizeof(desc) / sizeof(*desc), desc, &result);

    napi_set_named_property(env, exports, "EventPriority", result);

    return exports;
}

void ProcessEvent(const InnerEvent::Pointer& event)
{
    AsyncCallbackManager::GetInstance().DoCallback(event);
}

void AgentInit()
{
    EmitterEnhancedApi api = {
        .JS_Off = &JS_Off,
        .JS_Emit = &JS_Emit,
        .JS_GetListenerCount = &JS_GetListenerCount,
        .ProcessEvent = &ProcessEvent
    };

    GetEmitterEnhancedApiRegister().Register(api);
}
} // namespace AppExecFwk
} // namespace OHOS