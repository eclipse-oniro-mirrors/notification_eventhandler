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
#include "eventhandler_emitter.h"
#include "event_logger.h"
#include "aync_callback_manager.h"

namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
}

EventHandlerEmitter::EventHandlerEmitter(const std::shared_ptr<EventRunner>& runner): EventHandler(runner)
{
    HILOGI("EventHandlerEmitter constructed");
}

EventHandlerEmitter::~EventHandlerEmitter()
{
    HILOGI("EventHandlerEmitter de-constructed");
}

std::shared_ptr<EventHandlerEmitter> EventHandlerEmitter::GetInstance()
{
    static auto runner = EventRunner::Create("OS_eventsEmtr", ThreadMode::FFRT);
    if (runner.get() == nullptr) {
        HILOGE("failed to create EventRunner events_emitter");
        return nullptr;
    }
    static auto instance = std::make_shared<EventHandlerEmitter>(runner);
    return instance;
}

void EventHandlerEmitter::ProcessEvent([[maybe_unused]] const InnerEvent::Pointer& event)
{
    AsyncCallbackManager::GetInstance().DoCallback(event);
}
}
}