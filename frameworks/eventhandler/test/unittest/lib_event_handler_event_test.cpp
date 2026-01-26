/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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

#include <cstdlib>
#include <vector>

#include "event_handler.h"
#include "event_queue.h"
#include "event_queue_base.h"
#include "event_runner.h"
#include "inner_event.h"
#define private public
#include "native_implement_eventhandler.h"
#undef private

using namespace testing::ext;
using namespace OHOS::AppExecFwk;
namespace {
const size_t MAX_POOL_SIZE = 64;
}

typedef void (*FileFDCallback)(int32_t filedescriptor);

struct FileDescriptorCallbacks {
    FileFDCallback readableCallback_;
    FileFDCallback writableCallback_;
    FileFDCallback shutdownCallback_;
    FileFDCallback exceptionCallback_;
};

/**
 * test for task information.
 */
static void TestTaskInfo()
{
    string taskName("taskName");
    bool callbackCalled = false;
    auto f = [&callbackCalled]() { callbackCalled = true; };
    auto event = InnerEvent::Get(f, taskName);
    auto getName = event->GetTaskName();
    EXPECT_EQ(taskName, getName);
    // execute callback function, check whether the callback function is the one we set
    (event->GetTaskCallback())();
    // drop event, execute destructor function
    EXPECT_TRUE(callbackCalled);
}

/**
 * Deleter of event shared pointer.
 */
inline static void Deleter(uint32_t *object)
{
    if (object == nullptr) {
        return;
    }
    delete object;
    object = nullptr;
}

class LibEventHandlerEventTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

void LibEventHandlerEventTest::SetUpTestCase(void)
{}

void LibEventHandlerEventTest::TearDownTestCase(void)
{}

void LibEventHandlerEventTest::SetUp(void)
{}

void LibEventHandlerEventTest::TearDown(void)
{}

/*
 * @tc.name: GetEvent001
 * @tc.desc: get event from pool, set id and param
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEvent001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with event id and param, then get event id and param from event.
     * @tc.expected: step1. the event id and param is the same as we set.
     */
    uint32_t eventId = 0;
    int64_t eventParam = 0;
    auto event = InnerEvent::Get(eventId, eventParam);
    auto id = event->GetInnerEventId();
    auto param = event->GetParam();
    EXPECT_EQ(eventId, id);
    EXPECT_EQ(eventParam, param);
}

/*
 * @tc.name: GetEvent002
 * @tc.desc: get event from pool , set id param and shared pointer object
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEvent002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with event id, object, and param, then get event id, object and param from event.
     * @tc.expected: step1. the event id, object and param is the same as we set.
     */
    uint32_t eventId = 0;
    int64_t eventParam = 0;
    auto object = std::make_shared<uint32_t>();
    auto event = InnerEvent::Get(eventId, object, eventParam);
    auto id = event->GetInnerEventId();
    auto param = event->GetParam();
    auto sharedObject = event->GetSharedObject<uint32_t>();
    EXPECT_EQ(eventId, id);
    EXPECT_EQ(eventParam, param);
    EXPECT_EQ(object, sharedObject);
}

/*
 * @tc.name: GetEvent003
 * @tc.desc: get event from pool , set id param and weak pointer object
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEvent003, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with event id, weak pointer object, and param, then get event id,
     *            shared object and param from event.
     * @tc.expected: step1. the event id, object and param is the same as we set.
     */
    uint32_t eventId = 0;
    int64_t eventParam = 0;
    auto object = std::make_shared<uint32_t>();
    std::weak_ptr<uint32_t> weakObject = object;
    auto event = InnerEvent::Get(eventId, weakObject, eventParam);
    auto id = event->GetInnerEventId();
    auto param = event->GetParam();
    auto sharedObject = event->GetSharedObject<uint32_t>();
    auto weakToSharedObject = weakObject.lock();
    EXPECT_EQ(eventId, id);
    EXPECT_EQ(eventParam, param);
    EXPECT_EQ(weakToSharedObject, sharedObject);
}

