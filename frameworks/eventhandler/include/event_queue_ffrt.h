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

#ifndef BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_QUEUE_FFRT_H
#define BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_QUEUE_FFRT_H

#include "ffrt.h"
#include "event_queue.h"
#include "event_handler.h"

#define LOCAL_API __attribute__((visibility ("hidden")))
namespace OHOS {
namespace AppExecFwk {
class EventHandler;

class EventQueueFFRT : public EventQueue {
public:

    EventQueueFFRT();
    explicit EventQueueFFRT(const std::shared_ptr<IoWaiter> &ioWaiter);
    ~EventQueueFFRT();
    DISALLOW_COPY_AND_MOVE(EventQueueFFRT);

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
    LOCAL_API void Insert(InnerEvent::Pointer &event, Priority priority = Priority::LOW,
        EventInsertType insertType = EventInsertType::AT_END) override;

    /**
     * Remove events if its owner is invalid.
     */
    LOCAL_API void RemoveOrphanByHandlerId(const std::string& handlerId) override;

    /**
     * Remove all events.
     */
    LOCAL_API void RemoveAll() override;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     */
    LOCAL_API void Remove(const std::shared_ptr<EventHandler> &owner) override;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param innerEventId Remove events by event id.
     */
    LOCAL_API void Remove(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId) override;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param innerEventId Remove events by event id.
     * @param param Remove events by value of param.
     */
    LOCAL_API void Remove(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId, int64_t param) override;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param name Remove events by name of the task.
     */
    LOCAL_API bool Remove(const std::shared_ptr<EventHandler> &owner, const std::string &name) override;

    /**
     * Prints out the internal information about an object in the specified format,
     * helping you diagnose internal errors of the object.
     *
     * @param dumper The Dumper object you have implemented to process the output internal information.
     */
    LOCAL_API void Dump(Dumper &dumper) override;

    /**
     * Print out the internal information about an object in the specified format,
     * helping you diagnose internal errors of the object.
     *
     * @param queueInfo queue Info.
     */
    LOCAL_API void DumpQueueInfo(std::string& queueInfo) override;

    /**
     * Checks whether the current EventHandler is idle.
     *
     * @return Returns true if all events have been processed; returns false otherwise.
     */
    LOCAL_API bool IsIdle() override;

    /**
     * Check whether this event queue is empty.
     *
     * @return If queue is empty return true otherwise return false.
     */
    LOCAL_API bool IsQueueEmpty() override;

    /**
     * Check whether an event with the given ID can be found among the events that have been sent but not processed.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param innerEventId The id of the event.
     */
    LOCAL_API bool HasInnerEvent(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId) override;

    /**
     * Check whether an event carrying the given param can be found among the events that have been sent but not
     * processed.
     *
     * @param owner The owner of the event which is point to an instance of 'EventHandler'.
     * @param param The basic parameter of the event.
     */
    LOCAL_API bool HasInnerEvent(const std::shared_ptr<EventHandler> &owner, int64_t param) override;

    LOCAL_API bool HasPreferEvent(int basePrio) override;

    LOCAL_API std::string DumpCurrentQueueSize() override;

    LOCAL_API void* GetFfrtQueue() override;

    /**
     * Insert task to ffrt queue, and wait to handled, only for ffrt thread mode.
     */
    LOCAL_API void InsertSyncEvent(InnerEvent::Pointer &event, Priority priority = Priority::LOW,
        EventInsertType insertType = EventInsertType::AT_END) override;

    LOCAL_API PendingTaskInfo QueryPendingTaskInfo(int32_t fileDescriptor) override;

    /**
     * Cancel And Wait.
     */
    LOCAL_API void CancelAndWait() override;

private:
    LOCAL_API void InsertEvent(InnerEvent::Pointer &event, Priority priority = Priority::LOW, bool syncWait = false,
        EventInsertType insertType = EventInsertType::AT_END);
    LOCAL_API void SubmitEventAtEnd(InnerEvent::Pointer &event, Priority priority, bool syncWait,
        const std::string &taskName, std::unique_lock<std::mutex> &lock);
    LOCAL_API void SubmitEventAtFront(InnerEvent::Pointer &event, Priority priority, bool syncWait,
        const std::string &taskName, std::unique_lock<std::mutex> &lock);

    std::shared_ptr<ffrt::queue> ffrtQueue_ = nullptr;
};
}  // namespace AppExecFwk
}  // namespace OHOS

#endif  // #ifndef BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_QUEUE_FFRT_H
