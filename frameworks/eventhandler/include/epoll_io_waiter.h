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

#ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_EPOLL_IO_WAITER_H
#define BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_EPOLL_IO_WAITER_H

#include <atomic>
#include <map>
#include <mutex>

#include <sys/epoll.h>
#include "io_waiter.h"
#include "nocopyable.h"
#include "event_queue.h"
#include "file_descriptor_listener.h"

namespace OHOS {
namespace AppExecFwk {
class EventHandler;

class FileDescriptorInfo {
public:
    DISALLOW_COPY_AND_MOVE(FileDescriptorInfo);
    FileDescriptorInfo() {}
    FileDescriptorInfo(std::string taskName, EventQueue::Priority priority,
        std::shared_ptr<FileDescriptorListener>listener): taskName_(taskName), priority_(priority),
        listener_(listener) {}
    std::string taskName_;
    EventQueue::Priority priority_;
    std::shared_ptr<FileDescriptorListener> listener_;
};

// Use epoll to listen file descriptor.
class EpollIoWaiter final : public IoWaiter {
public:
    EpollIoWaiter();
    ~EpollIoWaiter() final;
    static EpollIoWaiter& GetInstance();
    DISALLOW_COPY_AND_MOVE(EpollIoWaiter);

    /**
     * Initialize epoll.
     *
     * @return True if succeeded.
     */
    bool Init();

    bool WaitFor(std::unique_lock<std::mutex> &lock, int64_t nanoseconds) final;

    void NotifyOne() final;
    void NotifyAll() final;
    void StopEpollIoWaiter();
    void StartEpollIoWaiter();
    bool SupportListeningFileDescriptor() const final;

    bool AddFileDescriptor(int32_t fileDescriptor, uint32_t events, const std::string &taskName) final;
    void RemoveFileDescriptor(int32_t fileDescriptor) final;

    void SetFileDescriptorEventCallback(const FileDescriptorEventCallback &callback) final;
    void InsertFileDescriptorMap(int32_t fileDescriptor, const std::string& taskName,
        EventQueue::Priority priority, const std::shared_ptr<FileDescriptorListener>& listener);
    void EraseFileDescriptorMap(int32_t fileDescriptor);
    std::shared_ptr<FileDescriptorInfo> GetFileDescriptorMap(int32_t fileDescriptor);
    bool AddFileDescriptorInfo(int32_t fileDescriptor, uint32_t events, const std::string &taskName,
        const std::shared_ptr<FileDescriptorListener>& listener, EventQueue::Priority priority);
    void HandleFileDescriptorEvent(int32_t fileDescriptor, uint32_t events);
private:
    void EpollWaitFor();
    void DrainAwakenPipe() const;
    void HandleEpollEvents(struct epoll_event *epollEvents, int32_t eventsCount);

    // File descriptor for epoll.
    int32_t epollFd_{-1};
    // File descriptor used to wake up epoll.
    int32_t awakenFd_{-1};
    std::mutex fileDescriptorMapLock;
    FileDescriptorEventCallback callback_;
    std::atomic<int32_t> waitingCount_{0};
    std::atomic<bool> running_ = false;
    std::atomic<bool> isFinished_ = false;
    std::unique_ptr<std::thread> epollThread_;
    std::map<int32_t, std::shared_ptr<FileDescriptorInfo>> fileDescriptorMap_;
};
}  // namespace AppExecFwk
}  // namespace OHOS

#endif  // #ifndef BASE_EVENTHANDLER_FRAMEWORKS_EVENTHANDLER_INCLUDE_EPOLL_IO_WAITER_H