/*
 * @tc.name: GetEvent004
 * @tc.desc: get event from pool , set id param and rvalue unique_ptr object
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEvent004, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with event id, rvalue unique_ptr object, and param. then get event id,
     *            unique object and param from event.
     * @tc.expected: step1. the event id, object and param is the same as we set.
     */
    uint32_t eventId = 0;
    int64_t eventParam = 0;
    uint32_t number = 1;
    std::unique_ptr<uint32_t> object = std::make_unique<uint32_t>(number);
    auto event = InnerEvent::Get(eventId, object, eventParam);
    auto id = event->GetInnerEventId();
    auto param = event->GetParam();
    auto uniqueNumber = *(event->GetUniqueObject<uint32_t>());
    EXPECT_EQ(eventId, id);
    EXPECT_EQ(eventParam, param);
    EXPECT_EQ(number, uniqueNumber);
}

/*
 * @tc.name: GetEvent005
 * @tc.desc: get event from pool , set id param and lvalue unique_ptr object
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEvent005, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with event id, lvalue unique_ptr object, and param. then get event id,
     *            unique object and param from event.
     * @tc.expected: step1. the event id, object and param is the same as we set.
     */
    uint32_t eventId = 0;
    int64_t eventParam = 0;
    uint32_t number = 1;
    std::unique_ptr<uint32_t> object = std::make_unique<uint32_t>(number);
    auto event = InnerEvent::Get(eventId, object, eventParam);
    auto id = event->GetInnerEventId();
    auto param = event->GetParam();
    auto uniqueNumber = *(event->GetUniqueObject<uint32_t>());
    EXPECT_EQ(eventId, id);
    EXPECT_EQ(eventParam, param);
    EXPECT_EQ(number, uniqueNumber);
}

/*
 * @tc.name: GetEvent006
 * @tc.desc: get event from pool set task name and callback
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEvent006, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with task and taskname, then get taskname from task and execute task,
     *            check whether the taskname and executed task is the same as we set.
     * @tc.expected: step1. the taskname and executed task is the same as we set.
     */
    TestTaskInfo();
}

/*
 * @tc.name: GetEvent007
 * @tc.desc: Get Unique pointer of saved unique_ptr<T, D> object
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEvent007, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with event id, param, and unique_ptr<T, D> type object. then get event id,
     *            unique_ptr<T, D> type unique object and param from event.
     * @tc.expected: step1. the event id, object and param is the same as we set.
     */
    using deleter = void (*)(uint32_t *);
    uint32_t eventId = 0;
    int64_t eventParam = 0;
    uint32_t number = 1;
    std::unique_ptr<uint32_t, deleter> object(new uint32_t(number), Deleter);
    auto event = InnerEvent::Get(eventId, object, eventParam);
    auto id = event->GetInnerEventId();
    auto param = event->GetParam();
    auto uniqueNumber = *(event->GetUniqueObject<uint32_t, deleter>());
    EXPECT_EQ(eventId, id);
    EXPECT_EQ(eventParam, param);
    EXPECT_EQ(number, uniqueNumber);
}

/*
 * @tc.name: GetEventInfo001
 * @tc.desc: set event owner and get event owner then compare
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEventInfo001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. init handler, get event with event id. then set the handler as the event owner
     *            and get event owner.
     * @tc.expected: step1. the event owner is the same as we set.
     */
    uint32_t eventId = 0;
    auto handler = std::make_shared<EventHandler>();
    auto event = InnerEvent::Get(eventId);
    event->SetOwner(handler);
    auto owner = event->GetOwner();
    EXPECT_EQ(handler, owner);
}

/*
 * @tc.name: GetEventInfo002
 * @tc.desc: set event sendTime and get event sendTime then compare
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEventInfo002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with event id. then set the send time of the event, get send time of the event.
     * @tc.expected: step1. the event send time is the same as we set.
     */
    uint32_t eventId = 0;
    auto event = InnerEvent::Get(eventId);
    InnerEvent::TimePoint now = InnerEvent::Clock::now();
    event->SetSendTime(now);
    auto sendTime = event->GetSendTime();
    EXPECT_EQ(now, sendTime);
}

