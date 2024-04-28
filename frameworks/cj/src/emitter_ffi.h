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

#ifndef COMMON_EVENT_MANAGER_FFI_H
#define COMMON_EVENT_MANAGER_FFI_H

#include "ffi_remote_data.h"
#include "cj_common_ffi.h"
#include "emitter_common.h"

using CEventData = OHOS::EventsEmitter::CEventData;

extern "C" {
    struct CEventCallback {
        char* name;
        void (*callbackRef)(const CEventData data);
    };

    // CommonEventManager ffi apis
    FFI_EXPORT int32_t CJ_OnWithId(uint32_t eventId, CEventCallback callbackInfo);
    FFI_EXPORT int32_t CJ_OnWithStringId(char* eventId, CEventCallback callbackInfo);
    FFI_EXPORT int32_t CJ_OnceWithId(uint32_t eventId, CEventCallback callbackInfo);
    FFI_EXPORT int32_t CJ_OnceWithStringId(char* eventId, CEventCallback callbackInfo);
    FFI_EXPORT void CJ_OffWithId(uint32_t eventId);
    FFI_EXPORT void CJ_OffWithString(char* eventId);
    FFI_EXPORT int32_t CJ_OffWithIdCallback(uint32_t eventId, CEventCallback callbackInfo);
    FFI_EXPORT int32_t CJ_OffWithStringCallback(char* eventId, CEventCallback callbackInfo);
    FFI_EXPORT void CJ_EmitWithId(uint32_t eventId, uint32_t priority, CEventData data);
    FFI_EXPORT void CJ_EmitWithString(char* eventId, uint32_t priority, CEventData data);
    FFI_EXPORT uint32_t CJ_GetListenerCountById(uint32_t eventId);
    FFI_EXPORT uint32_t CJ_GetListenerCountByString(char* eventId);
}


#endif