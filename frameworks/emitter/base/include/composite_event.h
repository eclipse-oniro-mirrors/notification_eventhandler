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
#ifndef BASE_EVENTHANDLER_FRAMEWORKS_COMPOSITE_EVENT_H
#define BASE_EVENTHANDLER_FRAMEWORKS_COMPOSITE_EVENT_H

#include <string>
#include <variant>

namespace OHOS {
namespace AppExecFwk {
struct CompositeEventId {
    std::variant<uint32_t, std::string> eventId;
    uint32_t emitterId = 0;

    CompositeEventId() = default;
    CompositeEventId(uint32_t id) : eventId(id), emitterId(0) {}
    CompositeEventId(const std::string& id) : eventId(id), emitterId(0) {}
    CompositeEventId(const char* id) : eventId(std::string(id)), emitterId(0) {}
    CompositeEventId(uint32_t id, uint32_t emitter) : eventId(id), emitterId(emitter) {}
    CompositeEventId(const std::string& id, uint32_t emitter) : eventId(id), emitterId(emitter) {}
    CompositeEventId(const char* id, uint32_t emitter) : eventId(std::string(id)), emitterId(emitter) {}
    bool operator<(const CompositeEventId& other) const
    {
        if (emitterId != other.emitterId) {
            return emitterId < other.emitterId;
        }

        if (eventId.index() != other.eventId.index()) {
            return eventId.index() < other.eventId.index();
        }

        if (eventId.index() == 0) {
            return std::get<uint32_t>(eventId) < std::get<uint32_t>(other.eventId);
        } else {
            return std::get<std::string>(eventId) < std::get<std::string>(other.eventId);
        }
    }

    bool operator!=(const CompositeEventId& other) const
    {
        return emitterId != other.emitterId || eventId != other.eventId;
    }
};

} // namespace AppExecFwk
} // namespace OHOS

#endif // BASE_EVENTHANDLER_FRAMEWORKS_COMPOSITE_EVENT_H

