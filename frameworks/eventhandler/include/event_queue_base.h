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

#ifndef BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_QUEUE_BASE_H
#define BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_QUEUE_BASE_H

#include <array>
#include <list>
#include <map>
#include <mutex>

#include "event_queue.h"

namespace OHOS {
namespace AppExecFwk {
class IoWaiter;
class EventHandler;
class EpollIoWaiter;

struct CurrentRunningEvent {
    InnerEvent::TimePoint beginTime_;
    std::weak_ptr<EventHandler> owner_;
    uint64_t senderKernelThreadId_{0};
    InnerEvent::TimePoint sendTime_;
    InnerEvent::TimePoint handleTime_;
    int64_t param_{0};
    bool hasTask_{false};
    std::string taskName_;
    InnerEvent::EventId innerEventId_ = 0u;
    CurrentRunningEvent();
    CurrentRunningEvent(InnerEvent::TimePoint time, InnerEvent::Pointer &event);
};

class EventQueueBase : public EventQueue {
public:

    EventQueueBase();
    explicit EventQueueBase(const std::shared_ptr<IoWaiter> &ioWaiter);
    ~EventQueueBase();
    DISALLOW_COPY_AND_MOVE(EventQueueBase);

    /**
     * Insert an event into event queue with different priority.
     * The events will be sorted by handle time.
     *
     * @param event Event instance which should be added into event queue.
     * @param Priority Priority of the event
     * @param insertType The type of insertint event to queue
     *
     * @see #Priority
     */
    void Insert(InnerEvent::Pointer &event, Priority priority = Priority::LOW,
        EventInsertType insertType = EventInsertType::AT_END) override;

    /**
     * Remove events if its owner is invalid.
     */
    void RemoveOrphan() override;

    /**
     * Remove all events.
     */
    void RemoveAll() override;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     */
    void Remove(const std::shared_ptr<EventHandler> &owner) override;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param innerEventId Remove events by event id.
     */
    void Remove(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId) override;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param innerEventId Remove events by event id.
     * @param param Remove events by value of param.
     */
    void Remove(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId, int64_t param) override;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param name Remove events by name of the task.
     */
    bool Remove(const std::shared_ptr<EventHandler> &owner, const std::string &name) override;

    /**
     * Get event from event queue one by one.
     * Before calling this method, developers should call {@link #Prepare} first.
     * If none should be handled right now, the thread will be blocked in this method.
     * Call {@link #Finish} to exit from blocking.
     *
     * @return Returns nullptr if event queue is not prepared yet, or {@link #Finish} is called.
     * Otherwise returns event instance.
     */
    InnerEvent::Pointer GetEvent() override;

    /**
     * Get expired event from event queue one by one.
     * Before calling this method, developers should call {@link #Prepare} first.
     *
     * @param nextExpiredTime Output the expired time for the next event.
     * @return Returns nullptr if none in event queue is expired.
     * Otherwise returns event instance.
     */
    InnerEvent::Pointer GetExpiredEvent(InnerEvent::TimePoint &nextExpiredTime) override;

    /**
     * Prints out the internal information about an object in the specified format,
     * helping you diagnose internal errors of the object.
     *
     * @param dumper The Dumper object you have implemented to process the output internal information.
     */
    void Dump(Dumper &dumper) override;

    /**
     * Print out the internal information about an object in the specified format,
     * helping you diagnose internal errors of the object.
     *
     * @param queueInfo queue Info.
     */
    void DumpQueueInfo(std::string& queueInfo) override;

    /**
     * Checks whether the current EventHandler is idle.
     *
     * @return Returns true if all events have been processed; returns false otherwise.
     */
    bool IsIdle() override;

    /**
     * Check whether this event queue is empty.
     *
     * @return If queue is empty return true otherwise return false.
     */
    bool IsQueueEmpty() override;

    /**
     * Check whether an event with the given ID can be found among the events that have been sent but not processed.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param innerEventId The id of the event.
     */
    bool HasInnerEvent(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId) override;

    /**
     * Check whether an event carrying the given param can be found among the events that have been sent but not
     * processed.
     *
     * @param owner The owner of the event which is point to an instance of 'EventHandler'.
     * @param param The basic parameter of the event.
     */
    bool HasInnerEvent(const std::shared_ptr<EventHandler> &owner, int64_t param) override;

    void PushHistoryQueueBeforeDistribute(const InnerEvent::Pointer &event) override;

    void PushHistoryQueueAfterDistribute() override;

    bool HasPreferEvent(int basePrio) override;

    std::string DumpCurrentQueueSize() override;

private:
    using RemoveFilter = std::function<bool(const InnerEvent::Pointer &)>;
    using HasFilter = std::function<bool(const InnerEvent::Pointer &)>;

    struct HistoryEvent {
        uint64_t senderKernelThreadId{0};
        std::string taskName;
        InnerEvent::EventId innerEventId = 0u;
        bool hasTask{false};
        InnerEvent::TimePoint sendTime;
        InnerEvent::TimePoint handleTime;
        InnerEvent::TimePoint triggerTime;
        InnerEvent::TimePoint completeTime;
        int32_t priority = -1;
    };

    /*
     * To avoid starvation of lower priority event queue, give a chance to process lower priority events,
     * after continuous processing several higher priority events.
     */
    static const uint32_t DEFAULT_MAX_HANDLED_EVENT_COUNT = 5;

    // Sub event queues for IMMEDIATE, HIGH and LOW priority. So use value of IDLE as size.
    static const uint32_t SUB_EVENT_QUEUE_NUM = static_cast<uint32_t>(Priority::IDLE);

    struct SubEventQueue {
        std::list<InnerEvent::Pointer> queue;
        uint32_t handledEventsCount{0};
        uint32_t maxHandledEventsCount{DEFAULT_MAX_HANDLED_EVENT_COUNT};
    };

    void Remove(const RemoveFilter &filter);
    void RemoveOrphan(const RemoveFilter &filter);
    bool HasInnerEvent(const HasFilter &filter);
    InnerEvent::Pointer PickEventLocked(const InnerEvent::TimePoint &now, InnerEvent::TimePoint &nextWakeUpTime);
    InnerEvent::Pointer GetExpiredEventLocked(InnerEvent::TimePoint &nextExpiredTime);
    std::string HistoryQueueDump(const HistoryEvent &historyEvent);
    std::string DumpCurrentRunning();
    void DumpCurrentRunningEventId(const InnerEvent::EventId &innerEventId, std::string &content);
    void DumpCurentQueueInfo(Dumper &dumper, uint32_t dumpMaxSize);

    // Sub event queues for different priority.
    std::array<SubEventQueue, SUB_EVENT_QUEUE_NUM> subEventQueues_;

    // Event queue for IDLE events.
    std::list<InnerEvent::Pointer> idleEvents_;

    // Next wake up time when block in 'GetEvent'.
    InnerEvent::TimePoint wakeUpTime_ { InnerEvent::TimePoint::max() };

    // Mark if in idle mode, and record the start time of idle.
    InnerEvent::TimePoint idleTimeStamp_ { InnerEvent::Clock::now() };

    bool isIdle_ {true};

    // current running event info
    CurrentRunningEvent currentRunningEvent_;

    static const uint8_t HISTORY_EVENT_NUM_POWER = 32; // 必须是2的幂次，使用位运算取余
    std::vector<HistoryEvent> historyEvents_;
    uint8_t historyEventIndex_ = 0;
};
}  // namespace AppExecFwk
}  // namespace OHOS

#endif  // #ifndef BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_QUEUE_BASE_H
