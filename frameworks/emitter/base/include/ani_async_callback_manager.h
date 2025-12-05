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

#ifndef BASE_EVENTHANDLER_FRAMEWORKS_ANI_ASYNC_CALLBACK_MANAGER_H
#define BASE_EVENTHANDLER_FRAMEWORKS_ANI_ASYNC_CALLBACK_MANAGER_H

#include <map>
#include <mutex>
#include <memory>
#include <unordered_set>
#include <queue>
#include <thread>
#include <condition_variable>

#include "ani.h"

#include "concurrency_helpers.h"
#include "ani.h"
#include "composite_event.h"
#include "inner_event.h"
#include "serialize.h"

namespace OHOS {
namespace AppExecFwk {
using arkts::concurrency_helpers::GetWorkerId;
using arkts::concurrency_helpers::SendEvent;

struct AniCallbackInfo {
    ani_vm* vm = nullptr;
    ani_ref callback = 0;
    std::string dataType;
    std::shared_ptr<SerializeData> serializeData;
};

struct AniAsyncCallbackInfo {
    ani_vm* vm = nullptr;
    std::string dataType;
    ani_ref callback = 0;
    std::atomic<bool> once = false;
    std::atomic<bool> isDeleted = false;
    InnerEvent::EventId eventId;
    uint32_t emitterId = 0;
    arkts::concurrency_helpers::AniWorkerId workId;
    ~AniAsyncCallbackInfo();
    static ani_status GetCallbackArgs(
        ani_env *env, std::string& dataType, std::vector<ani_ref>& args, std::shared_ptr<SerializeData> serializeData);
};

struct AniTaskData {
    std::shared_ptr<AniAsyncCallbackInfo> cb_ptr;
    std::shared_ptr<SerializeData> serializeData;
};

class AniAsyncCallbackManager {
public:
    /**
     * Delete all callback info of given event id.
     *
     * @param compositeId Composite event id.
     */
    void AniDeleteCallbackInfoByEventId(const CompositeEventId &compositeId);

    /**
     * Get all callback info counts of given event id.
     *
     * @param compositeId Composite event id.
     * @return Counts of callback info.
     */
    uint32_t AniGetListenerCountByEventId(const CompositeEventId &compositeId);

    /**
     * Find whether exists valid callback.
     *
     * @param compositeId Composite event id.
     * @return Returns true if exists valid callback.
     */
    bool AniIsExistValidCallback(const CompositeEventId &compositeId);

    /**
     * Insert callback.
     *
     * @param env A pointer to the environment structure.
     * @param compositeId Composite event id.
     * @param once Whether subscribe once. if true, subscribe once.
     * @param callback Event's callback.
     * @param dataType Data type of callback's parameter.
     */
    void AniInsertCallbackInfo(
        ani_env *env, CompositeEventId compositeId, bool once, ani_ref callback, ani_string dataType);

    /**
     * Delete callback of given event id and callback object.
     *
     * @param env A pointer to the environment structure.
     * @param compositeId Composite event id.
     * @param callback Event's callback.
     */
    void AniDeleteCallbackInfo(ani_env *env, const CompositeEventId &compositeId, ani_ref callback);

    /**
     * Execute callback.
     *
     * @param event Event Emitted by user.
     */
    void AniDoCallback(const InnerEvent::Pointer& event);

    /**
     * Delete callback of given instance and callback object.
     *
     * @param compositeId Composite event id.
     */
    void AniCleanCallbackInfo(const CompositeEventId &compositeId);
private:
    std::unordered_set<std::shared_ptr<AniAsyncCallbackInfo>> AniGetAsyncCallbackInfo(
        const CompositeEventId &compositeId);
    static void AniReleaseCallbackInfo(AniAsyncCallbackInfo* callbackInfo);
    std::string AniGetStdString(ani_env *env, ani_string str);
    void PushTaskData(const std::shared_ptr<AniTaskData>& data);
    std::shared_ptr<AniTaskData> PopTaskData();
    void WorkTheadFunction(ani_vm* vm);
    void SendEventData(ani_env *env, arkts::concurrency_helpers::AniWorkerId workId, void* data);

private:
    std::mutex aniAsyncCallbackContainerMutex_;
    std::map<CompositeEventId, std::unordered_set<std::shared_ptr<AniAsyncCallbackInfo>>>
        aniAsyncCallbackContainer_;
    std::mutex taskMutex_;
    std::queue<std::shared_ptr<AniTaskData>> taskData_;
    std::atomic<bool> taskRunning_{false};
    bool needNotify_{false};
    std::condition_variable taskCond_;
    std::thread workerThread_;
};
} // namespace AppExecFwk
} // namespace OHOS
#endif // BASE_EVENTHANDLER_FRAMEWORKS_ANI_ASYNC_CALLBACK_MANAGER_H