/*
 * @tc.name: GetEventInfo003
 * @tc.desc: set event handleTime and get event handleTime then compare
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEventInfo003, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with event id. then set the handle time of the event, get handle time of the event.
     * @tc.expected: step1. the event handle time is the same as we set.
     */
    uint32_t eventId = 0;
    auto event = InnerEvent::Get(eventId);
    InnerEvent::TimePoint now = InnerEvent::Clock::now();
    event->SetHandleTime(now);
    auto handleTime = event->GetHandleTime();
    EXPECT_EQ(now, handleTime);
}

/*
 * @tc.name: GetEventInfo004
 * @tc.desc: set event param and get event param then compare
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEventInfo004, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with event id and param. then get event param of the event.
     * @tc.expected: step1. the event param is the same as we set.
     */
    uint32_t eventId = 0;
    int64_t eventParam = 0;
    auto event = InnerEvent::Get(eventId, eventParam);
    auto param = event->GetParam();
    EXPECT_EQ(eventParam, param);
}

/*
 * @tc.name: GetEventInfo005
 * @tc.desc: set event callback taskName and and get event callback taskName then compare
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEventInfo005, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with task and taskname, then get taskname from task and execute task,
     *            check whether the taskname and executed task is the same as we set.
     * @tc.expected: step1. the taskname and executed task is the same as we set.
     */
    TestTaskInfo();
}

/*
 * @tc.name: GetEventInfo006
 * @tc.desc: judge whether the event has a task if it takes a task return true
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEventInfo006, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get event with task and taskname, then use HasTask() to check if the event has a task.
     * @tc.expected: step1. the event we get from event pool has a task.
     */
    string taskName("taskName");
    auto f = []() {};
    auto event = InnerEvent::Get(f, taskName);
    auto whetherHasTask = event->HasTask();
    EXPECT_TRUE(whetherHasTask);
}

/*
 * @tc.name: DrainPool001
 * @tc.desc: get event from pool when the pool is full, then get event from new memory area
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, DrainPool001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. Drain the event pool, make sure event pool is empty
     */
    std::vector<InnerEvent::Pointer> drainPool;
    int64_t param = 0;
    uint32_t eventId = 1;
    for (size_t i = 0; i < MAX_POOL_SIZE; ++i) {
        drainPool.push_back(InnerEvent::Get(eventId));
        ++eventId;
    }

    ++eventId;
    auto event = InnerEvent::Get(eventId, param);

    /**
     * @tc.steps: step2. clear all the event we get from pool, make sure the pool is full.
     */
    drainPool.clear();

    /**
     * @tc.steps: step3. get the event address of the event we get after the event pool is empty,
     *            then reset all the event we get from pool before and get event, compare the two events address.
     * @tc.expected: step3. the two event address are not the same.
     */
    auto firstAddr = event.get();
    event.reset(nullptr);

    auto f = []() {};
    event = InnerEvent::Get(f);
    auto secondAddr = event.get();
    event.reset(nullptr);

    EXPECT_NE(firstAddr, secondAddr);
}

/*
 * @tc.name: DrainPool002
 * @tc.desc: get event from pool when the pool is not full, then get event from new memory area
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, DrainPool002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. Drain the event pool, make sure event pool is empty
     */
    std::vector<InnerEvent::Pointer> drainPool;
    auto param = 0;
    uint32_t eventId = 1;
    for (size_t i = 0; i < MAX_POOL_SIZE; ++i) {
        drainPool.push_back(InnerEvent::Get(eventId));
        ++eventId;
    }

    /**
     * @tc.steps: step2. get event from pool and get the address of the event.
     */
    ++eventId;
    auto event = InnerEvent::Get(eventId, param);
    EXPECT_NE(nullptr, event);
    event.reset(nullptr);

    /**
     * @tc.steps: step3. get another event  from event pool, get the address of the event.
     */
    auto f = []() {};
    event = InnerEvent::Get(f);
    event.reset(nullptr);

    /**
     * @tc.steps: step4. reset the event pool
     */
    for (size_t i = 0; i < MAX_POOL_SIZE; ++i) {
        drainPool.back().reset(nullptr);
    }
}

