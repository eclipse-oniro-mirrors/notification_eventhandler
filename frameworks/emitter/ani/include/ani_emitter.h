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

#ifndef BASE_EVENTHANDLER_FRAMEWORKS_ANI_EMITTER_H
#define BASE_EVENTHANDLER_FRAMEWORKS_ANI_EMITTER_H

#include <memory>
#include "inner_event.h"
#include "event_queue.h"
#include "ani.h"
#include "ani_serialize.h"

namespace OHOS {
namespace AppExecFwk {

using Priority = EventQueue::Priority;
using EventDataAni = std::shared_ptr<ani_object>;

class EventsEmitter {
public:
    /**
     * Convert ani_string to std::string.
     *
     * @param env A pointer to the environment structure.
     * @param str An ani_string to be converted.
     * @return Returns a std::string of input str.
     */
    static std::string GetStdString(ani_env *env, ani_string str);

    /**
     * Subscribe an event of given event id.
     *
     * @param env A pointer to the environment structure.
     * @param eventId Event id.
     * @param once Whether subscribe once. if true, subscribe once.
     * @param callback Event's callback.
     * @param dataType Data type of callback's parameter.
     */
    static void OnOrOnce(ani_env *env, InnerEvent::EventId eventId, bool once, ani_ref callback, ani_string dataType);

    /**
     * Unsubscribe all of given event id.
     *
     * @param eventId Event id.
     */
    static void OffEmitterInstances(InnerEvent::EventId eventId);

    /**
     * Get all listener counts of given event id.
     *
     * @param eventId Event id.
     * @return Returns all listener counts of given event id.
     */
    static ani_long GetListenerCount(InnerEvent::EventId eventId);

    /**
     * Emit an event of given event id.
     *
     * @param env A pointer to the environment structure.
     * @param innerEvent An event structure including event id.
     * @param eventData Data to be emitted.
     */
    static void EmitWithEventId(ani_env *env, ani_object innerEvent, ani_object eventData);

    /**
     * Emit an event of given event id.
     *
     * @param env A pointer to the environment structure.
     * @param eventId Event id.
     * @param eventData Data to be emitted.
     * @param enumItem Prority of event.
     */
    static void EmitWithEventIdString(ani_env *env, ani_string eventId, ani_object eventData, ani_enum_item enumItem);

private:
    static std::shared_ptr<SerializeData> GetSharedSerializeData(ani_env *env);
    static void ReleaseAniData(ani_env *env, SerializeData* data);
    static void ReleaseNapiData(SerializeData* data);
};

} // namespace AppExecFwk
} // namespace OHOS
#endif // BASE_EVENTHANDLER_FRAMEWORKS_ANI_EMITTER_H