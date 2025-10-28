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

#include "ani_async_callback_manager.h"
#include "event_logger.h"
#include "ani_deserialize.h"

namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
constexpr const char* EVENT_DATA = "eventData";
constexpr const char* GENERIC_EVENT_DATA = "genericEventData";
}

AniAsyncCallbackInfo::~AniAsyncCallbackInfo()
{
    vm = nullptr;
}

void AniAsyncCallbackInfo::ProcessEvent([[maybe_unused]] const InnerEvent::Pointer& event)
{
    if (vm == nullptr) {
        return;
    }
    serializeData = event->GetSharedObject<SerializeData>();
    auto t = std::thread(ThreadFunction, this);
    t.join();
    if (once) {
        isDeleted = true;
    }
}

ani_status AniAsyncCallbackInfo::GetCallbackArgs(
    ani_env *env, std::string& dataType, std::vector<ani_ref>& args, std::shared_ptr<SerializeData> serializeData)
{
    if (env == nullptr || serializeData == nullptr) {
        HILOGE("Error: has nullptr");
        return ANI_ERROR;
    }
    ani_ref eventData;
    bool isDeserializeSuccess = false;
    if (serializeData->envType == EnvType::ANI) {
        isDeserializeSuccess = AniDeserialize::PeerDeserialize(env, &eventData, serializeData);
    } else {
        if (serializeData->isCrossRuntime) {
            isDeserializeSuccess = AniDeserialize::CrossDeserialize(env, &eventData, serializeData);
        }
    }
    if (!isDeserializeSuccess) {
        return ANI_INVALID_ARGS;
    }
    ani_status status = ANI_OK;
    ani_class cls;
    if (dataType == EVENT_DATA) {
        status = env->FindClass("@ohos.events.emitter.emitter.EventDataInner", &cls);
    } else if (dataType == GENERIC_EVENT_DATA) {
        status = env->FindClass("@ohos.events.emitter.emitter.GenericEventDataInner", &cls);
    } else {
        status = ANI_ERROR;
    }
    if (status != ANI_OK) {
        HILOGE("threadFunciton FindClass error%{public}d", status);
        return status;
    }
    ani_method ctor;
    status = env->Class_FindMethod(cls, "<ctor>", ":", &ctor);
    if (status != ANI_OK) {
        HILOGE("threadFunciton Class_FindMethod error%{public}d", status);
        return status;
    }
    ani_object obj;
    status = env->Object_New(cls, ctor, &obj);
    if (status != ANI_OK) {
        HILOGE("threadFunciton Object_New error%{public}d", status);
        return status;
    }
    env->Object_SetPropertyByName_Ref(obj, "data", eventData);
    args.push_back(reinterpret_cast<ani_ref>(obj));
    return status;
}

void AniAsyncCallbackInfo::ThreadFunction(AniAsyncCallbackInfo *asyncCallbackInfo)
{
    ani_env *env;
    ani_status status = ANI_OK;
    ani_option interopEnabled {"--interop=enable", nullptr};
    ani_options aniArgs {1, &interopEnabled};
    status = asyncCallbackInfo->vm->AttachCurrentThread(&aniArgs, ANI_VERSION_1, &env);
    if (ANI_OK != status) {
        HILOGE("vm GetEnv error %{public}d", status);
        return;
    }
    if (asyncCallbackInfo == nullptr || env == nullptr) {
        HILOGE("Error: has nullptr");
        return;
    }
    auto res = arkts::concurrency_helpers::SendEvent(env, asyncCallbackInfo->workId,
        [](void *data) {
            AniAsyncCallbackInfo* info = reinterpret_cast<AniAsyncCallbackInfo*>(data);
            ani_env *envCurr = nullptr;
            info->vm->GetEnv(ANI_VERSION_1, &envCurr);
            auto fnObj = reinterpret_cast<ani_fn_object>(info->callback);
            if (fnObj == nullptr) {
                HILOGE("threadFunciton fnObj is nullptr");
                return;
            }
            std::vector<ani_ref> args;
            ani_ref result;
            auto status = AniAsyncCallbackInfo::GetCallbackArgs(envCurr, info->dataType, args, info->serializeData);
            if (status != ANI_OK) {
                HILOGI("Get callback args failed. error %{public}d", status);
                return;
            }
            status = envCurr->FunctionalObject_Call(fnObj, args.size(), args.data(), &result);
            if (ANI_OK != status) {
                HILOGI("ANI call function failed. error %{public}d", status);
            }
        }, reinterpret_cast<void *>(asyncCallbackInfo));
    if (res != arkts::concurrency_helpers::WorkStatus::OK) {
        HILOGI("ANI SendEvent failed. error %{public}d", res);
    }
    asyncCallbackInfo->vm->DetachCurrentThread();
}