/*
 * @tc.name: Dump001
 * @tc.desc: Invoke Dump and GetCurrentEventQueue interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, Dump001, TestSize.Level1)
{
    uint32_t eventId = 0;
    int64_t eventParam = 0;
    std::string runnerInfo = "aa";
    auto runner = EventRunner::Create(true);
    runner->DumpRunnerInfo(runnerInfo);
    EventQueueBase queue(EventLockType::STANDARD);
    queue.DumpQueueInfo(runnerInfo);
    auto event = InnerEvent::Get(eventId, eventParam);
    std::string result = event->Dump();
    EXPECT_EQ("Event { No handler }\n", result);
    EXPECT_EQ(runner->GetCurrentEventQueue(), nullptr);
}

/*
 * @tc.name: GetEmitterId001
 * @tc.desc: set emitter id and get emitter id then compare
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, GetEmitterId001, TestSize.Level1)
{
    uint32_t eventId = 0;
    auto event = InnerEvent::Get(eventId);
    uint32_t emitterId = 1;
    event->SetEmitterId(emitterId);
    EXPECT_NE(event, nullptr);
    uint32_t result = event->GetEmitterId();
    EXPECT_EQ(result, emitterId);
}

/*
 * @tc.name: EventRunnerNativeImplement001
 * @tc.desc: Invoke GetEventRunnerNativeObj and CreateEventRunnerNativeObj interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, EventRunnerNativeImplement001, TestSize.Level1)
{
    EventRunnerNativeImplement eventRunnerNativeImplement(true);
    EXPECT_NE(eventRunnerNativeImplement.GetEventRunnerNativeObj(), nullptr);
    EXPECT_NE(eventRunnerNativeImplement.CreateEventRunnerNativeObj(), nullptr);
}

/*
 * @tc.name: EventRunnerNativeImplement002
 * @tc.desc: Invoke RunEventRunnerNativeObj and StopEventRunnerNativeObj interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, EventRunnerNativeImplement002, TestSize.Level1)
{
    EventRunnerNativeImplement eventRunnerNativeImplement(true);
    ErrCode runEventRunnerNativeObj = eventRunnerNativeImplement.RunEventRunnerNativeObj();
    EXPECT_EQ(OHOS::AppExecFwk::EVENT_HANDLER_ERR_NO_EVENT_RUNNER, runEventRunnerNativeObj);
    ErrCode stopEventRunnerNativeObj = eventRunnerNativeImplement.StopEventRunnerNativeObj();
    EXPECT_EQ(OHOS::AppExecFwk::EVENT_HANDLER_ERR_NO_EVENT_RUNNER, stopEventRunnerNativeObj);
}

/*
 * @tc.name: EventRunnerNativeImplement003
 * @tc.desc: InvokeRemoveFileDescriptorListener interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, EventRunnerNativeImplement003, TestSize.Level1)
{
    std::shared_ptr<EventRunnerNativeImplement> eventRunnerNativeImplement =
        std::make_shared<EventRunnerNativeImplement>(true);
    EXPECT_NE(eventRunnerNativeImplement, nullptr);
    eventRunnerNativeImplement->eventRunner_ = EventRunner::Create(false);
    int32_t fileDescriptor = 1;
    eventRunnerNativeImplement->RemoveFileDescriptorListener(fileDescriptor);
}

/*
 * @tc.name: EventRunnerNativeImplement004
 * @tc.desc: AddFileDescriptorListener interface verify whether it is normal
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, EventRunnerNativeImplement004, TestSize.Level1)
{
    std::shared_ptr<EventRunnerNativeImplement> eventRunnerNativeImplement =
        std::make_shared<EventRunnerNativeImplement>(true);
    EXPECT_NE(eventRunnerNativeImplement, nullptr);
    eventRunnerNativeImplement->eventRunner_ = EventRunner::Create(false);
    int32_t fileDescriptor = 1;
    uint32_t events = 2;
    struct FileDescriptorCallbacks P1;
    FileDescriptorCallbacks *fdCallbacks = &P1;
    eventRunnerNativeImplement->AddFileDescriptorListener(fileDescriptor, events, fdCallbacks);
}

/*
 * @tc.name: HandlerEvent001
 * @tc.desc: Execute event by eventhandler
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, HandlerEvent001, TestSize.Level1)
{
    auto runner = EventRunner::Create("Runner");
    EXPECT_NE(runner, nullptr);
    auto handler = std::make_shared<EventHandler>(runner);
    EXPECT_NE(handler, nullptr);
    auto f1 = []() {; };
    auto event = InnerEvent::Get(f1, "event");
    /**
     * @tc.steps: step1. send an event with delay time, then remove this event with event id,
     *                   then check whether the task is executed after delay time.
     * @tc.expected: step1. the task is not executed after delay time.
     */
    bool result = handler->SendEvent(event, 0, EventQueue::Priority::VIP);
    EXPECT_EQ(result, true);

    auto event1 = InnerEvent::Get(f1, "event");
    bool result1 = handler->SendEvent(event1, 0, EventQueue::Priority::IMMEDIATE);
    EXPECT_EQ(result1, true);

    auto event2 = InnerEvent::Get(f1, "event");
    bool result2 = handler->SendEvent(event2, 0, EventQueue::Priority::HIGH);
    EXPECT_EQ(result2, true);

    auto event3 = InnerEvent::Get(f1, "event");
    bool result3 = handler->SendEvent(event3, 0, EventQueue::Priority::LOW);
    EXPECT_EQ(result3, true);

    auto f2 = []() {; };
    auto event4 = InnerEvent::Get(f2, "event");
    bool result4 = handler->SendSyncEvent(event4, EventQueue::Priority::LOW);
    EXPECT_EQ(result4, true);
}

