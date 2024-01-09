/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_LOGGER_H
#define BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_LOGGER_H

#include <cinttypes>
#include <functional>
#include <future>
#include <string>
#include <sstream>

#include "hilog/log.h"

namespace OHOS {
namespace AppExecFwk {
inline constexpr uint32_t EH_LOG_DOMAIN = 0xD001200;

class InnerFunctionTracer {
public:
    using HilogFunc = std::function<int(const char *)>;

    InnerFunctionTracer(HilogFunc logfn, const char* tag, LogLevel level)
        : logfn_(logfn), tag_(tag), level_(level)
    {
        if (HiLogIsLoggable(OHOS::AppExecFwk::EH_LOG_DOMAIN, tag_, level_)) {
            if (logfn_ != nullptr) {
                logfn_("in %{public}s, enter");
            }
        }
    }
    ~InnerFunctionTracer()
    {
        if (HiLogIsLoggable(OHOS::AppExecFwk::EH_LOG_DOMAIN, tag_, level_)) {
            if (logfn_ != nullptr) {
                logfn_("in %{public}s, leave");
            }
        }
    }
private:
    HilogFunc logfn_ { nullptr };
    const char* tag_ { nullptr };
    LogLevel level_ { LOG_LEVEL_MIN };
};
} // namespace AppExecFwk
} // namespace OHOS

#ifndef EH_FUNC_FMT
#define EH_FUNC_FMT "in %{public}s:%{public}d, "
#endif

#ifndef EH_FUNC_INFO
#define EH_FUNC_INFO __FUNCTION__, __LINE__
#endif

#ifndef EH_FILE_NAME
#define EH_FILE_NAME   (strrchr((__FILE__), '/') ? strrchr((__FILE__), '/') + 1 : (__FILE__))
#endif

#ifndef EH_LINE_INFO
#define EH_LINE_INFO   EH_FILE_NAME, __LINE__
#endif

#define DEFINE_EH_HILOG_LABEL(name) \
    static const constexpr char* EH_LOG_LABEL = (name)

#define HILOGD(fmt, ...) do { \
    if (HiLogIsLoggable(OHOS::AppExecFwk::EH_LOG_DOMAIN, EH_LOG_LABEL, LOG_DEBUG)) { \
        ((void)HILOG_IMPL(LOG_CORE, LOG_INFO, EH_LOG_DOMAIN, EH_LOG_LABEL, fmt, ##__VA_ARGS__)); \
    } \
} while (0)

#define HILOGI(fmt, ...) do { \
    ((void)HILOG_IMPL(LOG_CORE, LOG_INFO, EH_LOG_DOMAIN, EH_LOG_LABEL, fmt, ##__VA_ARGS__)); \
} while (0)

#define HILOGW(fmt, ...) do { \
    ((void)HILOG_IMPL(LOG_CORE, LOG_WARN, EH_LOG_DOMAIN, EH_LOG_LABEL, fmt, ##__VA_ARGS__)); \
} while (0)

#define HILOGE(fmt, ...) do { \
    ((void)HILOG_IMPL(LOG_CORE, LOG_ERROR, EH_LOG_DOMAIN, EH_LOG_LABEL, fmt, ##__VA_ARGS__)); \
} while (0)

#define HILOGF(fmt, ...) do { \
    ((void)HILOG_IMPL(LOG_CORE, LOG_FATAL, EH_LOG_DOMAIN, EH_LOG_LABEL, fmt, ##__VA_ARGS__)); \
} while (0)

#ifndef EH_CALL_DEBUG_ENTER
#define EH_CALL_DEBUG_ENTER        ::OHOS::AppExecFwk::InnerFunctionTracer ___innerFuncTracer_Debug___   \
    { std::bind(&::OHOS::HiviewDFX::HiLog::Debug, EH_LOG_LABEL, std::placeholders::_1,                   \
      __FUNCTION__), EH_LOG_LABEL.tag, LOG_DEBUG }
#endif // EH_CALL_DEBUG_ENTER

#ifndef EH_CALL_INFO_TRACE
#define EH_CALL_INFO_TRACE         ::OHOS::AppExecFwk::InnerFunctionTracer ___innerFuncTracer_Info___    \
    { std::bind(&::OHOS::HiviewDFX::HiLog::Info, EH_LOG_LABEL, std::placeholders::_1,                    \
      __FUNCTION__), EH_LOG_LABEL.tag, LOG_INFO }
#endif // EH_CALL_INFO_TRACE

#ifndef EH_CALL_TEST_DEBUG
#define EH_CALL_TEST_DEBUG         ::OHOS::AppExecFwk::InnerFunctionTracer ___innerFuncTracer_Info___    \
    { std::bind(&::OHOS::HiviewDFX::HiLog::Info, EH_LOG_LABEL, std::placeholders::_1,                    \
      (test_info_ == nullptr ? "TestBody" : test_info_->name())), EH_LOG_LABEL.tag, LOG_DEBUG }
#endif // EH_CALL_TEST_DEBUG

#endif // BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_LOGGER_H
