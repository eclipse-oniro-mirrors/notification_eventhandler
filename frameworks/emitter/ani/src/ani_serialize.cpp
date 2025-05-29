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
#include "sts_events_json_common.h"
#include "event_logger.h"

namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
}

bool AniSerialize::PeerSerialize(ani_env* env, ani_object argv, std::shared_ptr<SerializeData> serializeData)
{
    ani_ref record = nullptr;
    if (GetRefPropertyByName(env, argv, "data", record)) {
        if (record != nullptr) {
            ani_ref peerData = nullptr;
            if (env->GlobalReference_Create(record, &peerData) != ANI_OK) {
                HILOGI("Json stringify failed");
                return false;
            }
            serializeData->peerData = peerData;
        } else {
            return false;
        }
    }
    return true;
}

bool AniSerialize::CrossSerialize(ani_env* env, ani_object argv, std::shared_ptr<SerializeData> serializeData)
{
    ani_ref record = nullptr;
    if (GetRefPropertyByName(env, argv, "data", record)) {
        if (record != nullptr) {
            ani_status status = ANI_OK;
            ani_namespace ns {};
            status = env->FindNamespace("L@ohos/util/json/json;", &ns);
            if (status != ANI_OK) {
                HILOGI("Failed to find namespace");
                return false;
            }
            ani_function fnStringify {};
            status = env->Namespace_FindFunction(ns, "stringify", nullptr, &fnStringify);
            if (status != ANI_OK) {
                HILOGI("Failed to find stringify");
                return false;
            }
            ani_ref ref {};
            ani_value args[] = {{.r = record}, {.r = nullptr}, {.r = nullptr}};
            status = env->Function_Call_Ref_A(fnStringify, &ref, args);
            if (status != ANI_OK) {
                HILOGI("Failed to call stringify");
                return false;
            }
            ani_size sz {};
            ani_string str = static_cast<ani_string>(ref);
            if (env->String_GetUTF8Size(str, &sz) != ANI_OK) {
                HILOGI("Failed to get string size");
                return false;
            }
            serializeData->crossData.resize(sz + 1);
            status = env->String_GetUTF8SubString(
                str, 0, sz, serializeData->crossData.data(), serializeData->crossData.size(), &sz);
            if (status != ANI_OK) {
                HILOGI("Failed to convert ani string to c++ string");
                return false;
            }
            serializeData->crossData.resize(sz);
        }
    }
    return true;
}

} // namespace AppExecFwk
} // namespace OHOS