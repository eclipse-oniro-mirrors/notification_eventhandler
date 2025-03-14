/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

import { Callback } from '@ohos.base';

namespace emitter {
  loadLibrary("eventEmitter.z");
  export interface EventData {
    data?: Map<string, object>;
  }

  class EventDataInner implements EventData {
    public data?: Map<string, object> | undefined;
  }

  export interface GenericEventData<T> {
    data?: T;
  }

  class GenericEventDataInner<T> implements GenericEventData<T> {
    public data?: T | undefined;
  }

  export enum EventPriority {
    IMMEDIATE = 0,
    HIGH = 1,
    LOW = 2,
    IDLE = 3,
  }

  export interface InnerEvent {
    eventId: number;
    priority?: EventPriority;
  }

  class InnerEventInner implements InnerEvent {
    public eventId: number = 0;
    public priority?: EventPriority | undefined;
  }

  export interface Options {
    priority?: EventPriority;
  }

  class OptionsInner implements Options {
    public priority?: EventPriority | undefined;
  }

  export native function OffStringSync(eventId: string, callback: Callback<EventData>): void;
  export native function OffGenericEventSync<T>(eventId: string, callback: Callback<GenericEventData<T>>): void;
  export native function OnOrOnceStringSync(eventId: string, once : boolean, callback: Callback<EventData>): void;
  export native function OnOrOnceGenericEventSync<T>(eventId: string, once : boolean, callback: Callback<GenericEventData<T>>): void;
  export native function OnOrOnceSync(eventId: number, once : boolean, callback: Callback<EventData>): void;
  
  export function on(event: InnerEvent, callback: Callback<EventData>): void {
    if (event.eventId) {
      emitter.OnOrOnceSync(event.eventId, false, callback);
    }
  }

  export function off<T>(eventId: string, callback: Callback<EventData> | Callback<GenericEventData<T>>): void {
    if (callback instanceof Callback<EventData>) {
      emitter.OffStringSync(eventId, callback as Callback<EventData>);
    } else {
      emitter.OffGenericEventSync(eventId, callback as Callback<GenericEventData<T>>);
    }
  }

  export function on<T>(eventId: string, callback: Callback<EventData> | Callback<GenericEventData<T>>): void {
    if (callback instanceof Callback<EventData>) {
      emitter.OnOrOnceStringSync(eventId, false, callback as Callback<EventData>);
    } else {
      emitter.OnOrOnceGenericEventSync(eventId, false, callback as Callback<GenericEventData<T>>);
    }
  }
}
export default emitter;