/*
 * @tc.name: HandlerEvent002
 * @tc.desc: Execute event by eventhandler
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, HandlerEvent002, TestSize.Level1)
{
    auto runner = EventRunner::Create("Runner");
    EXPECT_NE(runner, nullptr);
    auto handler = std::make_shared<EventHandler>(runner);
    EXPECT_NE(handler, nullptr);

    auto f = []() {; };
    auto event1 = InnerEvent::Get(f, "event");
    bool result1 = handler->SendSyncEvent(event1, EventQueue::Priority::VIP);
    EXPECT_EQ(result1, true);

    auto event2 = InnerEvent::Get(f, "event");
    bool result2 = handler->SendSyncEvent(event2, EventQueue::Priority::IMMEDIATE);
    EXPECT_EQ(result2, true);

    auto event3 = InnerEvent::Get(f, "event");
    bool result3 = handler->SendSyncEvent(event3, EventQueue::Priority::HIGH);
    EXPECT_EQ(result3, true);

    auto event4 = InnerEvent::Get(f, "event");
    bool result4 = handler->SendSyncEvent(event4, EventQueue::Priority::VIP);
    EXPECT_EQ(result4, true);
}

/*
 * @tc.name: HandlerEvent003
 * @tc.desc: Execute event by eventhandler
 * @tc.type: FUNC
 */
HWTEST_F(LibEventHandlerEventTest, HandlerEvent003, TestSize.Level1)
{
    auto runner = EventRunner::Create("Runner");
    EXPECT_NE(runner, nullptr);
    auto handler = std::make_shared<EventHandler>(runner);
    EXPECT_NE(handler, nullptr);

    /**
     * @tc.steps: step1. HasInnerEvent process
     *
     * @tc.expected: step1. HasInnerEvent process fail.
     */
    uint32_t eventId = 100;
    bool hasInnerEvent = handler->HasInnerEvent(eventId);
    EXPECT_EQ(hasInnerEvent, false);

    auto f = []() {; };
    auto event = InnerEvent::Get(f, "event");
    bool result = handler->SendSyncEvent(event, EventQueue::Priority::VIP);
    EXPECT_EQ(result, true);
}