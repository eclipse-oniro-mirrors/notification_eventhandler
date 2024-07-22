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

#ifndef EMITTER_H
#define EMITTER_H

#include "emitter_log.h"
#include "emitter_common.h"

namespace OHOS::EventsEmitter {
class CallbackImpl {
public:
    CallbackImpl(std::string name, std::function<void(CEventData)> callback);

    std::string name;
    std::function<void(CEventData)> callback = nullptr;
};

struct CallbackInfo {
    std::atomic<bool> once = false;
    std::atomic<bool> isDeleted = false;
    std::atomic<bool> processed = false;
    CallbackImpl *callbackImpl = nullptr;
    ~CallbackInfo();
};

class Emitter {
public:
    static int32_t On(uint32_t eventId, CallbackImpl *callback);

    static int32_t On(char* eventId, CallbackImpl *callback);

    static int32_t Once(uint32_t eventId, CallbackImpl *callback);

    static int32_t Once(char* eventId, CallbackImpl *callback);

    static void Off(uint32_t eventId);

    static void Off(char* eventId);

    static void Off(uint32_t eventId, CallbackImpl *callback);

    static void Off(char* eventId, CallbackImpl *callback);

    static void Emit(uint32_t eventId, uint32_t priority, CEventData data);

    static void Emit(char* eventId, uint32_t priority, CEventData data);

    static uint32_t GetListenerCount(uint32_t eventId);

    static uint32_t GetListenerCount(std::string eventId);
};
}

#endif