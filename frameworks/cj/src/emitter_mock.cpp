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

#include "cj_common_ffi.h"

extern "C" {
    // CommonEventManager ffi apis
    FFI_EXPORT int CJ_OnWithId = 0;
    FFI_EXPORT int CJ_OnWithStringId = 0;
    FFI_EXPORT int CJ_OnceWithId = 0;
    FFI_EXPORT int CJ_OnceWithStringId = 0;
    FFI_EXPORT int CJ_OffWithId = 0;
    FFI_EXPORT int CJ_OffWithString = 0;
    FFI_EXPORT int CJ_OffWithIdCallback = 0;
    FFI_EXPORT int CJ_OffWithStringCallback = 0;
    FFI_EXPORT int CJ_EmitWithId = 0;
    FFI_EXPORT int CJ_EmitWithString = 0;
    FFI_EXPORT int CJ_GetListenerCountById = 0;
    FFI_EXPORT int CJ_GetListenerCountByString = 0;
}