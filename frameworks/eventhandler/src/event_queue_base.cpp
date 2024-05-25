
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

#include "event_queue_base.h"

#include <algorithm>
#include <iterator>
#include <mutex>

#include "event_handler.h"
#include "event_handler_utils.h"
#include "event_logger.h"
#include "none_io_waiter.h"

namespace OHOS {
namespace AppExecFwk {
namespace {

DEFINE_EH_HILOG_LABEL("EventQueueBase");
constexpr uint32_t MAX_DUMP_SIZE = 500;
// Help to insert events into the event queue sorted by handle time.
void InsertEventsLocked(std::list<InnerEvent::Pointer> &events, InnerEvent::Pointer &event,
    EventInsertType insertType)
{
    if (insertType == EventInsertType::AT_FRONT) {
        if (!events.empty()) {
            // Ensure that events queue is in ordered
            auto headEvent = events.begin();
            if ((*headEvent)->GetHandleTime() < event->GetHandleTime()) {
                event->SetHandleTime((*headEvent)->GetHandleTime());
            }
        }
        events.emplace_front(std::move(event));
        return;
    }

    auto f = [](const InnerEvent::Pointer &first, const InnerEvent::Pointer &second) {
        if (!first || !second) {
            return false;
        }
        return first->GetHandleTime() < second->GetHandleTime();
    };
    auto it = std::upper_bound(events.begin(), events.end(), event, f);
    events.insert(it, std::move(event));
}

// Help to check whether there is a valid event in list and update wake up time.
inline bool CheckEventInListLocked(const std::list<InnerEvent::Pointer> &events, const InnerEvent::TimePoint &now,
    InnerEvent::TimePoint &nextWakeUpTime)
{
    if (!events.empty()) {
        const auto &handleTime = events.front()->GetHandleTime();
        if (handleTime < nextWakeUpTime) {
            nextWakeUpTime = handleTime;
            return handleTime <= now;
        }
    }

    return false;
}

inline InnerEvent::Pointer PopFrontEventFromListLocked(std::list<InnerEvent::Pointer> &events)
{
    InnerEvent::Pointer event = std::move(events.front());
    events.pop_front();
    return event;
}
}  // unnamed namespace

EventQueueBase::EventQueueBase() : EventQueue(), historyEvents_(std::vector<HistoryEvent>(HISTORY_EVENT_NUM_POWER))
{
    HILOGD("enter");
}

EventQueueBase::EventQueueBase(const std::shared_ptr<IoWaiter> &ioWaiter)
    : EventQueue(ioWaiter), historyEvents_(std::vector<HistoryEvent>(HISTORY_EVENT_NUM_POWER))
{
    HILOGD("enter");
}

EventQueueBase::~EventQueueBase()
{
    std::lock_guard<std::mutex> lock(queueLock_);
    usable_.store(false);
    ioWaiter_ = nullptr;
    EH_LOGI_LIMIT("EventQueueBase is unavailable hence");
}

void EventQueueBase::Insert(InnerEvent::Pointer &event, Priority priority, EventInsertType insertType)
{
    if (!event) {
        HILOGE("Could not insert an invalid event");
        return;
    }
    HILOGD("Insert task: %{public}s %{public}d.", (event->GetEventUniqueId()).c_str(), insertType);
    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return;
    }
    bool needNotify = false;
    event->SetEventPriority(static_cast<int32_t>(priority));
    switch (priority) {
        case Priority::VIP:
        case Priority::IMMEDIATE:
        case Priority::HIGH:
        case Priority::LOW: {
            needNotify = (event->GetHandleTime() < wakeUpTime_);
            InsertEventsLocked(subEventQueues_[static_cast<uint32_t>(priority)].queue, event, insertType);
            break;
        }
        case Priority::IDLE: {
            // Never wake up thread if insert an idle event.
            InsertEventsLocked(idleEvents_, event, insertType);
            break;
        }
        default:
            break;
    }

    if (needNotify) {
        ioWaiter_->NotifyOne();
    }
}

void EventQueueBase::RemoveOrphan()
{
    HILOGD("enter");
    // Remove all events which lost its owner.
    auto filter = [](const InnerEvent::Pointer &p) { return !p->GetOwner(); };

    RemoveOrphan(filter);

    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueueBase is unavailable.");
        return;
    }
    RemoveInvalidFileDescriptor();
}


void EventQueueBase::RemoveAll()
{
    HILOGD("enter");
    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueueBase is unavailable.");
        return;
    }
    for (uint32_t i = 0; i < SUB_EVENT_QUEUE_NUM; ++i) {
        subEventQueues_[i].queue.clear();
    }
    idleEvents_.clear();
}

