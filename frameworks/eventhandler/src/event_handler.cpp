/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include <unistd.h>
#include <sys/syscall.h>
#include "event_handler_utils.h"
#include "event_logger.h"
#ifdef HAS_HICHECKER_NATIVE_PART
#include "hichecker.h"
#endif // HAS_HICHECKER_NATIVE_PART
#include "thread_local_data.h"

using namespace OHOS::HiviewDFX;
namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventHandler");
}
thread_local std::weak_ptr<EventHandler> EventHandler::currentEventHandler;

std::shared_ptr<EventHandler> EventHandler::Current()
{
    return currentEventHandler.lock();
}

EventHandler::EventHandler(const std::shared_ptr<EventRunner> &runner) : eventRunner_(runner)
{
    HILOGD("enter");
}

EventHandler::~EventHandler()
{
    HILOGI("enter");
    if (eventRunner_) {
        HILOGI("eventRunner is alive");
        /*
         * This handler is finishing, need to remove all events belong to it.
         * But events only have weak pointer of this handler,
         * now weak pointer is invalid, so these events become orphans.
         */
        eventRunner_->GetEventQueue()->RemoveOrphan();
    }
}

bool EventHandler::SendEvent(InnerEvent::Pointer &event, int64_t delayTime, Priority priority)
{
    if (!event) {
        HILOGE("Could not send an invalid event");
        return false;
    }

    if (!eventRunner_) {
        HILOGE("MUST Set event runner before sending events");
        return false;
    }

    InnerEvent::TimePoint now = InnerEvent::Clock::now();
    event->SetSendTime(now);
    event->SetSenderKernelThreadId(syscall(__NR_gettid));
    event->SetEventUniqueId();
    if (delayTime > 0) {
        event->SetHandleTime(now + std::chrono::milliseconds(delayTime));
    } else {
        event->SetHandleTime(now);
    }

    event->SetOwner(shared_from_this());
    // get traceId from event, if HiTraceChain::begin has been called, would get a valid trace id.
    auto traceId = event->GetOrCreateTraceId();
    // if traceId is valid, out put trace information
    if (AllowHiTraceOutPut(traceId, event->HasWaiter())) {
        HiTracePointerOutPut(traceId, event, "Send", HiTraceTracepointType::HITRACE_TP_CS);
    }
    HILOGD("Current event id is %{public}s .", (event->GetEventUniqueId()).c_str());
    eventRunner_->GetEventQueue()->Insert(event, priority);
    return true;
}

bool EventHandler::SendTimingEvent(InnerEvent::Pointer &event, int64_t taskTime, Priority priority)
{
    InnerEvent::TimePoint nowSys = InnerEvent::Clock::now();
    auto epoch = nowSys.time_since_epoch();
    long nowSysTime = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    int64_t delayTime = taskTime - nowSysTime;
    if (delayTime < 0) {
        HILOGW("SendTime is before now systime, change to 0 delaytime Event");
        return SendEvent(event, 0, priority);
    }

    return SendEvent(event, delayTime, priority);
}

bool EventHandler::SendSyncEvent(InnerEvent::Pointer &event, Priority priority)
{
    if ((!event) || (priority == Priority::IDLE)) {
        HILOGE("Could not send an invalid event or idle event");
        return false;
    }

    if ((!eventRunner_) || (!eventRunner_->IsRunning())) {
        HILOGE("MUST Set a running event runner before sending sync events");
        return false;
    }

    // If send a sync event in same event runner, distribute here.
    if (eventRunner_ == EventRunner::Current()) {
        DistributeEvent(event);
        return true;
    }

    // get traceId from event, if HiTraceChain::begin has been called, would get a valid trace id.
    auto spanId = event->GetOrCreateTraceId();

    // Create waiter, used to block.
    auto waiter = event->CreateWaiter();
    // Send this event as normal one.
    if (!SendEvent(event, 0, priority)) {
        HILOGE("SendEvent is failed");
        return false;
    }
    // Wait until event is processed(recycled).
    waiter->Wait();

    if ((spanId) && (spanId->IsValid())) {
        HiTraceChain::Tracepoint(HiTraceTracepointType::HITRACE_TP_CR, *spanId, "event is processed");
    }

    return true;
}

void EventHandler::RemoveAllEvents()
{
    HILOGD("enter");
    if (!eventRunner_) {
        HILOGE("MUST Set event runner before removing all events");
        return;
    }

    eventRunner_->GetEventQueue()->Remove(shared_from_this());
}

void EventHandler::RemoveEvent(uint32_t innerEventId)
{
    HILOGD("enter");
    if (!eventRunner_) {
        HILOGE("MUST Set event runner before removing events by id");
        return;
    }

    eventRunner_->GetEventQueue()->Remove(shared_from_this(), innerEventId);
}

