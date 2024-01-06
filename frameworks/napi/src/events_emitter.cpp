/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "events_emitter.h"

#include <iterator>
#include <memory>
#include <mutex>
#include <uv.h>

#include "event_logger.h"
#include "js_native_api_types.h"
#include "napi/native_node_api.h"

using namespace std;
namespace OHOS {
namespace AppExecFwk {
namespace {
    DEFINE_EH_HILOG_LABEL("EventsEmitter");
    constexpr static uint32_t ARGC_ONE = 1u;
}
    static std::mutex emitterInsMutex;
    static map<InnerEvent::EventId, std::vector<AsyncCallbackInfo *>> emitterInstances;
    std::shared_ptr<EventHandlerInstance> eventHandler;

    AsyncCallbackInfo::~AsyncCallbackInfo()
    {
        env = nullptr;
    }
    EventHandlerInstance::EventHandlerInstance(const std::shared_ptr<EventRunner>& runner): EventHandler(runner)
    {
        HILOGI("EventHandlerInstance constructed");
    }
    EventHandlerInstance::~EventHandlerInstance()
    {
        HILOGI("EventHandlerInstance de-constructed");
    }
    std::shared_ptr<EventHandlerInstance> EventHandlerInstance::GetInstance()
    {
        static auto runner = EventRunner::Create("OS_eventsEmtr");
        if (runner.get() == nullptr) {
            HILOGE("failed to create EventRunner events_emitter");
            return nullptr;
        }
        static auto instance = std::make_shared<EventHandlerInstance>(runner);
        return instance;
    }

    void ProcessCallback(const EventDataWorker* eventDataInner)
    {
        HILOGD("enter");
        if (eventDataInner == nullptr) {
            HILOGW("EventDataWorkder instance(uv_work_t) is nullptr");
            return;
        }
        AsyncCallbackInfo* callbackInner = eventDataInner->callbackInfo;
        if (callbackInner->isDeleted) {
            HILOGD("ProcessEvent isDeleted");
            std::lock_guard<std::mutex> lock(emitterInsMutex);
            napi_value callback = nullptr;
            AsyncCallbackInfo* nativeCallback = nullptr;
            napi_get_reference_value(callbackInner->env, callbackInner->callback, &callback);
            napi_remove_wrap(callbackInner->env, callback, (void**)&nativeCallback);
            if (callbackInner->callback != nullptr) {
                napi_delete_reference(callbackInner->env, callbackInner->callback);
                callbackInner->callback = nullptr;
            }
        } else {
            napi_value resultData = nullptr;
            if (eventDataInner->data != nullptr) {
                if (napi_deserialize(callbackInner->env, *(eventDataInner->data), &resultData) != napi_ok ||
                    resultData == nullptr) {
                    HILOGE("Deserialize fail.");
                    return;
                }
            }
            napi_value event = nullptr;
            napi_create_object(callbackInner->env, &event);
            napi_set_named_property(callbackInner->env, event, "data", resultData);
            napi_value callback = nullptr;
            napi_value returnVal = nullptr;
            napi_get_reference_value(callbackInner->env, callbackInner->callback, &callback);
            napi_call_function(callbackInner->env, nullptr, callback, 1, &event, &returnVal);
            if (callbackInner->once) {
                HILOGD("ProcessEvent delete once");
                std::lock_guard<std::mutex> lock(emitterInsMutex);
                callbackInner->isDeleted = true;
                napi_value callback = nullptr;
                AsyncCallbackInfo* nativeCallback = nullptr;
                napi_get_reference_value(callbackInner->env, callbackInner->callback, &callback);
                napi_remove_wrap(callbackInner->env, callback, (void**)&nativeCallback);
                napi_delete_reference(callbackInner->env, callbackInner->callback);
                callbackInner->callback = nullptr;
            }
        }
        callbackInner->processed = true;
    }