void EventQueueBase::Remove(const std::shared_ptr<EventHandler> &owner)
{
    HILOGD("enter");
    if (!owner) {
        HILOGE("Invalid owner");
        return;
    }

    auto filter = [&owner](const InnerEvent::Pointer &p) { return (p->GetOwner() == owner); };

    Remove(filter);
}

void EventQueueBase::Remove(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId)
{
    HILOGD("enter");
    if (!owner) {
        HILOGE("Invalid owner");
        return;
    }
    auto filter = [&owner, innerEventId](const InnerEvent::Pointer &p) {
        return (!p->HasTask()) && (p->GetOwner() == owner) && (p->GetInnerEventId() == innerEventId);
    };

    Remove(filter);
}

void EventQueueBase::Remove(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId, int64_t param)
{
    HILOGD("enter");
    if (!owner) {
        HILOGE("Invalid owner");
        return;
    }

    auto filter = [&owner, innerEventId, param](const InnerEvent::Pointer &p) {
        return (!p->HasTask()) && (p->GetOwner() == owner) && (p->GetInnerEventId() == innerEventId) &&
               (p->GetParam() == param);
    };

    Remove(filter);
}

bool EventQueueBase::Remove(const std::shared_ptr<EventHandler> &owner, const std::string &name)
{
    HILOGD("enter");
    if ((!owner) || (name.empty())) {
        HILOGE("Invalid owner or task name");
        return false;
    }

    bool removed = false;
    auto filter = [&owner, &name, &removed](const InnerEvent::Pointer &p) {
        if (p == nullptr) {
            return false;
        }
        bool ret = (p->HasTask()) && (p->GetOwner() == owner) && (p->GetTaskName() == name);
        if (!removed) {
            removed = ret;
        }
        return ret;
    };

    Remove(filter);
    return removed;
}

void EventQueueBase::Remove(const RemoveFilter &filter)
{
    HILOGD("enter");
    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueueBase is unavailable.");
        return;
    }
    for (uint32_t i = 0; i < SUB_EVENT_QUEUE_NUM; ++i) {
        subEventQueues_[i].queue.remove_if(filter);
    }
    idleEvents_.remove_if(filter);
}

void EventQueueBase::RemoveOrphan(const RemoveFilter &filter)
{
    std::list<InnerEvent::Pointer> releaseIdleEvents;
    std::array<SubEventQueue, SUB_EVENT_QUEUE_NUM> releaseEventsQueue;
    {
        std::lock_guard<std::mutex> lock(queueLock_);
        if (!usable_.load()) {
            HILOGW("EventQueueBase is unavailable.");
            return;
        }
        for (uint32_t i = 0; i < SUB_EVENT_QUEUE_NUM; ++i) {
            auto it = std::stable_partition(subEventQueues_[i].queue.begin(), subEventQueues_[i].queue.end(), filter);
            std::move(subEventQueues_[i].queue.begin(), it, std::back_inserter(releaseEventsQueue[i].queue));
            subEventQueues_[i].queue.erase(subEventQueues_[i].queue.begin(), it);
        }
        auto idleEventIt = std::stable_partition(idleEvents_.begin(), idleEvents_.end(), filter);
        std::move(idleEvents_.begin(), idleEventIt, std::back_inserter(releaseIdleEvents));
        idleEvents_.erase(idleEvents_.begin(), idleEventIt);
    }
}

bool EventQueueBase::HasInnerEvent(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId)
{
    if (!owner) {
        HILOGE("Invalid owner");
        return false;
    }
    auto filter = [&owner, innerEventId](const InnerEvent::Pointer &p) {
        return (!p->HasTask()) && (p->GetOwner() == owner) && (p->GetInnerEventId() == innerEventId);
    };
    return HasInnerEvent(filter);
}

bool EventQueueBase::HasInnerEvent(const std::shared_ptr<EventHandler> &owner, int64_t param)
{
    if (!owner) {
        HILOGE("Invalid owner");
        return false;
    }
    auto filter = [&owner, param](const InnerEvent::Pointer &p) {
        return (!p->HasTask()) && (p->GetOwner() == owner) && (p->GetParam() == param);
    };
    return HasInnerEvent(filter);
}

