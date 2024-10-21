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

void EventQueue::CheckFileDescriptorEvent()
{
    InnerEvent::TimePoint now = InnerEvent::Clock::now();
    std::unique_lock<std::mutex> lock(queueLock_);
    WaitUntilLocked(now, lock);
}
}  // namespace AppExecFwk
}  // namespace OHOS
