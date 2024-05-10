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

#include "event_queue.h"


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
        (*ffrt)(nullptr, "x");
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
        (*ffrt)(nullptr);
    }
}