bool EventQueueBase::HasInnerEvent(const HasFilter &filter)
{
    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueueBase is unavailable.");
        return false;
    }
    for (uint32_t i = 0; i < SUB_EVENT_QUEUE_NUM; ++i) {
        std::list<InnerEvent::Pointer>::iterator iter =
            std::find_if(subEventQueues_[i].queue.begin(), subEventQueues_[i].queue.end(), filter);
        if (iter != subEventQueues_[i].queue.end()) {
            return true;
        }
    }
    if (std::find_if(idleEvents_.begin(), idleEvents_.end(), filter) != idleEvents_.end()) {
        return true;
    }
    return false;
}

InnerEvent::Pointer EventQueueBase::PickEventLocked(const InnerEvent::TimePoint &now,
    InnerEvent::TimePoint &nextWakeUpTime)
{
    uint32_t priorityIndex = SUB_EVENT_QUEUE_NUM;
    for (uint32_t i = 0; i < SUB_EVENT_QUEUE_NUM; ++i) {
        // Check whether any event need to be distributed.
        if (!CheckEventInListLocked(subEventQueues_[i].queue, now, nextWakeUpTime)) {
            continue;
        }

        // Check whether any event in higher priority need to be distributed.
        if (priorityIndex < SUB_EVENT_QUEUE_NUM) {
            SubEventQueue &subQueue = subEventQueues_[priorityIndex];
            // Check whether enough events in higher priority queue are handled continuously.
            if (subQueue.handledEventsCount < subQueue.maxHandledEventsCount) {
                subQueue.handledEventsCount++;
                break;
            }
        }

        // Try to pick event from this queue.
        priorityIndex = i;
    }

    if (priorityIndex >= SUB_EVENT_QUEUE_NUM) {
        // If not found any event to distribute, return nullptr.
        return InnerEvent::Pointer(nullptr, nullptr);
    }

    // Reset handled event count for sub event queues in higher priority.
    for (uint32_t i = 0; i < priorityIndex; ++i) {
        subEventQueues_[i].handledEventsCount = 0;
    }

    return PopFrontEventFromListLocked(subEventQueues_[priorityIndex].queue);
}

InnerEvent::Pointer EventQueueBase::GetExpiredEventLocked(InnerEvent::TimePoint &nextExpiredTime)
{
    auto now = InnerEvent::Clock::now();
    wakeUpTime_ = InnerEvent::TimePoint::max();
    // Find an event which could be distributed right now.
    InnerEvent::Pointer event = PickEventLocked(now, wakeUpTime_);
    if (event) {
        // Exit idle mode, if found an event to distribute.
        isIdle_ = false;
        currentRunningEvent_ = CurrentRunningEvent(now, event);
        return event;
    }

    // If found nothing, enter idle mode and make a time stamp.
    if (!isIdle_) {
        idleTimeStamp_ = now;
        isIdle_ = true;
    }

    if (!idleEvents_.empty()) {
        const auto &idleEvent = idleEvents_.front();

        // Return the idle event that has been sent before time stamp and reaches its handle time.
        if ((idleEvent->GetSendTime() <= idleTimeStamp_) && (idleEvent->GetHandleTime() <= now)) {
            event = PopFrontEventFromListLocked(idleEvents_);
            currentRunningEvent_ = CurrentRunningEvent(now, event);
            return event;
        }
    }

    // Update wake up time.
    nextExpiredTime = wakeUpTime_;
    currentRunningEvent_ = CurrentRunningEvent();
    return InnerEvent::Pointer(nullptr, nullptr);
}

InnerEvent::Pointer EventQueueBase::GetEvent()
{
    std::unique_lock<std::mutex> lock(queueLock_);
    while (!finished_) {
        InnerEvent::TimePoint nextWakeUpTime = InnerEvent::TimePoint::max();
        InnerEvent::Pointer event = GetExpiredEventLocked(nextWakeUpTime);
        if (event) {
            return event;
        }
        WaitUntilLocked(nextWakeUpTime, lock);
    }

    HILOGD("Break out");
    return InnerEvent::Pointer(nullptr, nullptr);
}

InnerEvent::Pointer EventQueueBase::GetExpiredEvent(InnerEvent::TimePoint &nextExpiredTime)
{
    std::unique_lock<std::mutex> lock(queueLock_);
    return GetExpiredEventLocked(nextExpiredTime);
}

void EventQueueBase::DumpCurrentRunningEventId(const InnerEvent::EventId &innerEventId, std::string &content)
{
    if (innerEventId.index() == TYPE_U32_INDEX) {
        content.append(", id = " + std::to_string(std::get<uint32_t>(innerEventId)));
    } else {
        content.append(", id = " + std::get<std::string>(innerEventId));
    }
}

