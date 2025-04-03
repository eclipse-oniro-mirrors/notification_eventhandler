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

#ifndef STS_EVENTS_EMITTER_H
#define STS_EVENTS_EMITTER_H

#include <memory>
#include <unordered_set>
#include "ani.h"
#include "../../../interfaces/inner_api/inner_event.h"
#include "event_queue.h"
#include "event_handler.h"

namespace OHOS {
namespace AppExecFwk {

using Priority = EventQueue::Priority;
using EventDataAni = std::shared_ptr<ani_object>;
struct AniAsyncCallbackInfo {
    ani_env* env;
    std::atomic<bool> once = false;
    std::atomic<bool> isDeleted = false;
    ani_object data = nullptr;
    std::string dataType;
    ani_ref callback = 0;
    InnerEvent::EventId eventId;
    ~AniAsyncCallbackInfo();
};

class EventsEmitter {
public:
    static std::string GetStdString(ani_env *env, ani_string str);
    static std::shared_ptr<AniAsyncCallbackInfo> SearchCallbackInfo(
        ani_env *env, const InnerEvent::EventId &eventIdValue, ani_ref callback);
    static void ReleaseCallbackInfo(ani_env *env, AniAsyncCallbackInfo* callbackInfo);
    static void UpdateOnceFlag(std::shared_ptr<AniAsyncCallbackInfo>callbackInfo, bool once);
    static void DeleteCallbackInfo(ani_env *env, const InnerEvent::EventId &eventIdValue, ani_ref callback);
    static void OnOrOnce(ani_env *env, InnerEvent::EventId eventId, bool once, ani_ref callback, ani_string dataType);
    static void OffEmitterInstances(InnerEvent::EventId eventId);
    static void AniWrap(ani_env *env, ani_ref callback);
    static ani_double GetListenerCount(InnerEvent::EventId eventId);
    static bool IsExistValidCallback(const InnerEvent::EventId &eventId, ani_object eventData);
    static void EmitWithEventId(ani_env *env, ani_object innerEvent, ani_object eventData);
    static void EmitWithEventIdString(
       ani_env *env, ani_string eventId, ani_object eventData, ani_enum_item enumItem);
    static void ThreadFunction(ani_env* env, ani_ref callback, ani_object data, std::string dataType);
};

class EventsEmitterInstance : public EventHandler {
public:
    EventsEmitterInstance(const std::shared_ptr<EventRunner>& runner);
    static std::shared_ptr<EventsEmitterInstance> GetInstance();
    void ProcessEvent(const InnerEvent::Pointer& event) override;
    std::unordered_set<std::shared_ptr<AniAsyncCallbackInfo>> GetAsyncCallbackInfo(const InnerEvent::EventId &eventId);
    ~EventsEmitterInstance();
};
}
}

#endif // STS_EVENTS_EMITTER_H