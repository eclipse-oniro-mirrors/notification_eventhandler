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
    data?: Record<string, Object> | ESObject;
  }

  class EventDataInner implements EventData {
    public data?: Record<string, Object> | ESObject;
  }

  export interface GenericEventData<T> {
    data?: T | undefined | ESObject;
  }

  class GenericEventDataInner<T> implements GenericEventData<T> {
    public data?: T | undefined | ESObject;
  }

  export enum EventPriority {
    IMMEDIATE = 0,
    HIGH = 1,
    LOW = 2,
    IDLE = 3,
  }

  export interface InnerEvent {
    eventId: long;
    priority?: EventPriority;
  }

  class InnerEventInner implements InnerEvent {
    public eventId: long = 0;
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
  export native function OffNumberSync(eventId: long): void;
  export native function OffNumberCallbackSync(eventId: long, callback: Callback<EventData>): void;
  export native function OnOrOnceStringSync(eventId: string, once: boolean, callback: Callback<EventData>, dataType: string): void;
  export native function OnOrOnceGenericEventSync<T>(eventId: string, once : boolean, callback: Callback<GenericEventData<T>>, dataType: string): void;
  export native function OnOrOnceSync(eventId: long, once: boolean, callback: Callback<EventData>, dataType: string): void;
  export native function getListenerCountSync(eventId: long): long;
  export native function getListenerCountStringSync(eventId: string): long;
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

  export function onEventData(eventId: string, callback: Callback<EventData>): void {
    if (eventId === '' || callback == null) {
      return;
    }
    OnOrOnceStringSync(eventId, false, callback, "eventData");
  }

  export function onGenericEventData<T>(eventId: string, callback: Callback<GenericEventData<T>>): void {
    if (eventId === '' || callback == null) {
      return;
    }
    OnOrOnceGenericEventSync(eventId, false, callback, "genericEventData");
  }

  export function once(event: InnerEvent, callback: Callback<EventData>): void {
    if (event == null || callback == null) {
      return;
    }
    if (event.eventId) {
      OnOrOnceSync(event.eventId, true, callback, "eventData");
    }
  }

  export function onceEventData(eventId: string, callback: Callback<EventData>): void {
    if (eventId === '' || callback == null) {
      return;
    }
    OnOrOnceStringSync(eventId, true, callback, "eventData");
  }

  export function onceGenericEventData<T>(eventId: string, callback: Callback<GenericEventData<T>>): void {
    if (eventId === '' || callback == null) {
      return;
    }
    OnOrOnceGenericEventSync(eventId, true, callback, "genericEventData");
  }

  export function off(eventId: long): void {
    OffNumberSync(eventId);
  }

  export function off(eventId: string): void {
    if (eventId === '') {
      return;
    }
    OffStringIdSync(eventId);
  }

  export function off(eventId: long, callback: Callback<EventData>): void {
    OffNumberCallbackSync(eventId, callback);
  }

  export function offEventData(eventId: string, callback: Callback<EventData>): void {
    if (eventId === '') {
      return;
    }
    OffStringSync(eventId, callback);
  }

  export function offGenericEventData<T>(eventId: string, callback: Callback<GenericEventData<T>>): void {
    if (eventId === '') {
      return;
    }
    OffGenericEventSync(eventId, callback);
  }

  export function emit(event: InnerEvent, data?: EventData): void {
    if (data != undefined) {
      EmitInnerEventDataSync(event, data as EventData);
    } else {
      EmitInnerEventSync(event);
    }
  }

  export function emit(eventId: string): void {
    if (eventId === '') {
      return;
    }

    EmitStringSync(eventId);
  }

  export function emit(eventId: string, data: EventData): void {
    if (eventId === '') {
      return;
    }

    EmitStringDataSync(eventId, data as EventData);
  }

  export function emit<T>(eventId: string, data: GenericEventData<T>): void {
    if (eventId === '') {
      return;
    }

    EmitStringGenericSync(eventId, data as GenericEventData<T>);
  }

  export function emit(eventId: string, options: Options): void {
    if (eventId === '') {
      return;
    }

    EmitStringOptionsSync(eventId, options);
  }

  export function emit(eventId: string, options: Options, data: EventData): void {
    if (eventId === '' || options == null) {
      return;
    }

    EmitStringOptionsDataSync(eventId, options, data as EventData);
  }

  export function emit<T>(eventId: string, options: Options, data: GenericEventData<T>): void {
    if (eventId === '' || options == null) {
      return;
    }

    EmitStringOptionsGenericSync(eventId, options, data as GenericEventData<T>);
  }

  export function getListenerCount(eventId: long | string): long {
    if (typeof eventId === 'string') {
      let eventTemp : string = eventId as string;
      return getListenerCountStringSync(eventTemp);
    } else {
      let eventTemp : long = eventId as long;
      return getListenerCountSync(eventTemp);
    }
  }
}
export default emitter;