std::string EventQueueBase::DumpCurrentRunning()
{
    std::string content;
    if (currentRunningEvent_.beginTime_ == InnerEvent::TimePoint::max()) {
        content.append("{}");
    } else {
        content.append("start at " + InnerEvent::DumpTimeToString(currentRunningEvent_.beginTime_) + ", ");
        content.append("Event { ");
        if (!currentRunningEvent_.owner_.expired()) {
            content.append("send thread = " + std::to_string(currentRunningEvent_.senderKernelThreadId_));
            content.append(", send time = " + InnerEvent::DumpTimeToString(currentRunningEvent_.sendTime_));
            content.append(", handle time = " + InnerEvent::DumpTimeToString(currentRunningEvent_.handleTime_));
            if (currentRunningEvent_.hasTask_) {
                content.append(", task name = " + currentRunningEvent_.taskName_);
            } else {
                DumpCurrentRunningEventId(currentRunningEvent_.innerEventId_, content);
            }
            if (currentRunningEvent_.param_ != 0) {
                content.append(", param = " + std::to_string(currentRunningEvent_.param_));
            }
        } else {
            content.append("No handler");
        }
        content.append(" }");
    }

    return content;
}

void EventQueueBase::DumpCurentQueueInfo(Dumper &dumper, uint32_t dumpMaxSize)
{
    std::string priority[] = {"VIP", "Immediate", "High", "Low"};
    uint32_t total = 0;
    for (uint32_t i = 0; i < SUB_EVENT_QUEUE_NUM; ++i) {
        uint32_t n = 0;
        dumper.Dump(dumper.GetTag() + " " + priority[i] + " priority event queue information:" + LINE_SEPARATOR);
        for (auto it = subEventQueues_[i].queue.begin(); it != subEventQueues_[i].queue.end(); ++it) {
            ++n;
            if (total < dumpMaxSize) {
                dumper.Dump(dumper.GetTag() + " No." + std::to_string(n) + " : " + (*it)->Dump());
            }
            ++total;
        }
        dumper.Dump(
            dumper.GetTag() + " Total size of " + priority[i] + " events : " + std::to_string(n) + LINE_SEPARATOR);
    }
    dumper.Dump(dumper.GetTag() + " Idle priority event queue information:" + LINE_SEPARATOR);
    int n = 0;
    for (auto it = idleEvents_.begin(); it != idleEvents_.end(); ++it) {
        ++n;
        if (total < dumpMaxSize) {
            dumper.Dump(dumper.GetTag() + " No." + std::to_string(n) + " : " + (*it)->Dump());
        }
        ++total;
    }
    dumper.Dump(dumper.GetTag() + " Total size of Idle events : " + std::to_string(n) + LINE_SEPARATOR);
    dumper.Dump(dumper.GetTag() + " Total event size : " + std::to_string(total) + LINE_SEPARATOR);
}

void EventQueueBase::Dump(Dumper &dumper)
{
    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return;
    }
    dumper.Dump(dumper.GetTag() + " Current Running: " + DumpCurrentRunning() + LINE_SEPARATOR);
    dumper.Dump(dumper.GetTag() + " History event queue information:" + LINE_SEPARATOR);
    uint32_t dumpMaxSize = MAX_DUMP_SIZE;
    for (uint8_t i = 0; i < HISTORY_EVENT_NUM_POWER; i++) {
        if (historyEvents_[i].senderKernelThreadId == 0) {
            continue;
        }
        --dumpMaxSize;
        dumper.Dump(dumper.GetTag() + " No. " + std::to_string(i) + " : " + HistoryQueueDump(historyEvents_[i]));
    }
    DumpCurentQueueInfo(dumper, dumpMaxSize);
}

void EventQueueBase::DumpQueueInfo(std::string& queueInfo)
{
    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return;
    }
    std::string priority[] = {"VIP", "Immediate", "High", "Low"};
    uint32_t total = 0;
    for (uint32_t i = 0; i < SUB_EVENT_QUEUE_NUM; ++i) {
        uint32_t n = 0;
        queueInfo +=  "            " + priority[i] + " priority event queue:" + LINE_SEPARATOR;
        for (auto it = subEventQueues_[i].queue.begin(); it != subEventQueues_[i].queue.end(); ++it) {
            ++n;
            queueInfo +=  "            No." + std::to_string(n) + " : " + (*it)->Dump();
            ++total;
        }
        queueInfo +=  "              Total size of " + priority[i] + " events : " + std::to_string(n) + LINE_SEPARATOR;
    }

    queueInfo += "            Idle priority event queue:" + LINE_SEPARATOR;

    int n = 0;
    for (auto it = idleEvents_.begin(); it != idleEvents_.end(); ++it) {
        ++n;
        queueInfo += "            No." + std::to_string(n) + " : " + (*it)->Dump();
        ++total;
    }
    queueInfo += "              Total size of Idle events : " + std::to_string(n) + LINE_SEPARATOR;
    queueInfo += "            Total event size : " + std::to_string(total);
}

