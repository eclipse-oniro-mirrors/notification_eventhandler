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

#ifndef BASE_EVENTHANDLER_FRAMEWORKS_SERIALIZE_H
#define BASE_EVENTHANDLER_FRAMEWORKS_SERIALIZE_H

#include <variant>

#include "ani.h"
#include "napi/native_api.h"
#include "js_native_api_types.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AppExecFwk {

enum EnvType {
    NAPI,
    ANI
};

struct SerializeData {
    std::variant<napi_value, ani_ref> peerData {};
    std::string crossData {};
    EnvType envType {EnvType::NAPI};
    bool isCrossRuntime {false};
};

} // namespace AppExecFwk
} // namespace OHOS
#endif // BASE_EVENTHANDLER_FRAMEWORKS_SERIALIZE_H