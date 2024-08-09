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

#include "emitter_ffi.h"
#include "emitter_log.h"
#include "emitter.h"
#include "cj_lambda.h"

using namespace OHOS::FFI;
using namespace OHOS::EventsEmitter;

extern "C" {
    CallbackImpl *CreateCallback(CEventCallback &callbackInfo)
    {
        auto onChange = [lambda = CJLambda::Create(callbackInfo.callbackRef)](const CEventData data) -> void {
            lambda(data);
        };
        CallbackImpl *callbackImpl = new CallbackImpl(std::string(callbackInfo.name), onChange);
        if (callbackImpl == nullptr) {
            LOGE("Fail to create CallbackImpl.")
            return nullptr;
        }
        return callbackImpl;
    }

    int32_t CJ_OnWithId(uint32_t eventId, CEventCallback callbackInfo)
    {
        CallbackImpl *callback = CreateCallback(callbackInfo);
        if (callback == nullptr) {
            return MEMORY_ERROR;
        }
        return Emitter::On(eventId, callback);
    }

    int32_t CJ_OnWithStringId(char* eventId, CEventCallback callbackInfo)
    {
        CallbackImpl *callback = CreateCallback(callbackInfo);
        if (callback == nullptr) {
            return MEMORY_ERROR;
        }
        return Emitter::On(eventId, callback);
    }

    int32_t CJ_OnceWithId(uint32_t eventId, CEventCallback callbackInfo)
    {
        CallbackImpl *callback = CreateCallback(callbackInfo);
        if (callback == nullptr) {
            return MEMORY_ERROR;
        }
        return Emitter::Once(eventId, callback);
    }

    int32_t CJ_OnceWithStringId(char* eventId, CEventCallback callbackInfo)
    {
        CallbackImpl *callback = CreateCallback(callbackInfo);
        if (callback == nullptr) {
            return MEMORY_ERROR;
        }
        return Emitter::Once(eventId, callback);
    }

    void CJ_OffWithId(uint32_t eventId)
    {
        Emitter::Off(eventId);
    }

    void CJ_OffWithString(char* eventId)
    {
        Emitter::Off(eventId);
    }

    int32_t CJ_OffWithIdCallback(uint32_t eventId, CEventCallback callbackInfo)
    {
        CallbackImpl *callback = CreateCallback(callbackInfo);
        if (callback == nullptr) {
            return MEMORY_ERROR;
        }
        Emitter::Off(eventId, callback);
        delete callback;
        return SUCCESS_CODE;
    }

    int32_t CJ_OffWithStringCallback(char* eventId, CEventCallback callbackInfo)
    {
        CallbackImpl *callback = CreateCallback(callbackInfo);
        if (callback == nullptr) {
            return MEMORY_ERROR;
        }
        Emitter::Off(eventId, callback);
        delete callback;
        return SUCCESS_CODE;
    }

    void CJ_EmitWithId(uint32_t eventId, uint32_t priority, CEventData data)
    {
        Emitter::Emit(eventId, priority, data);
    }

    void CJ_EmitWithString(char* eventId, uint32_t priority, CEventData data)
    {
        Emitter::Emit(eventId, priority, data);
    }

    uint32_t CJ_GetListenerCountById(uint32_t eventId)
    {
        auto ret = Emitter::GetListenerCount(eventId);
        return ret;
    }

    uint32_t CJ_GetListenerCountByString(char* eventId)
    {
        auto ret = Emitter::GetListenerCount(std::string(eventId));
        return ret;
    }
}
