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
#ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_EMITTER_H
#define BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_EMITTER_H

#include <memory>

#include "event_handler.h"
#include "event_runner.h"
#include "inner_event.h"

namespace OHOS {
namespace AppExecFwk {
class EventHandlerEmitter : public EventHandler {
public:
    DISALLOW_COPY_AND_MOVE(EventHandlerEmitter);
    EventHandlerEmitter(const std::shared_ptr<EventRunner>& runner);
    ~EventHandlerEmitter();
    static std::shared_ptr<EventHandlerEmitter> GetInstance();

    /**
     * Execute callback.
     *
     * @param event Event Emitted by user.
     */
    void ProcessEvent(const InnerEvent::Pointer& event) override;

private:
    EventHandlerEmitter() = default;
};
} // namespace AppExecFwk
} // namespace OHOS
#endif // BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_EMITTER_H