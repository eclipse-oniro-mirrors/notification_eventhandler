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

#include "ani_serialize.h"
#include "event_logger.h"
#include "interop_js/hybridgref_ani.h"
#include "interop_js/hybridgref_napi.h"
#include "interop_js/arkts_interop_js_api.h"
#include "napi/native_node_hybrid_api.h"
namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");

bool GetRefPropertyByName(ani_env *env, ani_object param, const char *name, ani_ref &ref)
{
    ani_status status = ANI_ERROR;
    if ((status = env->Object_GetPropertyByName_Ref(param, name, &ref)) != ANI_OK) {
        HILOGI("Object_GetFieldByName_Ref failed, status : %{public}d", status);
        return false;
    }
    return true;
}
}

bool AniSerialize::PeerSerialize(ani_env* env, ani_object argv, std::shared_ptr<SerializeData> serializeData)
{
    if (argv == nullptr) {
        // emit with no data
        return true;
    }
    ani_ref record = nullptr;
    if (GetRefPropertyByName(env, argv, "data", record)) {
        if (record == nullptr) {
            HILOGE("PeerSerialize failed to GetRefPropertyByName, record is nullptr");
            return false;
        }
        ani_ref peerData = nullptr;
        if (env->GlobalReference_Create(record, &peerData) != ANI_OK) {
            HILOGE("PeerSerialize failed to GlobalReference_Create");
            return false;
        }
        serializeData->peerData = peerData;
        return true;
    }
    return false;
}

bool AniSerialize::CrossSerialize(ani_env* env, ani_object argv, std::shared_ptr<SerializeData> serializeData)
{
    if (argv == nullptr) {
        // emit with no data
        return true;
    }
    ani_ref record = nullptr;
    if (GetRefPropertyByName(env, argv, "data", record)) {
        if (record == nullptr) {
            return false;
        }
        hybridgref dynamicHybrigRef = nullptr;
        if (!hybridgref_create_from_ani(env, reinterpret_cast<ani_ref>(record), &dynamicHybrigRef)) {
            HILOGE("CrossSerialize failed to hybridgref_create_from_ani");
            return false;
        }
        napi_env napiEnv = {};
        if (!arkts_napi_scope_open(env, &napiEnv)) {
            HILOGE("CrossSerialize failed to arkts_napi_scope_open");
            hybridgref_delete_from_ani(env, dynamicHybrigRef);
            return false;
        }
        napi_value napiData = nullptr;
        if (!hybridgref_get_napi_value(napiEnv, dynamicHybrigRef, &napiData)) {
            HILOGE("CrossSerialize failed to hybridgref_get_napi_value");
            hybridgref_delete_from_ani(env, dynamicHybrigRef);
            arkts_napi_scope_close_n(napiEnv, 0, nullptr, nullptr);
            return false;
        }
        napi_value undefined = nullptr;
        napi_get_undefined(napiEnv, &undefined);
        if (napi_serialize_hybrid(napiEnv, napiData, undefined, undefined, &serializeData->crossData) != napi_ok) {
            HILOGE("CrossSerialize failed to napi_serialize_hybrid");
            hybridgref_delete_from_ani(env, dynamicHybrigRef);
            arkts_napi_scope_close_n(napiEnv, 0, nullptr, nullptr);
            return false;
        }
        bool isDeleteHybridgrefSucc = hybridgref_delete_from_ani(env, dynamicHybrigRef);
        bool isCloseScopeSucc = arkts_napi_scope_close_n(napiEnv, 0, nullptr, nullptr);
        if (!isDeleteHybridgrefSucc || !isCloseScopeSucc) {
            HILOGE("delete status: %{public}d, close status: %{public}d", isDeleteHybridgrefSucc, isCloseScopeSucc);
            return false;
        }
        return true;
    }
    HILOGE("CrossSerialize failed to GetRefPropertyByName");
    return false;
}

} // namespace AppExecFwk
} // namespace OHOS