bool EventQueueBase::IsIdle()
{
    return isIdle_;
}

bool EventQueueBase::IsQueueEmpty()
{
    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return false;
    }
    for (uint32_t i = 0; i < SUB_EVENT_QUEUE_NUM; ++i) {
        uint32_t queueSize = subEventQueues_[i].queue.size();
        if (queueSize != 0) {
            return false;
        }
    }

    return idleEvents_.size() == 0;
}

void EventQueueBase::PushHistoryQueueBeforeDistribute(const InnerEvent::Pointer &event)
{
    if (event == nullptr) {
        HILOGW("event is nullptr.");
        return;
    }
    historyEvents_[historyEventIndex_].senderKernelThreadId = event->GetSenderKernelThreadId();
    historyEvents_[historyEventIndex_].sendTime = event->GetSendTime();
    historyEvents_[historyEventIndex_].handleTime = event->GetHandleTime();
    historyEvents_[historyEventIndex_].triggerTime = InnerEvent::Clock::now();
    historyEvents_[historyEventIndex_].priority = event->GetEventPriority();

    if (event->HasTask()) {
        historyEvents_[historyEventIndex_].hasTask = true;
        historyEvents_[historyEventIndex_].taskName = event->GetTaskName();
    } else {
        historyEvents_[historyEventIndex_].innerEventId = event->GetInnerEventIdEx();
    }
}

void EventQueueBase::PushHistoryQueueAfterDistribute()
{
    historyEvents_[historyEventIndex_].completeTime = InnerEvent::Clock::now();
    historyEventIndex_++;
    historyEventIndex_ = historyEventIndex_ & (HISTORY_EVENT_NUM_POWER - 1);
}

std::string EventQueueBase::HistoryQueueDump(const HistoryEvent &historyEvent)
{
    std::string content;

    content.append("Event { ");
    content.append("send thread = " + std::to_string(historyEvent.senderKernelThreadId));
    content.append(", send time = " + InnerEvent::DumpTimeToString(historyEvent.sendTime));
    content.append(", handle time = " + InnerEvent::DumpTimeToString(historyEvent.handleTime));
    content.append(", trigger time = " + InnerEvent::DumpTimeToString(historyEvent.triggerTime));
    content.append(", completeTime time = " + InnerEvent::DumpTimeToString(historyEvent.completeTime));
    content.append(", prio = " + std::to_string(historyEvent.priority));

    if (historyEvent.hasTask) {
        content.append(", task name = " + historyEvent.taskName);
    } else {
        DumpCurrentRunningEventId(historyEvent.innerEventId, content);
    }
    content.append(" }" + LINE_SEPARATOR);

    return content;
}

std::string EventQueueBase::DumpCurrentQueueSize()
{
    return "Current queue size: IMMEDIATE = " +
    std::to_string(subEventQueues_[static_cast<int>(Priority::IMMEDIATE)].queue.size()) + ", HIGH = " +
    std::to_string(subEventQueues_[static_cast<int>(Priority::HIGH)].queue.size()) + ", LOW = " +
    std::to_string(subEventQueues_[static_cast<int>(Priority::LOW)].queue.size()) + ", IDLE = " +
    std::to_string(idleEvents_.size()) + " ; ";
}

bool EventQueueBase::HasPreferEvent(int basePrio)
{
    for (int prio = 0; prio < basePrio; prio++) {
        if (subEventQueues_[prio].queue.size() > 0) {
            return true;
        }
    }
    return false;
}

CurrentRunningEvent::CurrentRunningEvent()
{
    beginTime_ = InnerEvent::TimePoint::max();
}

CurrentRunningEvent::CurrentRunningEvent(InnerEvent::TimePoint time, InnerEvent::Pointer &event)
{
    beginTime_ = time;
    owner_ = event->GetOwner();
    senderKernelThreadId_ = event->GetSenderKernelThreadId();
    sendTime_ = event->GetSendTime();
    handleTime_ = event->GetHandleTime();
    param_ = event->GetParam();
    if (event->HasTask()) {
        hasTask_ = true;
        taskName_ = event->GetTaskName();
    } else {
        innerEventId_ = event->GetInnerEventIdEx();
    }
}

}  // namespace AppExecFwk
}  // namespace OHOS
