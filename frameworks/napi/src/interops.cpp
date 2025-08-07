/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "interops.h"

namespace OHOS {
namespace AppExecFwk {
EmitterEnhancedApiRegister::EmitterEnhancedApiRegister()
{
    enhancedApi_ = std::make_shared<EmitterEnhancedApi>();
}

void EmitterEnhancedApiRegister::Register(const EmitterEnhancedApi& api)
{
    enhancedApi_->JS_Off = api.JS_Off;
    enhancedApi_->JS_Emit = api.JS_Emit;
    enhancedApi_->JS_GetListenerCount = api.JS_GetListenerCount;
    enhancedApi_->ProcessEvent = api.ProcessEvent;
    enhancedApi_->ProcessCallbackEnhanced = api.ProcessCallbackEnhanced;
    isInit_.store(true, std::memory_order_release);
}

bool EmitterEnhancedApiRegister::IsInit() const
{
    return isInit_.load(std::memory_order_acquire);
}

std::shared_ptr<EmitterEnhancedApi> EmitterEnhancedApiRegister::GetEnhancedApi() const
{
    return enhancedApi_;
}

EmitterEnhancedApiRegister& GetEmitterEnhancedApiRegister()
{
    static EmitterEnhancedApiRegister emitterEnhancedApiRegister;
    return emitterEnhancedApiRegister;
}
}
}