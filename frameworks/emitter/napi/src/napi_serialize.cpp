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
        napi_status serializeResult = napi_ok;
        napi_value undefined = nullptr;
        napi_get_undefined(env, &undefined);
        bool defaultTransfer = false;
        bool defaultCloneSendable = false;
        serializeResult = napi_serialize_inner(env, data, undefined, undefined,
            defaultTransfer, defaultCloneSendable, &result);
        if (serializeResult != napi_ok || result == nullptr) {
            HILOGE("Serialize fail.");
            return false;
        }
    }
    serializeData->peerData = reinterpret_cast<napi_value>(result);
    return true;
}

bool NapiSerialize::CrossSerialize(napi_env env, napi_value argv, std::shared_ptr<SerializeData> serializeData)
{
    bool hasData = false;
    napi_has_named_property(env, argv, "data", &hasData);
    if (hasData) {
        napi_value data = nullptr;
        napi_get_named_property(env, argv, "data", &data);
        napi_value globalValue;
        napi_get_global(env, &globalValue);
        napi_value jsonValue;
        napi_get_named_property(env, globalValue, "JSON", &jsonValue);
        napi_value stringifyValue;
        napi_get_named_property(env, jsonValue, "stringify", &stringifyValue);
        napi_value funcArgv[1] = { data };
        napi_value returnValue;
        if (napi_call_function(env, jsonValue, stringifyValue, 1, funcArgv, &returnValue) != napi_ok) {
            HILOGE("Serialize fail.");
            return false;
        }
        size_t len = 0;
        napi_get_value_string_utf8(env, returnValue, nullptr, 0, &len);
        std::unique_ptr<char[]> paramsChar = std::make_unique<char[]>(len + 1);
        napi_get_value_string_utf8(env, returnValue, paramsChar.get(), len + 1, &len);
        serializeData->crossData = paramsChar.get();
    }
    return true;
}
} // namespace AppExecFwk
} // namespace OHOS