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

#ifndef BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_QUEUE_H
#define BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_QUEUE_H

#include <array>
#include <functional>
#include <list>
#include <map>
#include <mutex>

#include "inner_event.h"
#include "event_handler_errors.h"
#include "file_descriptor_listener.h"
#include "dumper.h"

namespace OHOS {
namespace AppExecFwk {
class IoWaiter;
class EventHandler;
class DeamonIoWaiter;
struct PendingTaskInfo;

enum class EventInsertType: uint32_t {
    // Insert event at end
    AT_END = 0,
    // Insert event at front
    AT_FRONT
};

enum class Observer {
    ARKTS_GC,
};

enum class EventRunnerStage {
    // enter loop
    STAGE_ENTRY_RUNNER = 1<<0,
    // exit loop
    STAGE_EXIT_RUNNER = 1<<1,
    // waiting
    STAGE_BEFORE_WAITING = 1<<2,
    // recover form sleeping
    STAGE_AFTER_WAITING = 1<<3,
    // invaild key
    STAGE_INVAILD = 0,
};

struct StageInfo {
    // STAGE_BEFORE_WAITING, timesteap
    int64_t timestamp;
    // STAGE_BEFORE_WAITING, milliseconds
    int32_t sleepTime;
    // STAGE_AFTER_WAITING
    int reason;
};

using EventRunnerObserverCallBack = std::function<int(EventRunnerStage stage, const StageInfo* info)>;

struct EventRunnerObserver {
    Observer observer;
    uint32_t stages;
    EventRunnerObserverCallBack notifyCb;
    void ClearObserver()
    {
        stages = static_cast<uint32_t>(EventRunnerStage::STAGE_INVAILD);
        notifyCb = nullptr;
    }
};

struct ObserverTrace {
    std::string source;
    std::string stage;
    ObserverTrace() {};
    ObserverTrace(std::string currentSource, std::string currentStage)
        : source(currentSource), stage(currentStage) {}
    std::string getTraceInfo()
    {
        std::string traceInfo;
        traceInfo.append("Et-obs:");
        if (stage.empty()) {
            traceInfo.append(" ");
        } else {
            traceInfo.append(stage);
        }
        traceInfo.append(",");
        if (!source.empty()) {
            traceInfo.append(source);
        }
        return traceInfo;
    }
};

class EventQueue {
public:
    // Priority for the events
    enum class Priority : uint32_t {
        // The highest priority queue, should be distributed until the tasks in the queue are completed.
        VIP = 0,
        // Event that should be distributed at once if possible.
        IMMEDIATE,
        // High priority event, sorted by handle time, should be distributed before low priority event.
        HIGH,
        // Normal event, sorted by handle time.
        LOW,
        // Event that should be distributed only if no other event right now.
        IDLE,
    };

    EventQueue();
    explicit EventQueue(const std::shared_ptr<IoWaiter> &ioWaiter);
    virtual ~EventQueue();
    DISALLOW_COPY_AND_MOVE(EventQueue);

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
    virtual void Insert(InnerEvent::Pointer &event, Priority priority = Priority::LOW,
        EventInsertType insertType = EventInsertType::AT_END) = 0;

    /**
     * Remove events if its owner is invalid, for base queue.
     */
    virtual void RemoveOrphan() {};

    /**
     * Remove events if its owner is invalid, for ffrt queue.
     */
    virtual void RemoveOrphanByHandlerId(const std::string& handlerId) { (void)handlerId; };

    /**
     * Remove all events.
     */
    virtual void RemoveAll() = 0;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     */
    virtual void Remove(const std::shared_ptr<EventHandler> &owner) = 0;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param innerEventId Remove events by event id.
     */
    virtual void Remove(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId) = 0;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param innerEventId Remove events by event id.
     * @param param Remove events by value of param.
     */
    virtual void Remove(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId, int64_t param) = 0;

    /**
     * Remove events with specified requirements.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param name Remove events by name of the task.
     */
    virtual bool Remove(const std::shared_ptr<EventHandler> &owner, const std::string &name) = 0;

    /**
     * Add file descriptor listener for a file descriptor.
     *
     * @param fileDescriptor File descriptor.
     * @param events Events from file descriptor, such as input, output, error
     * @param listener Listener callback.
     * @return Return 'ERR_OK' on success.
     */
    ErrCode AddFileDescriptorListener(int32_t fileDescriptor, uint32_t events,
        const std::shared_ptr<FileDescriptorListener> &listener, const std::string &taskName,
        Priority priority = Priority::HIGH);

    /**
     * Remove all file descriptor listeners for a specified owner.
     *
     * @param owner Owner of the event which is point to an instance of 'FileDescriptorListener'.
     */
    void RemoveFileDescriptorListener(const std::shared_ptr<EventHandler> &owner);

    /**
     * Remove file descriptor listener for a file descriptor.
     *
     * @param fileDescriptor File descriptor.
     */
    void RemoveFileDescriptorListener(int32_t fileDescriptor);

    /**
     * Prepare event queue, before calling {@link #GetEvent}.
     * If {@link #Finish} is called, prepare event queue again, before calling {@link #GetEvent}.
     */
    void Prepare();

    /**
     * Exit from blocking in {@link #GetEvent}, and mark the event queue finished.
     * After calling {@link #Finish}, {@link #GetEvent} never returns any event, until {@link #Prepare} is called.
     */
    void Finish();

