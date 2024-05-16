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

#include "event_queue.h"

#include <algorithm>
#include <iterator>
#include <mutex>

#include "epoll_io_waiter.h"
#include "event_handler.h"
#include "event_handler_utils.h"
#include "event_logger.h"
#include "none_io_waiter.h"


namespace OHOS {
namespace AppExecFwk {
namespace {

DEFINE_EH_HILOG_LABEL("EventQueue");

// Help to remove file descriptor listeners.
template<typename T>
void RemoveFileDescriptorListenerLocked(std::map<int32_t, std::shared_ptr<FileDescriptorListener>> &listeners,
    const std::shared_ptr<IoWaiter>& ioWaiter, const T &filter, bool useNoWaitEpollWaiter)
{
    if (useNoWaitEpollWaiter && !ioWaiter) {
        return;
    }

    for (auto it = listeners.begin(); it != listeners.end();) {
        if (filter(it->second)) {
            if (useNoWaitEpollWaiter) {
                ioWaiter->RemoveFileDescriptor(it->first);
            } else {
                EpollIoWaiter::GetInstance().RemoveFileDescriptor(it->first);
            }
            it = listeners.erase(it);
        } else {
            ++it;
        }
    }
}

}  // unnamed namespace

EventQueue::EventQueue() : ioWaiter_(std::make_shared<NoneIoWaiter>())
{
    HILOGD("enter");
}

EventQueue::EventQueue(const std::shared_ptr<IoWaiter> &ioWaiter)
    : ioWaiter_(ioWaiter ? ioWaiter : std::make_shared<NoneIoWaiter>())
{
    HILOGD("enter");
}

EventQueue::~EventQueue()
{
    std::lock_guard<std::mutex> lock(queueLock_);
    usable_.store(false);
    ioWaiter_ = nullptr;
    EH_LOGI_LIMIT("EventQueue is unavailable hence");
}

InnerEvent::Pointer EventQueue::GetEvent()
{
    return InnerEvent::Pointer(nullptr, nullptr);
}

InnerEvent::Pointer EventQueue::GetExpiredEvent(InnerEvent::TimePoint &nextExpiredTime)
{
    return InnerEvent::Pointer(nullptr, nullptr);
}

ErrCode EventQueue::AddFileDescriptorListener(int32_t fileDescriptor, uint32_t events,
    const std::shared_ptr<FileDescriptorListener> &listener, const std::string &taskName,
    Priority priority)
{
    if ((fileDescriptor < 0) || ((events & FILE_DESCRIPTOR_EVENTS_MASK) == 0) || (!listener)) {
        HILOGE("%{public}d, %{public}u, %{public}s: Invalid parameter",
               fileDescriptor, events, listener ? "valid" : "null");
        return EVENT_HANDLER_ERR_INVALID_PARAM;
    }

    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return EVENT_HANDLER_ERR_NO_EVENT_RUNNER;
    }
    auto it = listeners_.find(fileDescriptor);
    if (it != listeners_.end()) {
        HILOGE("File descriptor %{public}d is already in listening", fileDescriptor);
        return EVENT_HANDLER_ERR_FD_ALREADY;
    }

    HILOGD("Add file descriptor %{public}d to io waiter %{public}d", fileDescriptor, useNoWaitEpollWaiter_);
    if (!EnsureIoWaiterSupportListerningFileDescriptorLocked()) {
        return EVENT_HANDLER_ERR_FD_NOT_SUPPORT;
    }

    if (!AddFileDescriptorByFd(fileDescriptor, events, taskName, listener, priority)) {
        HILOGE("Failed to add file descriptor into IO waiter");
        return EVENT_HANDLER_ERR_FD_FAILED;
    }

    listeners_.emplace(fileDescriptor, listener);
    return ERR_OK;
}

void EventQueue::RemoveFileDescriptorListener(const std::shared_ptr<EventHandler> &owner)
{
    HILOGD("enter");
    if (!owner) {
        HILOGE("Invalid owner");
        return;
    }

    auto listenerFilter = [&owner](const std::shared_ptr<FileDescriptorListener> &listener) {
        if (!listener) {
            return false;
        }
        return listener->GetOwner() == owner;
    };

    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return;
    }
    RemoveFileDescriptorListenerLocked(listeners_, epollIoWaiter_, listenerFilter, useNoWaitEpollWaiter_);
}

void EventQueue::RemoveFileDescriptorListener(int32_t fileDescriptor)
{
    HILOGD("enter");
    if (fileDescriptor < 0) {
        HILOGE("%{public}d: Invalid file descriptor", fileDescriptor);
        return;
    }

    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return;
    }
    if (listeners_.erase(fileDescriptor) > 0) {
        RemoveFileDescriptorByFd(fileDescriptor);
    }
}