    void OutPutEventIdLog(const InnerEvent::EventId &eventId)
    {
        if (eventId.index() == TYPE_U32_INDEX) {
            HILOGD("Event id value:%{public}u", std::get<uint32_t>(eventId));
        } else {
            HILOGD("Event id value:%{public}s", std::get<std::string>(eventId).c_str());
        }
    }

    void ThreadSafeCallback(napi_env env, napi_value jsCallback, void* context, void* data)
    {
        napi_handle_scope scope;
        EventDataWorker* eventDataInner = static_cast<EventDataWorker*>(data);
        if (eventDataInner != nullptr && eventDataInner->callbackInfo != nullptr &&
            !eventDataInner->callbackInfo->isDeleted) {
            HILOGD("eventDataInner address: %{public}p", &eventDataInner);
            napi_open_handle_scope(eventDataInner->callbackInfo->env, &scope);
            ProcessCallback(eventDataInner);
            napi_close_handle_scope(eventDataInner->callbackInfo->env, scope);
        } else {
            eventDataInner->callbackInfo->processed = true;
            HILOGD("callback is delete.");
        }
        delete eventDataInner;
        eventDataInner = nullptr;
        data = nullptr;
    }

    void EventHandlerInstance::ProcessEvent([[maybe_unused]] const InnerEvent::Pointer& event)
    {
        HILOGF("ProcessEvent");
        InnerEvent::EventId eventId = event->GetInnerEventIdEx();
        OutPutEventIdLog(eventId);
        std::lock_guard<std::mutex> lock(emitterInsMutex);
        auto subscribe = emitterInstances.find(eventId);
        if (subscribe == emitterInstances.end()) {
            HILOGW("ProcessEvent has no callback");
            return;
        }
        auto& callbackInfos = subscribe->second;
        HILOGD("size = %{public}zu", callbackInfos.size());
        auto value = event->GetUniqueObject<napi_value>();
        std::shared_ptr<napi_value> eventData(value.release(), [this](napi_value* pData) {
            if (pData != nullptr && (*pData) != nullptr && deleteEnv != nullptr) {
                napi_delete_serialization_data(deleteEnv, *pData);
            }
        });
        for (auto iter = callbackInfos.begin(); iter != callbackInfos.end();) {
            AsyncCallbackInfo* callbackInfo = *iter;
            EventDataWorker* eventDataWorker = new (std::nothrow) EventDataWorker();
            if (!eventDataWorker) {
                HILOGE("new object failed");
                continue;
            }
            deleteEnv = callbackInfo->env;
            eventDataWorker->data = eventData;
            eventDataWorker->callbackInfo = callbackInfo;
            eventDataWorker->eventId = eventId;
            if (callbackInfo->once || callbackInfo->isDeleted) {
                HILOGD("once callback or isDeleted callback");
                iter = callbackInfos.erase(iter);
                if (callbackInfo->processed) {
                    delete callbackInfo;
                    callbackInfo = nullptr;
                    continue;
                }
            } else {
                ++iter;
            }
            napi_acquire_threadsafe_function(callbackInfo->tsfn);
            napi_call_threadsafe_function(callbackInfo->tsfn, eventDataWorker, napi_tsfn_nonblocking);
        }

        if (callbackInfos.empty()) {
            emitterInstances.erase(eventId);
            HILOGD("ProcessEvent delete the last callback");
        }
    }

    static void UpdateOnceFlag(AsyncCallbackInfo *callbackInfo, bool once)
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

    AsyncCallbackInfo *SearchCallbackInfo(napi_env env, const InnerEvent::EventId &eventIdValue, napi_value argv)
    {
        auto subscribe = emitterInstances.find(eventIdValue);
        if (subscribe == emitterInstances.end()) {
            return nullptr;
        }
        for (auto callbackInfo : subscribe->second) {
            napi_value callback = nullptr;
            if (callbackInfo->isDeleted) {
                continue;
            }
            if (callbackInfo->env != env) {
                continue;
            }
            napi_get_reference_value(callbackInfo->env, callbackInfo->callback, &callback);
            bool isEq = false;
            napi_strict_equals(env, argv, callback, &isEq);
            if (!isEq) {
                continue;
            }
            return callbackInfo;
        }
        return nullptr;
    }