void EventHandler::RemoveEvent(uint32_t innerEventId, int64_t param)
{
    HILOGD("enter");
    if (!eventRunner_) {
        HILOGE("MUST Set event runner before removing events by id and param");
        return;
    }

    eventRunner_->GetEventQueue()->Remove(shared_from_this(), innerEventId, param);
}

void EventHandler::RemoveTask(const std::string &name)
{
    HILOGD("enter");
    if (!eventRunner_) {
        HILOGE("MUST Set event runner before removing events by task name");
        return;
    }

    eventRunner_->GetEventQueue()->Remove(shared_from_this(), name);
}

ErrCode EventHandler::AddFileDescriptorListener(int32_t fileDescriptor, uint32_t events,
    const std::shared_ptr<FileDescriptorListener> &listener, const std::string &taskName)
{
    HILOGD("enter");
    if ((fileDescriptor < 0) || ((events & FILE_DESCRIPTOR_EVENTS_MASK) == 0) || (!listener)) {
        HILOGE("%{public}d, %{public}u, %{public}s: Invalid parameter",
               fileDescriptor, events, listener ? "valid" : "null");
        return EVENT_HANDLER_ERR_INVALID_PARAM;
    }

    if (!eventRunner_) {
        HILOGE("MUST Set event runner before adding fd listener");
        return EVENT_HANDLER_ERR_NO_EVENT_RUNNER;
    }

    listener->SetOwner(shared_from_this());
    return eventRunner_->GetEventQueue()->AddFileDescriptorListener(fileDescriptor, events, listener, taskName);
}

void EventHandler::RemoveAllFileDescriptorListeners()
{
    HILOGD("enter");
    if (!eventRunner_) {
        HILOGE("MUST Set event runner before removing all fd listener");
        return;
    }

    eventRunner_->GetEventQueue()->RemoveFileDescriptorListener(shared_from_this());
}

void EventHandler::RemoveFileDescriptorListener(int32_t fileDescriptor)
{
    HILOGD("enter");
    if (fileDescriptor < 0) {
        HILOGE("fd %{public}d: Invalid parameter", fileDescriptor);
        return;
    }

    if (!eventRunner_) {
        HILOGE("MUST Set event runner before removing fd listener by fd");
        return;
    }

    eventRunner_->GetEventQueue()->RemoveFileDescriptorListener(fileDescriptor);
}

void EventHandler::SetEventRunner(const std::shared_ptr<EventRunner> &runner)
{
    HILOGD("enter");
    if (eventRunner_ == runner) {
        return;
    }

    if (eventRunner_) {
        HILOGW("It is not recommended to change the event runner dynamically");

        // Remove all events and listeners from old event runner.
        RemoveAllEvents();
        RemoveAllFileDescriptorListeners();
    }

    // Switch event runner.
    eventRunner_ = runner;
    return;
}

void EventHandler::DeliveryTimeAction(const InnerEvent::Pointer &event, InnerEvent::TimePoint nowStart)
{
#ifdef HAS_HICHECKER_NATIVE_PART
    HILOGD("enter");
    if (!HiChecker::NeedCheckSlowEvent()) {
        return;
    }
    int64_t deliveryTimeout = eventRunner_->GetDeliveryTimeout();
    if (deliveryTimeout > 0) {
        std::string threadName = eventRunner_->GetRunnerThreadName();
        std::string eventName = GetEventName(event);
        int64_t threadId = gettid();
        std::string threadIdCharacter = std::to_string(threadId);
        std::chrono::duration<double> deliveryTime = nowStart - event->GetSendTime();
        std::string deliveryTimeCharacter = std::to_string((deliveryTime).count());
        std::string deliveryTimeoutCharacter = std::to_string(deliveryTimeout);
        std::string handOutTag = "threadId: " + threadIdCharacter + "," + "threadName: " + threadName + "," +
            "eventName: " + eventName + "," + "deliveryTime: " + deliveryTimeCharacter + "," +
            "deliveryTimeout: " + deliveryTimeoutCharacter;
        if ((nowStart - std::chrono::milliseconds(deliveryTimeout)) > event->GetHandleTime()) {
            HiChecker::NotifySlowEvent(handOutTag);
            if (deliveryTimeoutCallback_) {
                deliveryTimeoutCallback_();
            }
        }
    }
#endif // HAS_HICHECKER_NATIVE_PART
}