void EventQueue::Prepare()
{
    HILOGD("enter");
    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return;
    }
    finished_ = false;
    epollIoWaiter_ = nullptr;
}

void EventQueue::Finish()
{
    HILOGD("enter");
    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return;
    }
    finished_ = true;
    ioWaiter_->NotifyAll();
}

void EventQueue::HandleFileDescriptorEvent(int32_t fileDescriptor, uint32_t events,
    const std::string &taskName) __attribute__((no_sanitize("cfi")))
{
    std::shared_ptr<FileDescriptorListener> listener;
    {
        std::lock_guard<std::mutex> lock(queueLock_);
        if (!usable_.load()) {
            HILOGW("EventQueue is unavailable.");
            return;
        }
        auto it = listeners_.find(fileDescriptor);
        if (it == listeners_.end()) {
            HILOGW("Can not found listener, maybe it is removed");
            return;
        }
        // Hold instance of listener.
        listener = it->second;
        if (!listener) {
            return;
        }
    }

    auto handler = listener->GetOwner();
    if (!handler) {
        HILOGW("Owner of listener is released");
        return;
    }

    std::weak_ptr<FileDescriptorListener> wp = listener;
    auto f = [fileDescriptor, events, wp]() {
        auto listener = wp.lock();
        if (!listener) {
            HILOGW("Listener is released");
            return;
        }

        if ((events & FILE_DESCRIPTOR_INPUT_EVENT) != 0) {
            listener->OnReadable(fileDescriptor);
        }

        if ((events & FILE_DESCRIPTOR_OUTPUT_EVENT) != 0) {
            listener->OnWritable(fileDescriptor);
        }

        if ((events & FILE_DESCRIPTOR_SHUTDOWN_EVENT) != 0) {
            listener->OnShutdown(fileDescriptor);
        }

        if ((events & FILE_DESCRIPTOR_EXCEPTION_EVENT) != 0) {
            listener->OnException(fileDescriptor);
        }
    };

    // Post a high priority task to handle file descriptor events.
    handler->PostHighPriorityTask(f, taskName);
}

void EventQueue::CheckFileDescriptorEvent()
{
    if (epollIoWaiter_) {
        epollIoWaiter_->WaitForNoWait(0);
    }
}

bool EventQueue::EnsureIoWaiterForDefault()
{
    if (!EpollIoWaiter::GetInstance().Init()) {
        HILOGE("Failed to initialize epoll");
        return false;
    }
    EpollIoWaiter::GetInstance().StartEpollIoWaiter();
    return true;
}

bool EventQueue::EnsureIoWaiterForNoWait()
{
    if (epollIoWaiter_ != nullptr) {
        return true;
    }

    epollIoWaiter_ = std::make_shared<EpollIoWaiter>();
    if (!epollIoWaiter_->Init()) {
        HILOGW("Failed to initialize epoll for no wait.");
        return false;
    }
    return true;
}

bool EventQueue::EnsureIoWaiterSupportListerningFileDescriptorLocked()
{
    HILOGD("enter");
    if (useNoWaitEpollWaiter_) {
        return EnsureIoWaiterForNoWait();
    }
    return EnsureIoWaiterForDefault();
}

void EventQueue::RemoveFileDescriptorByFd(int32_t fileDescriptor)
{
    if (useNoWaitEpollWaiter_) {
        if (epollIoWaiter_) {
            epollIoWaiter_->RemoveFileDescriptor(fileDescriptor);
        }
    } else {
        EpollIoWaiter::GetInstance().RemoveFileDescriptor(fileDescriptor);
    }
}

bool EventQueue::AddFileDescriptorByFd(int32_t fileDescriptor, uint32_t events, const std::string &taskName,
    const std::shared_ptr<FileDescriptorListener>& listener, EventQueue::Priority priority)
{
    if (useNoWaitEpollWaiter_) {
        if (epollIoWaiter_) {
            return epollIoWaiter_->AddFileDescriptorInfo(fileDescriptor, events, taskName, listener, priority);
        }
        return false;
    }
    return EpollIoWaiter::GetInstance().AddFileDescriptorInfo(fileDescriptor, events, taskName,
        listener, priority);
}

void EventQueue::RemoveInvalidFileDescriptor()
{
    // Remove all listeners which lost its owner.
    auto listenerFilter = [](const std::shared_ptr<FileDescriptorListener> &listener) {
        if (!listener) {
            return true;
        }
        HILOGD("Start get to GetOwner");
        return !listener->GetOwner();
    };

    RemoveFileDescriptorListenerLocked(listeners_, epollIoWaiter_, listenerFilter, useNoWaitEpollWaiter_);
}

}  // namespace AppExecFwk
}  // namespace OHOS
