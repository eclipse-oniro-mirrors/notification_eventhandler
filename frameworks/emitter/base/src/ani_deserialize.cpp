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

namespace OHOS {
namespace AppExecFwk {
namespace {
    DEFINE_EH_HILOG_LABEL("EventsEmitter");
}
static ani_ref JsonParse(ani_env *env, std::string jsonStr)
{
    ani_status status = ANI_ERROR;
    ani_class cls = nullptr;
    if ((status = env->FindClass("L@ohos/events/json/RecordSerializeTool;", &cls)) != ANI_OK) {
        HILOGI("FindClass RecordSerializeTool failed, status : %{public}d", status);
        return nullptr;
    }
    if (cls == nullptr) {
        HILOGI("RecordSerializeTool class null");
        return nullptr;
    }
    ani_static_method parseNoThrowMethod = nullptr;
    status = env->Class_FindStaticMethod(cls, "parseNoThrow", nullptr, &parseNoThrowMethod);
    if (status != ANI_OK) {
        HILOGI("failed to get parseNoThrow method, status : %{public}d", status);
        return nullptr;
    }

    ani_string aniString;
    status = env->String_NewUTF8(jsonStr.c_str(), jsonStr.length(), &aniString);
    if (status != ANI_OK) {
        HILOGI("String_NewUTF8 wantParamsString failed, status : %{public}d", status);
        return nullptr;
    }

    ani_ref aniResult = nullptr;
    status = env->Class_CallStaticMethod_Ref(cls, parseNoThrowMethod, &aniResult, aniString);
    if (status != ANI_OK) {
        HILOGI("failed to call parseNoThrow method, status : %{public}d", status);
        return nullptr;
    }
    return aniResult;
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
    if (serializeData->crossData.empty()) {
        return true;
    }
    *crossData = JsonParse(env, serializeData->crossData);
    if (*crossData == nullptr) {
        HILOGI("json parse failed");
        return false;
    }
    return true;
}

} // namespace AppExecFwk
} // namespace OHOS