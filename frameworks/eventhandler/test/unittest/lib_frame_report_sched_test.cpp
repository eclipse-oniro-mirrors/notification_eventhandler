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
 
#include <gtest/gtest.h>
#include <string>
#define private public
#include "frame_report_sched.h"
 
using namespace testing::ext;
using namespace OHOS::AppExecFwk;
 
class FrameReportTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp() override;
    void TearDown() override;
};
 
void FrameReportTest::SetUpTestCase() {}
void FrameReportTest::TearDownTestCase() {}
void FrameReportTest::SetUp() {}
void FrameReportTest::TearDown() {}
 
/**
 * @tc.name: LoadLibrary001
 * @tc.desc: test
 * @tc.type:FUNC
 * @tc.require:
 */
HWTEST_F(FrameReportTest, LoadLibrary001, TestSize.Level1)
{
    FrameReport& fr = FrameReport::GetInstance();
    fr.LoadLibrary();
    EXPECT_TRUE(fr.frameSchedSoLoaded_);
}
 
/**
 * @tc.name: CloseLibrary001
 * @tc.desc: test
 * @tc.type:FUNC
 * @tc.require:
 */
HWTEST_F(FrameReportTest, CloseLibrary001, TestSize.Level1)
{
    FrameReport& fr = FrameReport::GetInstance();
    fr.CloseLibrary();
    EXPECT_FALSE(fr.frameSchedSoLoaded_);
}
 
/**
 * @tc.name: LoadSymbol001
 * @tc.desc: test
 * @tc.type:FUNC
 * @tc.require:
 */
HWTEST_F(FrameReportTest, LoadSymbol001, TestSize.Level1)
{
    FrameReport& fr = FrameReport::GetInstance();
    fr.LoadSymbol("function");
    EXPECT_EQ(fr.frameSchedHandle_, nullptr);
    fr.LoadLibrary();
    EXPECT_EQ(fr.LoadSymbol("function"), nullptr);
    EXPECT_NE(fr.LoadSymbol("ReportSchedEvent"), nullptr);
}
 
/**
 * @tc.name: ReportSchedEvent001
 * @tc.desc: test
 * @tc.type:FUNC
 * @tc.require:
 */
HWTEST_F(FrameReportTest, ReportSchedEvent001, TestSize.Level1)
{
    FrameReport& fr = FrameReport::GetInstance();
    fr.ReportSchedEvent(FrameSchedEvent::INIT, {});
    EXPECT_NE(fr.reportSchedEventFunc_, nullptr);
    fr.uid_ = 1003;
    fr.ReportSchedEvent(FrameSchedEvent::INIT, {});
    EXPECT_NE(fr.reportSchedEventFunc_, nullptr);
}