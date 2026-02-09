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
#include "frame_report_sched.h"
#include "none_io_waiter.h"
#include "parameters.h"


namespace OHOS {
namespace AppExecFwk {
namespace {
static const bool MONITOR_FLAG =
    system::GetBoolParameter("const.sys.param_file_description_monitor", false);

DEFINE_EH_HILOG_LABEL("EventQueue");

// Help to remove file descriptor listeners.
template<typename T>
void RemoveFileDescriptorListenerLocked(std::map<int32_t, std::shared_ptr<FileDescriptorListener>> &listeners,
    const std::shared_ptr<IoWaiter>& ioWaiter, const T &filter, bool useDeamonIoWaiter_)
{
    for (auto it = listeners.begin(); it != listeners.end();) {
        if (filter(it->second)) {
            if (useDeamonIoWaiter_ || (it->second->GetIsDeamonWaiter() && MONITOR_FLAG)) {
                DeamonIoWaiter::GetInstance().RemoveFileDescriptor(it->first);
            }
            if (ioWaiter) {
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
    EH_LOGD_LIMIT("EventQueue is unavailable hence");
}

InnerEvent::Pointer EventQueue::GetEvent()
{
    return InnerEvent::Pointer(nullptr, nullptr);
}

InnerEvent::Pointer EventQueue::GetExpiredEvent(InnerEvent::TimePoint &nextExpiredTime)
{
    return InnerEvent::Pointer(nullptr, nullptr);
}

void EventQueue::WaitUntilLocked(const InnerEvent::TimePoint &when, std::unique_lock<std::mutex> &lock, bool vsyncOnly)
{
    // Get a temp reference of IO waiter, otherwise it maybe released while waiting.
    auto ioWaiterHolder = ioWaiter_;
    if (!ioWaiterHolder->WaitFor(lock, TimePointToTimeOut(when), vsyncOnly)) {
        HILOGE("Failed to call wait, reset IO waiter");
        ioWaiter_ = std::make_shared<NoneIoWaiter>();
        listeners_.clear();
    }
}

void EventQueue::CheckFileDescriptorEvent()
{
    InnerEvent::TimePoint now = InnerEvent::Clock::now();
    std::unique_lock<std::mutex> lock(queueLock_);
    WaitUntilLocked(now, lock);
}

bool EventQueue::AddFileDescriptorByFd(int32_t fileDescriptor, uint32_t events, const std::string &taskName,
    const std::shared_ptr<FileDescriptorListener>& listener, EventQueue::Priority priority)
{
    bool isVsyncListener = listener && listener->IsVsyncListener();
    bool isDaemonListener = listener && listener->GetIsDeamonWaiter() && MONITOR_FLAG;

    if (isVsyncListener) {
        priority = Priority::VIP;
    }
    if (useDeamonIoWaiter_ || (isDaemonListener && !isVsyncListener)) {
        return DeamonIoWaiter::GetInstance().AddFileDescriptor(fileDescriptor, events, taskName,
            listener, priority);
    }
    if (ioWaiter_) {
        auto handler = listener->GetOwner();
        if (taskName == "vSyncTask" && handler && handler->GetEventRunner() == EventRunner::GetMainEventRunner()) {
            DeamonIoWaiter::GetInstance().AddFileDescriptor(fileDescriptor, events, taskName, listener, priority);
        }
        return ioWaiter_->AddFileDescriptor(fileDescriptor, events, taskName, listener, priority);
    }
    return false;
}

bool EventQueue::EnsureIoWaiterLocked(const std::shared_ptr<FileDescriptorListener>& listener)
{
    HILOGD("enter");
    if (useDeamonIoWaiter_ || (listener && listener->GetIsDeamonWaiter() && MONITOR_FLAG)) {
        if (!DeamonIoWaiter::GetInstance().Init()) {
            HILOGE("Failed to initialize deamon waiter");
            return false;
        }
        DeamonIoWaiter::GetInstance().StartEpollIoWaiter();
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

ErrCode EventQueue::AddFileDescriptorListenerBase(int32_t fileDescriptor, uint32_t events,
    const std::shared_ptr<FileDescriptorListener> &listener, const std::string &taskName, Priority priority)
{
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
    if (!EnsureIoWaiterLocked(listener)) {
        return EVENT_HANDLER_ERR_FD_NOT_SUPPORT;
    }

    if (!AddFileDescriptorByFd(fileDescriptor, events, taskName, listener, priority)) {
        HILOGE("Failed to add file descriptor into IO waiter");
        return EVENT_HANDLER_ERR_FD_FAILED;
    }

    listeners_.emplace(fileDescriptor, listener);
    return ERR_OK;
}

static void PostTaskForVsync(const InnerEvent::Callback &cb, const std::string &name,
    const std::shared_ptr<EventHandler> &handler, EventQueue::Priority priority)
{
    auto runner = handler->GetEventRunner();
    if (!runner) {
        return;
    }
    auto event = InnerEvent::Get(cb, name);
    if (!event) {
        return;
    }
    event->MarkVsyncTask();
    handler->SendEvent(event, UINT32_MAX, priority);
}

std::shared_ptr<FileDescriptorListener> EventQueue::GetListenerByfd(int32_t fileDescriptor)
{
    std::lock_guard<std::mutex> lock(queueLock_);
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return nullptr;
    }
    auto it = listeners_.find(fileDescriptor);
    if (it == listeners_.end()) {
        HILOGW("Can not found listener, maybe it is removed");
        return nullptr;
    }
    // Hold instance of listener.
    return it->second;
}

void EventQueue::HandleFileDescriptorEvent(int32_t fileDescriptor, uint32_t events,
    const std::string &taskName, Priority priority) __attribute__((no_sanitize("cfi")))
{
    std::shared_ptr<FileDescriptorListener> listener = GetListenerByfd(fileDescriptor);
    if (!listener) {
        return;
    }

    auto handler = listener->GetOwner();
    if (!handler) {
        HILOGW("Owner of listener is released");
        return;
    }

    bool isVsyncTask = handler->GetEventRunner() && listener->IsVsyncListener();
    std::weak_ptr<FileDescriptorListener> wp = listener;
    auto f = [fileDescriptor, events, wp, isVsyncTask]() {
        auto queue = EventRunner::Current()->GetEventQueue();
        if (isVsyncTask) {
            queue->HandleVsyncTaskNotify();
            queue->SetBarrierMode(false);
        }
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

        if (isVsyncTask) {
            queue->HandleVsyncTaskCompletely();
        }
    };

    HILOGD("Post fd %{public}d, task %{public}s, priority %{public}d.", fileDescriptor, taskName.c_str(), priority);
    // Post a high priority task to handle file descriptor events.
    if (isVsyncTask) {
        PostTaskForVsync(f, taskName, handler, priority);
    } else {
        handler->PostTask(f, taskName, 0, priority);
    }
}

void EventQueue::RemoveListenerByOwner(const std::shared_ptr<EventHandler> &owner)
{
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return;
    }
    auto listenerFilter = [&owner](const std::shared_ptr<FileDescriptorListener> &listener) {
        if (!listener) {
            return false;
        }
        return listener->GetOwner() == owner;
    };
    RemoveFileDescriptorListenerLocked(listeners_, ioWaiter_, listenerFilter, useDeamonIoWaiter_);
}

void EventQueue::RemoveListenerByFd(int32_t fileDescriptor)
{
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return;
    }
    auto it = listeners_.find(fileDescriptor);
    std::shared_ptr<FileDescriptorListener> listener;
    if (it != listeners_.end()) {
        listener = it->second;
    }
    if (listeners_.erase(fileDescriptor) > 0) {
        std::shared_ptr<FileDescriptorInfo> fdInfo = DeamonIoWaiter::GetInstance().GetFileDescriptorMap(fileDescriptor);
        if (useDeamonIoWaiter_ || (listener && listener->GetIsDeamonWaiter() && MONITOR_FLAG) ||
            (fdInfo && fdInfo->taskName_ == "vSyncTask" && listener && listener->GetOwner() &&
            listener->GetOwner()->GetEventRunner() == EventRunner::GetMainEventRunner())) {
            DeamonIoWaiter::GetInstance().RemoveFileDescriptor(fileDescriptor);
        }
        if (ioWaiter_) {
            ioWaiter_->RemoveFileDescriptor(fileDescriptor);
        }
    }
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

void EventQueue::SetVsyncLazyMode(bool isLazy)
{
    if (vsyncPolicy_ == VsyncPolicy::DISABLE_VSYNC_FIRST) {
        return;
    }
    isLazyMode_.store(isLazy);
    HILOGD("%{public}s(%{public}d)", __func__, isLazy);
}

void EventQueue::SetVsyncFirstForceEnableTime(bool enable, uint64_t timeout)
{
    if (enable) {
        vsyncPolicy_ = VsyncPolicy::VSYNC_FIRST_WITHOUT_DEFAULT_BARRIER;
        vsyncFirstForceEnableEndTime_ = NOW_NS + timeout;
    } else {
        vsyncPolicy_ = vsyncOriginPolicy_;
        vsyncFirstForceEnableEndTime_ = 0;
    }
}

void EventQueue::SetVsyncPolicy(VsyncPolicy vsyncPolicy)
{
    HILOGD("%{public}s(%{public}d)", __func__, vsyncPolicy);
    vsyncPolicy_ = vsyncPolicy;
    vsyncOriginPolicy_ = vsyncPolicy;
    if (vsyncPolicy_ == VsyncPolicy::DISABLE_VSYNC_FIRST) {
        isLazyMode_.store(true);
    }
}

void EventQueue::TryEpollFd(const InnerEvent::TimePoint &when, std::unique_lock<std::mutex> &lock)
{
    bool need = needEpoll_;

    WaitUntilLocked(when, lock, !need);
    if (sumOfPendingVsync_) {
        return;
    }

    if (need) {
        needEpoll_ = false;
    } else {
        if (vsyncPeriod_ < MAX_CHECK_VSYNC_PERIOD_NS) {
            vsyncCheckTime_ += vsyncPeriod_;
            vsyncPeriod_ *= 2;
        } else {
            vsyncCheckTime_ = INT64_MAX;
        }
    }
}

void EventQueue::PrepareBase()
{
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return;
    }
    finished_ = false;
}

void EventQueue::FinishBase()
{
    if (!usable_.load()) {
        HILOGW("EventQueue is unavailable.");
        return;
    }
    finished_ = true;
    ioWaiter_->NotifyAll();
}

void EventQueue::NotifyObserverVipDone(const InnerEvent::Pointer &event)
{
    HILOGD("eventQueue NotifyObserverVipDone enter.");
}
}  // namespace AppExecFwk
}  // namespace OHOS