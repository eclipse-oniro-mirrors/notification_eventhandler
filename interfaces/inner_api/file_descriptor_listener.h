/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef BASE_EVENTHANDLER_INTERFACES_INNER_API_FILE_DESCRIPTOR_LISTENER_H
#define BASE_EVENTHANDLER_INTERFACES_INNER_API_FILE_DESCRIPTOR_LISTENER_H

#include <cstdint>
#include <memory>

#include "nocopyable.h"

namespace OHOS {
namespace AppExecFwk {
// Listen input or output events
const uint32_t FILE_DESCRIPTOR_INPUT_EVENT = 1;
const uint32_t FILE_DESCRIPTOR_OUTPUT_EVENT = 2;
// Listen shutdown and exception events automatically, so not necessary to set.
const uint32_t FILE_DESCRIPTOR_SHUTDOWN_EVENT = 4;
const uint32_t FILE_DESCRIPTOR_EXCEPTION_EVENT = 8;
const uint32_t FILE_DESCRIPTOR_EVENTS_MASK = (FILE_DESCRIPTOR_INPUT_EVENT | FILE_DESCRIPTOR_OUTPUT_EVENT);

class EventHandler;

class FileDescriptorListener {
public:
    enum ListenerType {
        LTYPE_UNKNOW,
        LTYPE_VSYNC,
        LTYPE_UV,
        LTYPE_MMI,
        LTYPE_WEBVIEW,
    };
    DISALLOW_COPY_AND_MOVE(FileDescriptorListener);

    /**
     * Called while file descriptor is readable.
     *
     * @param fileDescriptor File descriptor which is readable.
     */
    virtual void OnReadable(int32_t fileDescriptor);

    /**
     * Called while file descriptor is writable.
     *
     * @param fileDescriptor File descriptor which is writable.
     */
    virtual void OnWritable(int32_t fileDescriptor);

    /**
     * Called while shutting down this file descriptor.
     *
     * @param fileDescriptor File descriptor which is shutting down.
     */
    virtual void OnShutdown(int32_t fileDescriptor);

    /**
     * Called while error happened on this file descriptor.
     *
     * @param fileDescriptor Error happened on this file descriptor.
     */
    virtual void OnException(int32_t fileDescriptor);

    /**
     * Get owner of the event.
     *
     * @return Returns owner of the event after it has been sent.
     */
    inline std::shared_ptr<EventHandler> GetOwner() const
    {
        return owner_.lock();
    }

    /**
     * Set owner of the event.
     *
     * @param owner Owner for the event.
     */
    inline void SetOwner(const std::shared_ptr<EventHandler> &owner)
    {
        owner_ = owner;
    }

    /**
     * Get isDeamonWaiter of the event.
     *
     * @return Returns deamon waiter flag.
     */
    inline bool GetIsDeamonWaiter()
    {
        return isDeamonWaiter_;
    }

    /**
     * Set Deamon Waiter of the event.
     */
    inline void SetDeamonWaiter()
    {
        isDeamonWaiter_ = true;
    }

    /**
     * Set the delay time for handling the event.
     */
    inline void SetDelayTime(uint32_t delayms)
    {
        delayTime_ = delayms;
    }

    /**
     * Get the delay time for handling the event.
     */
    inline uint32_t GetDelayTime()
    {
        return delayTime_;
    }

    /**
     * Set the event type for the fd listener.
     */
    inline void SetType(enum ListenerType type)
    {
        type_ = type;
    }

    /**
     * Get the event type for the fd listener.
     */
    inline enum ListenerType GetType()
    {
        return type_;
    }

    /**
     * Check the event type for the fd listener is vsync or not.
     */
    inline bool IsVsyncListener()
    {
        return type_ == LTYPE_VSYNC;
    }
protected:
    FileDescriptorListener() = default;
    virtual ~FileDescriptorListener() = default;

private:
    std::weak_ptr<EventHandler> owner_;
    bool isDeamonWaiter_{false};
    enum ListenerType type_ = LTYPE_UNKNOW;
    uint32_t delayTime_ = 0;
};
}  // namespace AppExecFwk
}  // namespace OHOS

#endif  // #ifndef BASE_EVENTHANDLER_INTERFACES_INNER_API_FILE_DESCRIPTOR_LISTENER_H