void AniAsyncCallbackManager::AniDeleteCallbackInfoByEventId(const InnerEvent::EventId &eventIdValue)
{
    std::lock_guard<std::mutex> lock(aniAsyncCallbackContainerMutex_);
    auto iter = aniAsyncCallbackContainer_.find(eventIdValue);
    if (iter != aniAsyncCallbackContainer_.end()) {
        for (auto callbackInfo : iter->second) {
            callbackInfo->isDeleted = true;
        }
    }
    aniAsyncCallbackContainer_.erase(eventIdValue);
}

uint32_t AniAsyncCallbackManager::AniGetListenerCountByEventId(const InnerEvent::EventId &eventId)
{
    uint32_t cnt = 0u;
    std::lock_guard<std::mutex> lock(aniAsyncCallbackContainerMutex_);
    auto subscribe = aniAsyncCallbackContainer_.find(eventId);
    if (subscribe != aniAsyncCallbackContainer_.end()) {
        for (auto it = subscribe->second.begin(); it != subscribe->second.end();) {
            if ((*it)->isDeleted == true || (*it)->vm == nullptr) {
                it = subscribe->second.erase(it);
                continue;
            }
            ++it;
            ++cnt;
        }
    }
    return cnt;
}

bool AniAsyncCallbackManager::AniIsExistValidCallback(const InnerEvent::EventId &eventId)
{
    std::lock_guard<std::mutex> lock(aniAsyncCallbackContainerMutex_);
    auto subscribe = aniAsyncCallbackContainer_.find(eventId);
    if (subscribe == aniAsyncCallbackContainer_.end()) {
        return false;
    }
    if (subscribe->second.size() != 0) {
        return true;
    }
    return false;
}

void AniAsyncCallbackManager::AniInsertCallbackInfo(
    ani_env *env, InnerEvent::EventId eventId, bool once, ani_ref callback, ani_string dataType)
{
    std::lock_guard<std::mutex> lock(aniAsyncCallbackContainerMutex_);
    ani_ref globalRefCallback;
    env->GlobalReference_Create(callback, &globalRefCallback);
    auto subscriber = aniAsyncCallbackContainer_.find(eventId);
    if (subscriber != aniAsyncCallbackContainer_.end()) {
        for (auto callbackInfo : subscriber->second) {
            if (callbackInfo->isDeleted) {
                continue;
            }
            ani_boolean isEq = false;
            env->Reference_StrictEquals(globalRefCallback, callbackInfo->callback, &isEq);
            if (!isEq) {
                continue;
            }
            callbackInfo->once = once;
            return;
        }
    }
    auto callbackInfoPtr = new (std::nothrow) AniAsyncCallbackInfo();
    if (!callbackInfoPtr) {
        HILOGE("new object failed");
        return;
    }
    std::shared_ptr<AniAsyncCallbackInfo> callbackInfo(callbackInfoPtr, [](AniAsyncCallbackInfo* callbackInfo) {
        AniReleaseCallbackInfo(callbackInfo);
    });
    auto status = env->GetVM(&callbackInfo->vm);
    if (callbackInfo->vm == nullptr) {
        HILOGE("Get vm failed. status: %{public}d", status);
        return;
    }
    callbackInfo->workId = GetWorkerId(env);
    callbackInfo->once = once;
    callbackInfo->eventId = eventId;
    callbackInfo->callback = globalRefCallback;
    callbackInfo->dataType = AniGetStdString(env, dataType);
    aniAsyncCallbackContainer_[eventId].insert(callbackInfo);
}

