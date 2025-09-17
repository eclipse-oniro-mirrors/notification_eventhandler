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

#include "ani_deserialize.h"
#include "event_logger.h"
#include "napi/native_node_hybrid_api.h"
#include "interop_js/hybridgref_ani.h"
#include "interop_js/hybridgref_napi.h"
#include "interop_js/arkts_interop_js_api.h"

namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
}

bool AniDeserialize::PeerDeserialize(ani_env* env, ani_ref* peerData, std::shared_ptr<SerializeData> serializeData)
{
    if (std::holds_alternative<ani_ref>(serializeData->peerData)) {
        *peerData = std::get<ani_ref>(serializeData->peerData);
    }
    return true;
}

bool AniDeserialize::CrossDeserialize(ani_env* env, ani_ref* crossData, std::shared_ptr<SerializeData> serializeData)
{
    if (!std::get<napi_value>(serializeData->peerData)) {
        return true;
    }
    napi_env napiEnv = {};
    if (!arkts_napi_scope_open(env, &napiEnv)) {
        HILOGE("CrossDeserialize failed to arkts_napi_scope_open");
        return false;
    }
    hybridgref dynamicHybrigRef = nullptr;
    napi_value deserializeNapiData = nullptr;
    if (napi_deserialize_hybrid(napiEnv, std::get<napi_value>(serializeData->peerData),
        &deserializeNapiData) != napi_ok) {
        HILOGE("CrossDeserialize failed to napi_deserialize_hybrid");
        arkts_napi_scope_close_n(napiEnv, 0, nullptr, nullptr);
        return false;
    }
    if (!hybridgref_create_from_napi(napiEnv, deserializeNapiData, &dynamicHybrigRef)) {
        HILOGE("CrossDeserialize failed to hybridgref_create_from_napi");
        arkts_napi_scope_close_n(napiEnv, 0, nullptr, nullptr);
        return false;
    }
    if (!hybridgref_get_esvalue(env, dynamicHybrigRef, reinterpret_cast<ani_object*>(crossData))) {
        HILOGE("CrossDeserialize failed to hybridgref_create_from_napi");
        arkts_napi_scope_close_n(napiEnv, 0, nullptr, nullptr);
        hybridgref_delete_from_napi(napiEnv, dynamicHybrigRef);
        return false;
    }
    hybridgref_delete_from_napi(napiEnv, dynamicHybrigRef);
    if (!arkts_napi_scope_close_n(napiEnv, 0, nullptr, nullptr)) {
        HILOGE("CrossDeserialize failed to arkts_napi_scope_close_n");
        return false;
    }
    crossData = reinterpret_cast<ani_ref*>(crossData);
    return true;
}

} // namespace AppExecFwk
} // namespace OHOS