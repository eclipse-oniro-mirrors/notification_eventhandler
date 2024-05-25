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

#include "deamon_io_waiter.h"
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
    const std::shared_ptr<IoWaiter>& ioWaiter, const T &filter, bool useDeamonIoWaiter_)
{
    if (!useDeamonIoWaiter_ && !ioWaiter) {
        return;
    }

    for (auto it = listeners.begin(); it != listeners.end();) {
        if (filter(it->second)) {
            if (useDeamonIoWaiter_) {
                DeamonIoWaiter::GetInstance().RemoveFileDescriptor(it->first);
            } else {
                ioWaiter->RemoveFileDescriptor(it->first);
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
    if (ioWaiter_->SupportListeningFileDescriptor()) {
        // Set callback to handle events from file descriptors.
        ioWaiter_->SetFileDescriptorEventCallback(
            std::bind(&EventQueue::HandleFileDescriptorEvent, this, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
    }
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

bool EventQueue::AddFileDescriptorByFd(int32_t fileDescriptor, uint32_t events, const std::string &taskName,
    const std::shared_ptr<FileDescriptorListener>& listener, EventQueue::Priority priority)
{
    if (useDeamonIoWaiter_) {
        return DeamonIoWaiter::GetInstance().AddFileDescriptor(fileDescriptor, events, taskName,
            listener, priority);
    }
    if (ioWaiter_) {
        return ioWaiter_->AddFileDescriptor(fileDescriptor, events, taskName, listener, priority);
    }
    return false;
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

    HILOGD("Add file descriptor %{public}d to io waiter %{public}d", fileDescriptor, useDeamonIoWaiter_);
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
    RemoveFileDescriptorListenerLocked(listeners_, ioWaiter_, listenerFilter, useDeamonIoWaiter_);
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
        if (useDeamonIoWaiter_) {
            DeamonIoWaiter::GetInstance().RemoveFileDescriptor(fileDescriptor);
            return;
        }
        if (ioWaiter_) {
            ioWaiter_->RemoveFileDescriptor(fileDescriptor);
        }
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

void EventQueue::WaitUntilLocked(const InnerEvent::TimePoint &when, std::unique_lock<std::mutex> &lock)
{
    // Get a temp reference of IO waiter, otherwise it maybe released while waiting.
    auto ioWaiterHolder = ioWaiter_;
    if (!ioWaiterHolder->WaitFor(lock, TimePointToTimeOut(when))) {
        HILOGE("Failed to call wait, reset IO waiter");
        ioWaiter_ = std::make_shared<NoneIoWaiter>();
        listeners_.clear();
    }
}

void EventQueue::HandleFileDescriptorEvent(int32_t fileDescriptor, uint32_t events,
    const std::string &taskName, Priority priority) __attribute__((no_sanitize("cfi")))
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

    HILOGD("Post fd %{public}d, task %{public}s, priority %{public}d.", fileDescriptor,
        taskName.c_str(), priority);
    // Post a high priority task to handle file descriptor events.
    handler->PostTask(f, taskName, 0, priority);
}

void EventQueue::CheckFileDescriptorEvent()
{
    InnerEvent::TimePoint now = InnerEvent::Clock::now();
    std::unique_lock<std::mutex> lock(queueLock_);
    WaitUntilLocked(now, lock);
}

bool EventQueue::EnsureIoWaiterSupportListerningFileDescriptorLocked()
{
    HILOGD("enter");
    if (useDeamonIoWaiter_) {
        if (!DeamonIoWaiter::GetInstance().Init()) {
            HILOGE("Failed to initialize deamon waiter");
            return false;
        }
        DeamonIoWaiter::GetInstance().StartEpollIoWaiter();
        return true;
    }

    if (ioWaiter_->SupportListeningFileDescriptor()) {
        return true;
    }

    auto newIoWaiter = std::make_shared<EpollIoWaiter>();
    if (!newIoWaiter->Init()) {
        HILOGE("Failed to initialize epoll");
        return false;
    }

    // Set callback to handle events from file descriptors.
    newIoWaiter->SetFileDescriptorEventCallback(
        std::bind(&EventQueue::HandleFileDescriptorEvent, this, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3, std::placeholders::_4));

    ioWaiter_->NotifyAll();
    ioWaiter_ = newIoWaiter;
    return true;
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

    RemoveFileDescriptorListenerLocked(listeners_, ioWaiter_, listenerFilter, useDeamonIoWaiter_);
}

}  // namespace AppExecFwk
}  // namespace OHOS
