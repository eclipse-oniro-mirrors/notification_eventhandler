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

#ifndef STS_EVENTS_EMITTER_H
#define STS_EVENTS_EMITTER_H

#include "ani.h"
#include "../../../interfaces/inner_api/inner_event.h"

namespace OHOS {
namespace AppExecFwk {

struct AniAsyncCallbackInfo {
    std::atomic<ani_env*> env;
    std::atomic<bool> once = false;
    std::atomic<bool> isDeleted = false;
    ani_ref callback = 0;
    InnerEvent::EventId eventId;
    ~AniAsyncCallbackInfo();
};

class EventsEmitter {
public:
    static std::string GetStdString(ani_env *env, ani_string str);
    static std::shared_ptr<AniAsyncCallbackInfo> SearchCallbackInfo(
        ani_env *env, const InnerEvent::EventId &eventIdValue, ani_ref callback);
    static void ReleaseCallbackInfo(ani_env *env, AniAsyncCallbackInfo* callbackInfo);
    static void UpdateOnceFlag(std::shared_ptr<AniAsyncCallbackInfo>callbackInfo, bool once);
    static void DeleteCallbackInfo(ani_env *env, const InnerEvent::EventId &eventIdValue, ani_ref callback);
    static void onOrOnce(ani_env *env, InnerEvent::EventId eventId, bool once, ani_ref callback);
    static void offEmitterInstances(InnerEvent::EventId eventId);
    static void AniWrap(ani_env *env, ani_ref callback);
};
}
}

#endif // STS_EVENTS_EMITTER_H