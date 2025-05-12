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
#ifndef BASE_EVENTHANDLER_FRAMEWORKS_NAPI_ASYNC_CALLBACK_MANAGER_H
#define BASE_EVENTHANDLER_FRAMEWORKS_NAPI_ASYNC_CALLBACK_MANAGER_H

#include <map>
#include <mutex>
#include <memory>
#include <unordered_set>

#include "nocopyable.h"
#include "inner_event.h"
#include "napi/native_api.h"
#include "js_native_api_types.h"
#include "napi/native_node_api.h"
#include "serialize.h"

namespace OHOS {
namespace AppExecFwk {

struct NapiAsyncCallbackInfo : public std::enable_shared_from_this<NapiAsyncCallbackInfo> {
    std::atomic<napi_env> env;
    napi_ref callback = 0;
    napi_threadsafe_function tsfn = nullptr;
    std::atomic<bool> once = false;
    std::atomic<bool> isDeleted = false;
    InnerEvent::EventId eventId;

    ~NapiAsyncCallbackInfo();
    void ProcessEvent([[maybe_unused]] const InnerEvent::Pointer& event);
};

struct EventDataWorker {
    std::shared_ptr<SerializeData> serializeData;
    std::shared_ptr<NapiAsyncCallbackInfo> callbackInfo;
};

class NapiAsyncCallbackManager {

public:
    /**
     * Delete all callback info of given event id.
     *
     * @param eventIdValue event id.
     */
    void NapiDeleteCallbackInfoByEventId(const InnerEvent::EventId &eventIdValue);

    /**
     * Get all callback info counts of given event id.
     *
     * @param eventId event id.
     * @return Counts of callback info.
     */
    uint32_t NapiGetListenerCountByEventId(const InnerEvent::EventId &eventId);

    /**
     * Find whether exists valid callback.
     *
     * @param eventId event id.
     * @return Returns true if exists valid callback.
     */
    bool NapiIsExistValidCallback(const InnerEvent::EventId &eventId);

    /**
     * Insert callback.
     *
     * @param env A pointer to the environment structure.
     * @param eventIdValue Event id.
     * @param argv Event's callback.
     * @param once Whether subscribe once. if true, subscribe once.
     */
    napi_value NapiInsertCallbackInfo(
        napi_env env, const InnerEvent::EventId &eventIdValue, napi_value argv, bool once);

    /**
     * Delete callback of given event id and callback object.
     *
     * @param env A pointer to the environment structure.
     * @param eventIdValue Event id.
     * @param argv Event's callback.
     */
    void NapiDeleteCallbackInfo(napi_env env, const InnerEvent::EventId &eventIdValue, napi_value argv);

    /**
     * Execute callback.
     *
     * @param event Event Emitted by user.
     */
    void NapiDoCallback(const InnerEvent::Pointer& event);

private:
    std::unordered_set<std::shared_ptr<NapiAsyncCallbackInfo>> NapiGetAsyncCallbackInfo(
        const InnerEvent::EventId &eventId);
    static void NapiReleaseCallbackInfo(NapiAsyncCallbackInfo* callbackInfo);
    static void NapiThreadFinished(napi_env env, void* data, [[maybe_unused]] void* context);
    static void NapiThreadSafeCallback(napi_env env, napi_value jsCallback, void* context, void* data);
    static void NapiProcessCallback(const EventDataWorker* eventDataInner);

private:
    std::mutex napiAsyncCallbackContainerMutex_;
    std::map<InnerEvent::EventId, std::unordered_set<std::shared_ptr<NapiAsyncCallbackInfo>>>
        napiAsyncCallbackContainer_;
};

} // namespace AppExecFwk
} // namespace OHOS

#endif // BASE_EVENTHANDLER_FRAMEWORKS_NAPI_ASYNC_CALLBACK_MANAGER_H