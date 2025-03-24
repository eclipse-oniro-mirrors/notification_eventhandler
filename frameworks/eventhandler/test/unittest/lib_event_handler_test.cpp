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

#include "event_handler.h"
#include "event_queue.h"
#include "event_runner.h"
#include "inner_event.h"
#ifdef FFRT_USAGE_ENABLE
#include "ffrt_inner.h"
#endif // FFRT_USAGE_ENABLE
#include "ffrt_descriptor_listener.h"

#include <gtest/gtest.h>
#include <dlfcn.h>
#include <string>
#include <unistd.h>


using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::AppExecFwk;

typedef void (*Ffrt)();
typedef bool (*FfrtPostTask)(void* handler, const std::function<void()>& callback,
    const std::string &name, int64_t delayTime, EventQueue::Priority priority);
typedef bool (*FfrtSyncPostTask)(void* handler, const std::function<void()>& callback,
    const std::string &name, EventQueue::Priority priority);
typedef bool (*RemoveTaskForFFRT)(void* handler, const std::string &name);
typedef bool (*RemoveAllTaskForFFRT)(void* handler);
typedef bool (*FfrtPostSyncTask)(void* handler, const std::function<void()>& callback,
    const std::string &name, EventQueue::Priority priority);
typedef int (*AddFdListenerByFFRT)(void* handler, uint32_t fd, uint32_t event, void* data, ffrt_poller_cb cb);
typedef int (*RemoveFdListenerByFFRT)(void* handler, uint32_t fd);

class LibEventHandlerTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

void LibEventHandlerTest::SetUpTestCase(void)
{}

void LibEventHandlerTest::TearDownTestCase(void)
{}

void LibEventHandlerTest::SetUp(void)
{}

void LibEventHandlerTest::TearDown(void)
{}

void* GetTemp(char* func, void* handle)
{
    if (!handle) {
        return nullptr;
    }
    void* temp = dlsym(handle, func);
    return temp;
}

void ExecFfrtNoParam(char* func)
{
    void* handle = dlopen("/system/lib64/chipset-pub-sdk/libeventhandler.z.so", RTLD_LAZY);
    void* temp = GetTemp(func, handle);
    if (temp) {
        Ffrt ffrt = reinterpret_cast<Ffrt>(temp);
        EXPECT_NE(nullptr, ffrt);
        (*ffrt)();
    }
    dlclose(handle);
}

