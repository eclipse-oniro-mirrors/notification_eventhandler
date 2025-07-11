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
    data?: Record<string, Object>;
  }

  class EventDataInner implements EventData {
    public data?: Record<string,Object> | undefined;
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

  export native function OffStringIdSync(eventId: string): void;
  export native function OffStringSync(eventId: string, callback: Callback<EventData>): void;
  export native function OffGenericEventSync<T>(eventId: string, callback: Callback<GenericEventData<T>>): void;
  export native function OffNumberSync(eventId: number): void;
  export native function OffNumberCallbackSync(eventId: number, callback: Callback<EventData>): void;
  export native function OnOrOnceStringSync(eventId: string, once: boolean, callback: Callback<EventData>, dataType: string): void;
  export native function OnOrOnceGenericEventSync<T>(eventId: string, once : boolean, callback: Callback<GenericEventData<T>>, dataType: string): void;
  export native function OnOrOnceSync(eventId: number, once: boolean, callback: Callback<EventData>, dataType: string): void;
  export native function getListenerCountSync(eventId: number): number;
  export native function getListenerCountStringSync(eventId: string): number;
  export native function EmitInnerEventSync(eventId: InnerEvent): void;
  export native function EmitInnerEventDataSync(eventId: InnerEvent, data: EventData): void;
  export native function EmitStringSync(eventId: string): void;
  export native function EmitStringDataSync(eventId: string, data: EventData): void;
  export native function EmitStringGenericSync<T>(eventId: string, data: GenericEventData<T>): void;
  export native function EmitStringOptionsDataSync(eventId: string, options: Options, data: EventData): void;
  export native function EmitStringOptionsGenericSync<T>(eventId: string, options: Options, data: GenericEventData<T>): void;
  export native function EmitStringOptionsSync(eventId:string, options: Options): void;

  export function on(event: InnerEvent, callback: Callback<EventData>): void {
    if (event == null || callback == null) {
      return;
    }
    if (event.eventId) {
      OnOrOnceSync(event.eventId, false, callback, "eventData");
    }
  }

  export function on(eventId: string, callback: Callback<EventData>): void {
    if (eventId === '' || callback == null) {
      return;
    }
    emitter.OnOrOnceStringSync(eventId, false, callback, "eventData");
  }

  export function on<T>(eventId: string, callback: Callback<EventData> | Callback<GenericEventData<T>>): void {
    if (eventId === '' || callback == null) {
      return;
    }
    if (callback instanceof Callback<EventData>) {
      OnOrOnceStringSync(eventId, false, callback as Callback<EventData>, "eventData");
    } else {
      OnOrOnceGenericEventSync(eventId, false, callback as Callback<GenericEventData<T>>, "genericEventData");
    }
  }

  export function once(event: InnerEvent, callback: Callback<EventData>): void {
    if (event == null || callback == null) {
      return;
    }
    if (event.eventId) {
      OnOrOnceSync(event.eventId, true, callback, "eventData");
    }
  }

  export function once(eventId: string, callback: Callback<EventData>): void {
    if (eventId === '' || callback == null) {
      return;
    }
    OnOrOnceStringSync(eventId, true, callback, "eventData");
  }

  export function off(eventId: number): void {
    OffNumberSync(eventId);
  }

  export function off(eventId: string): void {
    if (eventId === '') {
      return;
    }
    OffStringIdSync(eventId);
  }

  export function off(eventId: number, callback: Callback<EventData>): void {
    OffNumberCallbackSync(eventId, callback);
  }

  export function off(eventId: string, callback: Callback<EventData>): void {
    if (eventId === '') {
      return;
    }
    emitter.OffStringSync(eventId, callback);
  }

  export function off<T>(eventId: string, callback: Callback<EventData> | Callback<GenericEventData<T>>): void {
    if (eventId === '') {
      return;
    }
    if (callback instanceof Callback<EventData>) {
      OffStringSync(eventId, callback as Callback<EventData>);
    } else {
      OffGenericEventSync(eventId, callback as Callback<GenericEventData<T>>);
    }
  }

  export function emit(event: InnerEvent, data?: EventData): void {
    if (data != undefined) {
      EmitInnerEventDataSync(event, data);
    } else {
      EmitInnerEventSync(event);
    }
  }

  export function emit(eventId: string, data?: EventData): void {
    if (eventId === '') {
      return;
    }
    if (data != undefined) {
      EmitStringDataSync(eventId, data as EventData);
    } else {
      EmitStringSync(eventId);
    }
  }

  export function emit<T>(eventId: string, data?: GenericEventData<T>): void {
    if (eventId === '') {
      return;
    }
    if (data != undefined) {
      EmitStringGenericSync(eventId, data as GenericEventData<T>);
    } else {
      EmitStringSync(eventId);
    }
  }

  export function emit(eventId: string, options: Options, data?: EventData): void {
    if (eventId === '' || options == null) {
      return;
    }
    if (data != undefined) {
      EmitStringOptionsDataSync(eventId, options, data as EventData);
    } else {
      EmitStringOptionsSync(eventId, options);
    }
  }

  export function emit<T>(eventId: string, options: Options, data?: GenericEventData<T>): void {
    if (eventId === '' || options == null) {
      return;
    }
    if (data != undefined) {
      EmitStringOptionsGenericSync(eventId, options, data as GenericEventData<T>);
    } else {
      EmitStringOptionsSync(eventId, options);
    }
  }

  export function getListenerCount(eventId: number | string): number {
    let count : number = 0;
    if (typeof eventId === "number") {
      count = getListenerCountSync(eventId as number);
    } else {
      if (eventId === '') {
        return count;
      }
      count = getListenerCountStringSync(eventId as string);
    }
    return count;
  }
}
export default emitter;