void EventHandler::DistributeTimeAction(const InnerEvent::Pointer &event, InnerEvent::TimePoint nowStart)
{
#ifdef HAS_HICHECKER_NATIVE_PART
    HILOGD("enter");
    if (!HiChecker::NeedCheckSlowEvent()) {
        return;
    }
    int64_t distributeTimeout = eventRunner_->GetDistributeTimeout();
    if (distributeTimeout > 0) {
        std::string threadName = eventRunner_->GetRunnerThreadName();
        std::string eventName = GetEventName(event);
        int64_t threadId = gettid();
        std::string threadIdCharacter = std::to_string(threadId);
        InnerEvent::TimePoint nowEnd = InnerEvent::Clock::now();
        std::chrono::duration<double> distributeTime = nowEnd - nowStart;
        std::string distributeTimeCharacter = std::to_string((distributeTime).count());
        std::string distributeTimeoutCharacter = std::to_string(distributeTimeout);
        std::string executeTag = "threadId: " + threadIdCharacter + "," + "threadName: " + threadName + "," +
            "eventName: " + eventName + "," + "distributeTime: " + distributeTimeCharacter + "," +
            "distributeTimeout: " + distributeTimeoutCharacter;
        if ((nowEnd - std::chrono::milliseconds(distributeTimeout)) > nowStart) {
            HiChecker::NotifySlowEvent(executeTag);
            if (distributeTimeoutCallback_) {
                distributeTimeoutCallback_();
            }
        }
    }
#endif // HAS_HICHECKER_NATIVE_PART
}

void EventHandler::DistributeEvent(const InnerEvent::Pointer &event)
{
    if (!event) {
        HILOGE("Could not distribute an invalid event");
        return;
    }

    currentEventHandler = shared_from_this();
    if (enableEventLog_) {
        auto now = InnerEvent::Clock::now();
        auto currentRunningInfo = "start at " + InnerEvent::DumpTimeToString(now) + "; " + event->Dump() +
        eventRunner_->GetEventQueue()->DumpCurrentQueueSize();
        HILOGD("%{public}s", currentRunningInfo.c_str());
    }

    auto spanId = event->GetTraceId();
    auto traceId = HiTraceChain::GetId();
    bool allowTraceOutPut = AllowHiTraceOutPut(spanId, event->HasWaiter());
    if (allowTraceOutPut) {
        HiTraceChain::SetId(*spanId);
        HiTracePointerOutPut(spanId, event, "Receive", HiTraceTracepointType::HITRACE_TP_SR);
    }

    InnerEvent::TimePoint nowStart = InnerEvent::Clock::now();
    DeliveryTimeAction(event, nowStart);
    HILOGD("EventName is %{public}s, eventId is %{public}s .", GetEventName(event).c_str(),
        (event->GetEventUniqueId()).c_str());
    if (event->HasTask()) {
        // Call task callback directly if contains a task.
        (event->GetTaskCallback())();
    } else {
        // Otherwise let developers to handle it.
        ProcessEvent(event);
    }

    DistributeTimeAction(event, nowStart);

    if (allowTraceOutPut) {
        HiTraceChain::Tracepoint(HiTraceTracepointType::HITRACE_TP_SS, *spanId, "Event Distribute over");
        HiTraceChain::ClearId();
        if (traceId.IsValid()) {
            HiTraceChain::SetId(traceId);
        }
    }
    if (enableEventLog_) {
        auto now = InnerEvent::Clock::now();
        HILOGD("end at %{public}s", InnerEvent::DumpTimeToString(now).c_str());
    }
}

void EventHandler::Dump(Dumper &dumper)
{
    auto now = std::chrono::system_clock::now();
    dumper.Dump(dumper.GetTag() + " EventHandler dump begin curTime: " +
        InnerEvent::DumpTimeToString(now) + LINE_SEPARATOR);
    if (eventRunner_ == nullptr) {
        dumper.Dump(dumper.GetTag() + " event runner uninitialized!" + LINE_SEPARATOR);
    } else {
        eventRunner_->Dump(dumper);
    }
}

bool EventHandler::HasInnerEvent(uint32_t innerEventId)
{
    if (!eventRunner_) {
        HILOGE("event runner uninitialized!");
        return false;
    }
    return eventRunner_->GetEventQueue()->HasInnerEvent(shared_from_this(), innerEventId);
}

bool EventHandler::HasInnerEvent(int64_t param)
{
    if (!eventRunner_) {
        HILOGE("event runner uninitialized!");
        return false;
    }
    return eventRunner_->GetEventQueue()->HasInnerEvent(shared_from_this(), param);
}

std::string EventHandler::GetEventName(const InnerEvent::Pointer &event)
{
    std::string eventName;
    if (!event) {
        HILOGW("event is nullptr");
        return eventName;
    }

    if (event->HasTask()) {
        eventName = event->GetTaskName();
    } else {
        InnerEvent::EventId eventId = event->GetInnerEventIdEx();
        if (eventId.index() == TYPE_U32_INDEX) {
            eventName = std::to_string(std::get<uint32_t>(eventId));
        } else {
            eventName = std::get<std::string>(eventId);
        }
    }
    return eventName;
}

bool EventHandler::IsIdle()
{
    return eventRunner_->GetEventQueue()->IsIdle();
}

void EventHandler::ProcessEvent(const InnerEvent::Pointer &)
{}

void EventHandler::EnableEventLog(bool enableEventLog)
{
    enableEventLog_ = enableEventLog;
}
}  // namespace AppExecFwk
}  // namespace OHOS