void AniAsyncCallbackManager::AniDeleteCallbackInfo(
    ani_env *env, const InnerEvent::EventId &eventIdValue, ani_ref callback)
{
    std::lock_guard<std::mutex> lock(aniAsyncCallbackContainerMutex_);
    auto iter = aniAsyncCallbackContainer_.find(eventIdValue);
    if (iter == aniAsyncCallbackContainer_.end()) {
        return;
    }
    for (auto callbackInfo = iter->second.begin(); callbackInfo != iter->second.end();) {
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
}

std::unordered_set<std::shared_ptr<AniAsyncCallbackInfo>> AniAsyncCallbackManager::AniGetAsyncCallbackInfo(
    const InnerEvent::EventId &eventId)
{
    std::lock_guard<std::mutex> lock(aniAsyncCallbackContainerMutex_);
    auto iter = aniAsyncCallbackContainer_.find(eventId);
    if (iter == aniAsyncCallbackContainer_.end()) {
        std::unordered_set<std::shared_ptr<AniAsyncCallbackInfo>> result;
        return result;
    }
    for (auto it = iter->second.begin(); it != iter->second.end();) {
        if (*it == nullptr || (*it)->isDeleted == true || (*it)->vm == nullptr) {
            it = iter->second.erase(it);
            continue;
        }
        ++it;
    }
    return iter->second;
}

void AniAsyncCallbackManager::AniDoCallback(const InnerEvent::Pointer& event)
{
    auto aniCallbackInfos = AniGetAsyncCallbackInfo(event->GetInnerEventIdEx());
    for (auto it = aniCallbackInfos.begin(); it != aniCallbackInfos.end(); ++it) {
        (*it)->ProcessEvent(event);
    }
}

void AniAsyncCallbackManager::AniReleaseCallbackInfo(AniAsyncCallbackInfo* callbackInfo)
{
    if (callbackInfo != nullptr) {
        ani_env *MyEnv;
        if (ANI_OK != callbackInfo->vm->GetEnv(ANI_VERSION_1, &MyEnv)) {
            ani_status status = ANI_OK;
            ani_option interopEnabled {"--interop=disable", nullptr};
            ani_options aniArgs {1, &interopEnabled};
            status = callbackInfo->vm->AttachCurrentThread(&aniArgs, ANI_VERSION_1, &MyEnv);
            if (ANI_OK != status) {
                HILOGE("attach thread failed");
                delete callbackInfo;
                callbackInfo = nullptr;
                return;
            }
            status = MyEnv->GlobalReference_Delete(callbackInfo->callback);
            if (status != ANI_OK) {
                HILOGE("delete reference failed");
            }
            callbackInfo->vm->DetachCurrentThread();
            delete callbackInfo;
            callbackInfo = nullptr;
            return;
        }
        MyEnv->GlobalReference_Delete(callbackInfo->callback);
        delete callbackInfo;
        callbackInfo = nullptr;
    }
}

std::string AniAsyncCallbackManager::AniGetStdString(ani_env *env, ani_string str)
{
    std::string result;
    ani_size sz {};
    env->String_GetUTF8Size(str, &sz);
    result.resize(sz + 1);
    env->String_GetUTF8SubString(str, 0, sz, result.data(), result.size(), &sz);
    result.resize(sz);
    return result;
}

} // namespace AppExecFwk
} // namespace OHOS