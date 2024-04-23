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

#include <gtest/gtest.h>
#include "inner_event.h"

using namespace testing::ext;
using namespace OHOS::AppExecFwk;

class LibEventHandlerInnerEventTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

void LibEventHandlerInnerEventTest::SetUpTestCase(void)
{}

void LibEventHandlerInnerEventTest::TearDownTestCase(void)
{}

void LibEventHandlerInnerEventTest::SetUp(void)
{}

void LibEventHandlerInnerEventTest::TearDown(void)
{}


/*
 * @tc.name: TraceInfo001
 * @tc.desc: Invoke TraceInfo interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerInnerEventTest, TraceInfo001, TestSize.Level1)
{
    uint32_t eventId = 0;
    int64_t eventParam = 0;
    auto event = InnerEvent::Get(eventId, eventParam);
    std::string result = event->TraceInfo();
    EXPECT_EQ("Et:NA", result);
}


/*
 * @tc.name: GetEventPointer001
 * @tc.desc: Invoke GetEventPointer001 interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerInnerEventTest, GetEventPointer001, TestSize.Level1)
{
    string taskName("taskName");
    bool callbackCalled = false;
    auto f = [&callbackCalled]() { callbackCalled = true; };
    InnerEvent::Pointer event = InnerEvent::Get(f, taskName);
    auto getName = event->GetTaskName();
    EXPECT_EQ(taskName, getName);
    // execute callback function, check whether the callback function is the one we set
    (event->GetTaskCallback())();
    // drop event, execute destructor function
    EXPECT_TRUE(callbackCalled);
}
