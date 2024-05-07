/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "event_handler_impl.h"
#include "emitter_log.h"

namespace OHOS::EventsEmitter {
    EventHandlerImpl::EventHandlerImpl(const std::shared_ptr<EventRunner>& runner): EventHandler(runner)
    {
        LOGI("EventHandlerImpl constructed");
    }

    EventHandlerImpl::~EventHandlerImpl()
    {
        LOGI("EventHandlerImpl de-constructed");
    }

    std::shared_ptr<EventHandlerImpl> EventHandlerImpl::GetEventHandler()
    {
        static auto runner = EventRunner::Create("OS_eventsEmtr");
        if (runner.get() == nullptr) {
            LOGE("failed to create EventRunner events_emitter");
            return nullptr;
        }
        static auto instance = std::make_shared<EventHandlerImpl>(runner);
        return instance;
    }
}