    bool GetEventIdWithObjectOrString(
        napi_env env, napi_value argv, napi_valuetype eventValueType, InnerEvent::EventId &eventId)
    {
        if (eventValueType == napi_string) {
            size_t valueStrBufLength = 0;
            napi_get_value_string_utf8(env, argv, nullptr, NAPI_VALUE_STRING_LEN, &valueStrBufLength);
            auto valueCStr = std::make_unique<char[]>(NAPI_VALUE_STRING_LEN + 1);
            size_t valueStrLength = 0;
            napi_get_value_string_utf8(env, argv, valueCStr.get(), NAPI_VALUE_STRING_LEN, &valueStrLength);
            std::string id(valueCStr.get(), valueStrLength);
            if (id.empty()) {
                HILOGE("Event id is empty for argument 1.");
                return false;
            }
            eventId = id;
            HILOGD("Event id value:%{public}s", id.c_str());
        } else {
            bool hasEventId = false;
            napi_has_named_property(env, argv, "eventId", &hasEventId);
            if (!hasEventId) {
                HILOGE("Argument 1 does not have event id.");
                return false;
            }

            napi_value eventIdValue = nullptr;
            napi_get_named_property(env, argv, "eventId", &eventIdValue);
            uint32_t id = 0u;
            napi_get_value_uint32(env, eventIdValue, &id);
            eventId = id;
            HILOGD("Event id value:%{public}u", id);
        }
        return true;
    }

    void ThreadFinished(napi_env env, void* data, [[maybe_unused]] void* context)
    {
        HILOGF("ThreadFinished");
        AsyncCallbackInfo* callbackInfo = reinterpret_cast<AsyncCallbackInfo*>(data);
        if (callbackInfo != nullptr) {
            napi_release_threadsafe_function(callbackInfo->tsfn, napi_tsfn_release);
        }
    }

    napi_value OnOrOnce(napi_env env, napi_callback_info cbinfo, bool once)
    {
        size_t argc = ARGC_NUM;
        napi_value argv[ARGC_NUM] = {0};
        NAPI_CALL(env, napi_get_cb_info(env, cbinfo, &argc, argv, NULL, NULL));
        if (argc < ARGC_NUM) {
            HILOGE("requires 2 parameter");
            return nullptr;
        }

        napi_valuetype eventValueType;
        napi_typeof(env, argv[0], &eventValueType);
        if (eventValueType != napi_object && eventValueType != napi_string) {
            HILOGE("type mismatch for parameter 1");
            return nullptr;
        }

        napi_valuetype eventHandleType;
        napi_typeof(env, argv[1], &eventHandleType);
        if (eventHandleType != napi_function) {
            HILOGE("type mismatch for parameter 2");
            return nullptr;
        }

        InnerEvent::EventId eventIdValue = 0u;
        bool ret = GetEventIdWithObjectOrString(env, argv[0], eventValueType, eventIdValue);
        if (!ret) {
            return nullptr;
        }
        std::lock_guard<std::mutex> lock(emitterInsMutex);
        auto callbackInfo = SearchCallbackInfo(env, eventIdValue, argv[1]);
        if (callbackInfo != nullptr) {
            UpdateOnceFlag(callbackInfo, once);
        } else {
            callbackInfo = new (std::nothrow) AsyncCallbackInfo();
            if (!callbackInfo) {
                HILOGE("new object failed");
                return nullptr;
            }
            callbackInfo->env = env;
            callbackInfo->once = once;
            napi_create_reference(env, argv[1], 1, &callbackInfo->callback);
            napi_wrap(env, argv[1], callbackInfo, [](napi_env env, void* data, void* hint) {
                auto callbackInfo = reinterpret_cast<AsyncCallbackInfo*>(data);
                if (callbackInfo != nullptr && !callbackInfo->isDeleted) {
                    callbackInfo->isDeleted = true;
                    callbackInfo->processed = true;
                    callbackInfo->env = nullptr;
                }
            }, nullptr, nullptr);
            napi_value resourceName = nullptr;
            napi_create_string_utf8(env, "Call thread-safe function", NAPI_AUTO_LENGTH, &resourceName);
            napi_create_threadsafe_function(env, argv[1], nullptr, resourceName, 0, 1, callbackInfo, ThreadFinished,
                nullptr, ThreadSafeCallback, &(callbackInfo->tsfn));
            emitterInstances[eventIdValue].push_back(callbackInfo);
        }
        return nullptr;
    }

