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

#include "composite_event.h"
#include "nocopyable.h"
#include "inner_event.h"
#include "napi/native_api.h"
#include "js_native_api_types.h"
#include "napi/native_node_api.h"
#include "serialize.h"
#include "interops.h"

namespace OHOS {
namespace AppExecFwk {

struct NapiEventDataWorker {
    std::shared_ptr<SerializeData> serializeData;
    std::shared_ptr<AsyncCallbackInfo> callbackInfo;
    int type;
};

class NapiAsyncCallbackManager {
using AsyncCallbackInfoContainer =
    std::map<CompositeEventId, std::unordered_set<std::shared_ptr<AsyncCallbackInfo>>>;
public:
    NapiAsyncCallbackManager(
        std::mutex& containerMutex = GetAsyncCallbackInfoContainerMutex(),
        AsyncCallbackInfoContainer& callbackInfoContainer = GetAsyncCallbackInfoContainer()
    ) : napiAsyncCallbackContainerMutex_(containerMutex), napiAsyncCallbackContainer_(callbackInfoContainer) {}

    /**
     * Delete all callback info of given event id.
     *
     * @param compositeId composite event id.
     */
    void NapiDeleteCallbackInfoByEventId(const CompositeEventId &compositeId);

    /**
     * Get all callback info counts of given event id.
     *
     * @param compositeId composite event id.
     * @return Counts of callback info.
     */
    uint32_t NapiGetListenerCountByEventId(const CompositeEventId &compositeId);

    /**
     * Find whether exists valid callback.
     *
     * @param compositeId composite event id.
     * @return Returns true if exists valid callback.
     */
    bool NapiIsExistValidCallback(const CompositeEventId &compositeId);

    /**
     * Delete callback of given event id and callback object.
     *
     * @param env A pointer to the environment structure.
     * @param  compositeId composite event id.
     * @param argv Event's callback.
     */
    void NapiDeleteCallbackInfo(napi_env env, const CompositeEventId &compositeId, napi_value argv);

    /**
     * Execute callback.
     *
     * @param event Event Emitted by user.
     */
    void NapiDoCallback(const InnerEvent::Pointer& event);

private:
    std::unordered_set<std::shared_ptr<AsyncCallbackInfo>> NapiGetAsyncCallbackInfo(
        const CompositeEventId &compositeId);
private:
    std::mutex& napiAsyncCallbackContainerMutex_;
    AsyncCallbackInfoContainer& napiAsyncCallbackContainer_;
};

} // namespace AppExecFwk
} // namespace OHOS

#endif // BASE_EVENTHANDLER_FRAMEWORKS_NAPI_ASYNC_CALLBACK_MANAGER_H
