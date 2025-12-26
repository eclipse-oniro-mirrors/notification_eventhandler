/*
 * Copyright (c) 2022-2025 Huawei Device Co., Ltd.
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

#include "async_stack_adapter.h"
#include "event_logger.h"

namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("AsyncStackAdapter");
} // namespace
AsyncStackAdapter& AsyncStackAdapter::GetInstance()
{
    static AsyncStackAdapter instance;
    return instance;
}

void AsyncStackAdapter::SetAsyncStackFunc(EventCollectAsyncStackFunc func)
{
    asyncStackFunc = func;
}

void AsyncStackAdapter::SetStackIdFunc(EventSetStackIdFunc func)
{
    stackIdFunc = func;
}

uint64_t AsyncStackAdapter::EventCollectAsyncStack(uint64_t type)
{
    auto func = asyncStackFunc;
    if (func != nullptr) {
        return func(type);
    }

    return 0;
}

void AsyncStackAdapter::EventSetStackId(uint64_t stackId)
{
    auto func = stackIdFunc;
    if (func != nullptr) {
        return func(stackId);
    }
}

AsyncStackAdapter::AsyncStackAdapter()
{
    HILOGD("create AsyncStackAdapter");
}

AsyncStackAdapter::~AsyncStackAdapter()
{
    asyncStackFunc = nullptr;
    stackIdFunc = nullptr;
}
}  // namespace AppExecFwk
}  // namespace OHOS