    bool GetEventIdWithNumberOrString(
        napi_env env, napi_value argv, napi_valuetype eventValueType, InnerEvent::EventId &eventId)
    {
        if (eventValueType == napi_string) {
            size_t valueStrBufLength = 0;
            napi_get_value_string_utf8(env, argv, nullptr, NAPI_VALUE_STRING_LEN, &valueStrBufLength);
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

    napi_value JS_On(napi_env env, napi_callback_info cbinfo)
    {
        HILOGD("enter");
        return OnOrOnce(env, cbinfo, false);
    }

    napi_value JS_Once(napi_env env, napi_callback_info cbinfo)
    {
        HILOGD("enter");
        return OnOrOnce(env, cbinfo, true);
    }

    napi_value JS_Off(napi_env env, napi_callback_info cbinfo)
    {
        HILOGD("enter");
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
        std::lock_guard<std::mutex> lock(emitterInsMutex);

        if (argc == ARGC_NUM) {
            napi_valuetype eventHandleType;
            napi_typeof(env, argv[1], &eventHandleType);
            if (eventHandleType != napi_function) {
                HILOGE("type mismatch for parameter 2");
                return nullptr;
            }

            auto callbackInfo = SearchCallbackInfo(env, eventId, argv[1]);
            if (callbackInfo != nullptr) {
                callbackInfo->isDeleted = true;
            }
            return nullptr;
        }

        auto subscribe = emitterInstances.find(eventId);
        if (subscribe != emitterInstances.end()) {
            for (auto callbackInfo : subscribe->second) {
                callbackInfo->isDeleted = true;
            }
        }
        return nullptr;
    }

    bool EmitWithEventData(napi_env env, napi_value argv, const InnerEvent::EventId &eventId, Priority priority)
    {
        HILOGD("enter");
        napi_valuetype dataType;
        napi_typeof(env, argv, &dataType);
        if (dataType != napi_object) {
            HILOGE("type mismatch for parameter 2");
            return false;
        }
        bool hasData = false;
        napi_has_named_property(env, argv, "data", &hasData);
        if (hasData) {
            napi_value data = nullptr;
            napi_get_named_property(env, argv, "data", &data);
            napi_value serializeData = nullptr;
            napi_status serializeResult = napi_ok;
            napi_value result = nullptr;
            napi_get_undefined(env, &result);
            serializeResult = napi_serialize(env, data, result, &serializeData);
            if (serializeResult != napi_ok || serializeData == nullptr) {
                HILOGE("Serialize fail.");
                return false;
            }
            OutPutEventIdLog(eventId);
            auto event = InnerEvent::Get(eventId, make_unique<napi_value>(serializeData));
            eventHandler->SendEvent(event, 0, priority);
            return true;
        }
        return false;
    }

    bool IsExistValidCallback(napi_env env, const InnerEvent::EventId &eventId)
    {
        std::lock_guard<std::mutex> lock(emitterInsMutex);
        auto subscribe = emitterInstances.find(eventId);
        if (subscribe == emitterInstances.end()) {
            HILOGW("JS_Emit has no callback");
            return false;
        }
        vector<AsyncCallbackInfo *> callbackInfo = subscribe->second;
        size_t callbackSize = callbackInfo.size();
        for (size_t i = 0; i < callbackSize; i++) {
            if (!callbackInfo[i]->isDeleted) {
                return true;
            }
        }
        return false;
    }

    napi_value EmitWithEventIdUint32(napi_env env, size_t argc, napi_value argv[])
    {
        InnerEvent::EventId eventId = 0u;
        bool hasEventId = false;
        napi_value value = nullptr;
        napi_has_named_property(env, argv[0], "eventId", &hasEventId);
        if (hasEventId == false) {
            HILOGE("Wrong argument 1 does not have event id.");
            return nullptr;
        }

        napi_get_named_property(env, argv[0], "eventId", &value);
        uint32_t id = 0u;
        napi_get_value_uint32(env, value, &id);
        eventId = id;
        HILOGD("Event id value:%{public}u", id);

        if (!IsExistValidCallback(env, eventId)) {
            HILOGE("Invalid callback");
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
            auto event = InnerEvent::Get(eventId, make_unique<EventData>());
            eventHandler->SendEvent(event, 0, priority);
        }
        return nullptr;
    }

    napi_value EmitWithEventIdString(napi_env env, size_t argc, napi_value argv[])
    {
        InnerEvent::EventId eventId = 0u;
        size_t valueStrBufLength = 0;
        napi_get_value_string_utf8(env, argv[0], nullptr, NAPI_VALUE_STRING_LEN, &valueStrBufLength);
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

        if (!IsExistValidCallback(env, eventId)) {
            HILOGE("Invalid callback");
            return nullptr;
        }

        Priority priority = Priority::LOW;
        if (argc < ARGC_NUM) {
            auto event = InnerEvent::Get(eventId, make_unique<EventData>());
            eventHandler->SendEvent(event, 0, priority);
            return nullptr;
        }

        bool hasPriority = false;
        napi_value value = nullptr;
        napi_has_named_property(env, argv[1], "priority", &hasPriority);
        if (!hasPriority) {
            if (!EmitWithEventData(env, argv[1], eventId, priority)) {
                auto event = InnerEvent::Get(eventId, make_unique<EventData>());
                eventHandler->SendEvent(event, 0, priority);
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
            auto event = InnerEvent::Get(eventId, make_unique<EventData>());
            eventHandler->SendEvent(event, 0, priority);
        }
        return nullptr;
    }

    napi_value JS_Emit(napi_env env, napi_callback_info cbinfo)
    {
        HILOGD("enter");
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

        napi_create_uint32(env, (uint32_t)Priority::IMMEDIATE, &immediate);
        napi_create_uint32(env, (uint32_t)Priority::HIGH, &high);
        napi_create_uint32(env, (uint32_t)Priority::LOW, &low);
        napi_create_uint32(env, (uint32_t)Priority::IDLE, &idle);

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
        HILOGD("enter");
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

        uint32_t cnt = 0u;
        InnerEvent::EventId eventId = 0u;
        bool ret = GetEventIdWithNumberOrString(env, argv[0], eventValueType, eventId);
        if (!ret) {
            HILOGE("Event id is empty for parameter 1.");
            return CreateJsUndefined(env);
        }
        std::lock_guard<std::mutex> lock(emitterInsMutex);
        auto subscribe = emitterInstances.find(eventId);
        if (subscribe != emitterInstances.end()) {
            for (auto callbackInfo : subscribe->second) {
                if (!callbackInfo->isDeleted) {
                    ++cnt;
                }
            }
        }
        return CreateJsNumber(env, cnt);
    }

    napi_value EmitterInit(napi_env env, napi_value exports)
    {
        HILOGD("enter");
        napi_property_descriptor desc[] = {
            DECLARE_NAPI_FUNCTION("on", JS_On),
            DECLARE_NAPI_FUNCTION("once", JS_Once),
            DECLARE_NAPI_FUNCTION("off", JS_Off),
            DECLARE_NAPI_FUNCTION("emit", JS_Emit),
            DECLARE_NAPI_FUNCTION("getListenerCount", JS_GetListenerCount),
        };
        NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));

        CreateEnumEventPriority(env, exports);

        eventHandler = EventHandlerInstance::GetInstance();
        return exports;
    }
}  // namespace AppExecFwk
}  // namespace OHOS
