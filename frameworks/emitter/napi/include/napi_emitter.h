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

#ifndef BASE_EVENTHANDLER_FRAMEWORKS_NAPI_EMITTER_H
#define BASE_EVENTHANDLER_FRAMEWORKS_NAPI_EMITTER_H

#include "event_queue.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AppExecFwk {
using Priority = EventQueue::Priority;

napi_value JS_Off(napi_env env, napi_callback_info cbinfo);
napi_value JS_Emit(napi_env env, napi_callback_info cbinfo);
void ProcessEvent(const InnerEvent::Pointer& event);

static inline napi_valuetype GetNapiType(napi_env env, napi_value param)
{
    napi_valuetype type;
    napi_typeof(env, param, &type);
    return type;
}
} // namespace AppExecFwk
} // namespace OHOS

#endif  // BASE_EVENTHANDLER_FRAMEWORKS_NAPI_EMITTER_H