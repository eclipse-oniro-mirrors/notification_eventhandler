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

#include "napi_deserialize.h"
#include "event_logger.h"

namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
}
bool NapiDeserialize::PeerDeserialize(napi_env env, napi_value* peerData, std::shared_ptr<SerializeData> serializeData)
{
    if (std::get<napi_value>(serializeData->peerData)) {
        if (napi_deserialize(env, std::get<napi_value>(serializeData->peerData), peerData) != napi_ok) {
            HILOGE("Deserialize fail.");
            return false;
        }
    }
    return true;
}

bool NapiDeserialize::CrossDeserialize(
    napi_env env, napi_value* crossData, std::shared_ptr<SerializeData> serializeData)
{
    if (serializeData->crossData.length() > 0) {
        napi_value globalValue;
        napi_get_global(env, &globalValue);
        napi_value jsonValue;
        napi_get_named_property(env, globalValue, "JSON", &jsonValue);
        napi_value parseValue;
        napi_get_named_property(env, jsonValue, "parse", &parseValue);
        napi_value paramsNApi;
        napi_create_string_utf8(env, serializeData->crossData.c_str(), NAPI_AUTO_LENGTH, &paramsNApi);
        napi_value funcArgv[1] = { paramsNApi };
        napi_call_function(env, jsonValue, parseValue, 1, funcArgv, crossData);
    }
    return true;
}
} // namespace AppExecFwk
} // namespace OHOS