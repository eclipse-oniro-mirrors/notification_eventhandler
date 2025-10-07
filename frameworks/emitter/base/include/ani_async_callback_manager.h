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

#include "inner_event.h"
#include "ani.h"
#include "composite_event.h"
#include "serialize.h"

namespace OHOS {
namespace AppExecFwk {

struct AniAsyncCallbackInfo {
    ani_vm* vm = nullptr;
    std::string dataType;
    ani_ref callback = 0;
    std::atomic<bool> once = false;
    std::atomic<bool> isDeleted = false;
    InnerEvent::EventId eventId;
    uint32_t emitterId = 0;

    ~AniAsyncCallbackInfo();
    void ProcessEvent([[maybe_unused]] const InnerEvent::Pointer& event);
    static void ThreadFunction(
        ani_vm* vm, ani_ref callback, std::string dataType, std::shared_ptr<SerializeData> serializeData);
    static ani_status GetCallbackArgs(
        ani_env *env, std::string& dataType, std::vector<ani_ref>& args, std::shared_ptr<SerializeData> serializeData);
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

private:
    std::mutex aniAsyncCallbackContainerMutex_;
    std::map<CompositeEventId, std::unordered_set<std::shared_ptr<AniAsyncCallbackInfo>>>
        aniAsyncCallbackContainer_;
};
} // namespace AppExecFwk
} // namespace OHOS
#endif // BASE_EVENTHANDLER_FRAMEWORKS_ANI_ASYNC_CALLBACK_MANAGER_H