/*
 * @tc.name: Ffrt001
 * @tc.desc: Invoke TraceInfo interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, Ffrt001, TestSize.Level1)
{
    string str = "GetMainEventHandlerForFFRT";
    ExecFfrtNoParam(str.data());
}

/*
 * @tc.name: Ffrt002
 * @tc.desc: Invoke TraceInfo interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, Ffrt002, TestSize.Level1)
{
    string str = "GetCurrentEventHandlerForFFRT";
    ExecFfrtNoParam(str.data());
}

/*
 * @tc.name: Ffrt003
 * @tc.desc: Invoke TraceInfo interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, Ffrt003, TestSize.Level1)
{
    string str = "PostTaskByFFRT";
    void* handle = dlopen("/system/lib64/chipset-pub-sdk/libeventhandler.z.so", RTLD_LAZY);
    void* temp = GetTemp(str.data(), handle);
    if (temp) {
        auto task = [this]() {
            return;
        };
        FfrtPostTask ffrt = reinterpret_cast<FfrtPostTask>(temp);
        bool result = (*ffrt)(nullptr, task, "x", 10, EventQueue::Priority::LOW);
        EXPECT_EQ(false, result);
    }
}

/*
 * @tc.name: Ffrt004
 * @tc.desc: Invoke TraceInfo interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, Ffrt004, TestSize.Level1)
{
    string str = "PostSyncTaskByFFRT";
    void* handle = dlopen("/system/lib64/chipset-pub-sdk/libeventhandler.z.so", RTLD_LAZY);
    void* temp = GetTemp(str.data(), handle);
    if (temp) {
        auto task = [this]() {
            return;
        };
        FfrtPostSyncTask ffrt = reinterpret_cast<FfrtPostSyncTask>(temp);
        bool result = (*ffrt)(nullptr, task, "x", EventQueue::Priority::LOW);
        EXPECT_EQ(false, result);
        auto handler = std::make_shared<EventHandler>(nullptr);
        bool result1 = (*ffrt)(&handler, task, "x", EventQueue::Priority::LOW);
        EXPECT_EQ(false, result1);
    }
}

/*
 * @tc.name: Ffrt005
 * @tc.desc: Invoke TraceInfo interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, Ffrt005, TestSize.Level1)
{
    string str = "RemoveTaskForFFRT";
    void* handle = dlopen("/system/lib64/chipset-pub-sdk/libeventhandler.z.so", RTLD_LAZY);
    void* temp = GetTemp(str.data(), handle);
    if (temp) {
        RemoveTaskForFFRT ffrt = reinterpret_cast<RemoveTaskForFFRT>(temp);
        EXPECT_NE(nullptr, ffrt);
        (*ffrt)(nullptr, "x");
        auto handler = std::make_shared<EventHandler>(nullptr);
        (*ffrt)(&handler, "x");
    }
}

/*
 * @tc.name: Ffrt006
 * @tc.desc: Invoke TraceInfo interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, Ffrt006, TestSize.Level1)
{
    string str = "RemoveTaskForFFRT";
    void* handle = dlopen("/system/lib64/chipset-pub-sdk/libeventhandler.z.so", RTLD_LAZY);
    void* temp = GetTemp(str.data(), handle);
    if (temp) {
        RemoveAllTaskForFFRT ffrt = reinterpret_cast<RemoveAllTaskForFFRT>(temp);
        EXPECT_NE(nullptr, ffrt);
        (*ffrt)(nullptr);
    }
}

/*
 * @tc.name: Ffrt007
 * @tc.desc: Invoke TraceInfo interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, Ffrt007, TestSize.Level1)
{
    string str = "RemoveAllTaskForFFRT";
    void* handle = dlopen("/system/lib64/chipset-pub-sdk/libeventhandler.z.so", RTLD_LAZY);
    void* temp = GetTemp(str.data(), handle);
    if (temp) {
        RemoveAllTaskForFFRT ffrt = reinterpret_cast<RemoveAllTaskForFFRT>(temp);
        EXPECT_NE(nullptr, ffrt);
        (*ffrt)(nullptr);
        auto handler = std::make_shared<EventHandler>(nullptr);
        (*ffrt)(&handler);
    }
}

/*
 * @tc.name: Ffrt008
 * @tc.desc: Ffrt008 test
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, Ffrt008, TestSize.Level1)
{
    string str = "AddFdListenerByFFRT";
    void* handle = dlopen("/system/lib64/chipset-pub-sdk/libeventhandler.z.so", RTLD_LAZY);
    void* temp = GetTemp(str.data(), handle);
    if (temp) {
        AddFdListenerByFFRT ffrt = reinterpret_cast<AddFdListenerByFFRT>(temp);
        EXPECT_NE(nullptr, ffrt);
        (*ffrt)(nullptr, 1, 1, nullptr, nullptr);
        auto handler = std::make_shared<EventHandler>(nullptr);
        ffrt_poller_cb cb = [](void* data, uint32_t param) {
            if (!data) {
                return;
            }
            if (param ==0) {
                return;
            }
        };
        (*ffrt)(&handler, 1, 1, &handler, cb);
    }
}

/*
 * @tc.name: Ffrt009
 * @tc.desc: Ffrt009 test
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, Ffrt009, TestSize.Level1)
{
    string str = "RemoveFdListenerByFFRT";
    void* handle = dlopen("/system/lib64/chipset-pub-sdk/libeventhandler.z.so", RTLD_LAZY);
    void* temp = GetTemp(str.data(), handle);
    if (temp) {
        RemoveFdListenerByFFRT ffrt = reinterpret_cast<RemoveFdListenerByFFRT>(temp);
        EXPECT_NE(nullptr, ffrt);
        (*ffrt)(nullptr, 1);
        auto handler = std::make_shared<EventHandler>(nullptr);
        (*ffrt)(&handler, 1);
    }
}

/*
 * @tc.name: RemoveTask
 * @tc.desc: remove task by name
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, RemoveTask001, TestSize.Level1)
{
    auto runner = EventRunner::Create("Runner");
    auto handler = std::make_shared<EventHandler>(runner);
    int result = handler->RemoveTaskWithRet("task");
    EXPECT_EQ(result, 1);
}

/*
 * @tc.name: RemoveFileDescriptor_001
 * @tc.desc: RemoveFileDescriptor_001 test
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, RemoveFileDescriptor_001, TestSize.Level1)
{
    auto handler = std::make_shared<EventHandler>(nullptr);
    handler->RemoveAllFileDescriptorListeners();
    handler->RemoveFileDescriptorListener(1);
    handler->HasPendingHigherEvent(1);
    handler->QueryPendingTaskInfo(1);
    handler->TaskCancelAndWait();
    int result = handler->RemoveTaskWithRet("task");
    EXPECT_EQ(result, 1);
}

/*
 * @tc.name: DistributeTimeoutHandler_001
 * @tc.desc: DistributeTimeoutHandler_001 test
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerTest, DistributeTimeoutHandler_001, TestSize.Level1)
{
    auto runner = EventRunner::GetMainEventRunner();
    auto handler = std::make_shared<EventHandler>(runner);
    runner->SetTimeout(2);
    InnerEvent::TimePoint now = InnerEvent::Clock::now();
    handler->DistributeTimeoutHandler(now);
    InnerEvent::Pointer event(nullptr, nullptr);
    handler->GetEventName(event);
    bool result = handler->HasPendingHigherEvent(0);
    EXPECT_EQ(result, false);
    Caller call;
    InnerEvent::EventId id = "a";
    auto event1 = InnerEvent::Get(id, 1, call);
    handler->GetEventName(event1);
}