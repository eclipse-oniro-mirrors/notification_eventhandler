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

#include "napi_serialize.h"
#include "event_logger.h"
#include "napi/native_node_hybrid_api.h"

namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
}
bool NapiSerialize::PeerSerialize(napi_env env, napi_value argv, std::shared_ptr<SerializeData> serializeData)
{
    bool hasData = false;
    void* result = nullptr;
    napi_has_named_property(env, argv, "data", &hasData);
    if (hasData) {
        napi_value data = nullptr;
        napi_get_named_property(env, argv, "data", &data);
        napi_value undefined = nullptr;
        napi_get_undefined(env, &undefined);
        if (napi_serialize_hybrid(env, data, undefined, undefined, &result) != napi_ok) {
            HILOGE("PeerSerialize failed to napi_serialize_hybrid");
            return false;
        }
    }
    serializeData->peerData = reinterpret_cast<napi_value>(result);
    return true;
}
} // namespace AppExecFwk
} // namespace OHOS