    /**
     * Get event from event queue one by one.
     * Before calling this method, developers should call {@link #Prepare} first.
     * If none should be handled right now, the thread will be blocked in this method.
     * Call {@link #Finish} to exit from blocking.
     *
     * @return Returns nullptr if event queue is not prepared yet, or {@link #Finish} is called.
     * Otherwise returns event instance.
     */
    virtual InnerEvent::Pointer GetEvent();

    /**
     * Get expired event from event queue one by one.
     * Before calling this method, developers should call {@link #Prepare} first.
     *
     * @param nextExpiredTime Output the expired time for the next event.
     * @return Returns nullptr if none in event queue is expired.
     * Otherwise returns event instance.
     */
    virtual InnerEvent::Pointer GetExpiredEvent(InnerEvent::TimePoint &nextExpiredTime);

    /**
     * Prints out the internal information about an object in the specified format,
     * helping you diagnose internal errors of the object.
     *
     * @param dumper The Dumper object you have implemented to process the output internal information.
     */
    virtual void Dump(Dumper &dumper) = 0;

    /**
     * Print out the internal information about an object in the specified format,
     * helping you diagnose internal errors of the object.
     *
     * @param queueInfo queue Info.
     */
    virtual void DumpQueueInfo(std::string& queueInfo) = 0;

    /**
     * Checks whether the current EventHandler is idle.
     *
     * @return Returns true if all events have been processed; returns false otherwise.
     */
    virtual bool IsIdle() = 0;

    /**
     * Check whether this event queue is empty.
     *
     * @return If queue is empty return true otherwise return false.
     */
    virtual bool IsQueueEmpty() = 0;

    /**
     * Check whether an event with the given ID can be found among the events that have been sent but not processed.
     *
     * @param owner Owner of the event which is point to an instance of 'EventHandler'.
     * @param innerEventId The id of the event.
     */
    virtual bool HasInnerEvent(const std::shared_ptr<EventHandler> &owner, uint32_t innerEventId) = 0;

    /**
     * Check whether an event carrying the given param can be found among the events that have been sent but not
     * processed.
     *
     * @param owner The owner of the event which is point to an instance of 'EventHandler'.
     * @param param The basic parameter of the event.
     */
    virtual bool HasInnerEvent(const std::shared_ptr<EventHandler> &owner, int64_t param) = 0;

    virtual void PushHistoryQueueBeforeDistribute(const InnerEvent::Pointer &event) { (void)event; }

    virtual void PushHistoryQueueAfterDistribute() {}

    virtual bool HasPreferEvent(int basePrio) = 0;

    virtual std::string DumpCurrentQueueSize() = 0;

    /**
     * Check whether there are currenty file descriptors is need to be processed.
     */
    void CheckFileDescriptorEvent();

    /**
     * Set waiter mode, true for deamon io waiter
     */
    void SetIoWaiter(bool useDeamonIoWaiter)
    {
        useDeamonIoWaiter_ = useDeamonIoWaiter;
    }

    /**
     * Get ffrt queue handler type, only for ffrt thread mode.
     */
    virtual void* GetFfrtQueue() { return nullptr; }

    /**
     * Insert task to ffrt queue, and wait to handled, only for ffrt thread mode.
     */
    virtual void InsertSyncEvent(InnerEvent::Pointer &event, Priority priority = Priority::LOW,
        EventInsertType insertType = EventInsertType::AT_END)
    {
        (void)event;
        (void)priority;
        (void)insertType;
    }

    /**
     * Get pending task info
     */
    virtual PendingTaskInfo QueryPendingTaskInfo(int32_t fileDescriptor) = 0;
    /**
     * add observer
     *
     * @param observer runner observer.
     * @param stages The stages of observer
     * @param callback observer callback.
     */
    void AddObserver(Observer observer, uint32_t stages, EventRunnerObserverCallBack callback)
    {
        observer_.observer = observer;
        observer_.notifyCb = callback;
        observer_.stages = stages;
    }

    /**
     * Cancel And Wait
     */
    virtual void CancelAndWait() = 0;
    
private:

    void HandleFileDescriptorEvent(int32_t fileDescriptor, uint32_t events, const std::string &name,
        Priority priority);
    bool EnsureIoWaiterSupportListerningFileDescriptorLocked();
    bool AddFileDescriptorByFd(int32_t fileDescriptor, uint32_t events, const std::string &taskName,
        const std::shared_ptr<FileDescriptorListener>& listener, EventQueue::Priority priority);
protected:
    void RemoveInvalidFileDescriptor();
    void WaitUntilLocked(const InnerEvent::TimePoint &when, std::unique_lock<std::mutex> &lock);

    std::mutex queueLock_;

    std::atomic_bool usable_ {true};

    bool isIdle_ {true};

    // Mark if the event queue is finished.
    bool finished_ {true};

    // IO waiter used to block if no events while calling 'GetEvent'.
    std::shared_ptr<IoWaiter> ioWaiter_;

    // select different epoll
    bool useDeamonIoWaiter_ = false;

    // File descriptor listeners to handle IO events.
    std::map<int32_t, std::shared_ptr<FileDescriptorListener>> listeners_;

    EventRunnerObserver observer_ = {.stages = static_cast<uint32_t>(EventRunnerStage::STAGE_INVAILD),
        .notifyCb = nullptr};
};
}  // namespace AppExecFwk
}  // namespace OHOS

#endif  // #ifndef BASE_EVENTHANDLER_INTERFACES_INNER_API_EVENT_QUEUE_H
