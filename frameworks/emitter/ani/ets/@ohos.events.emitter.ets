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

import { Callback, RecordData } from '@ohos.base';

namespace emitter {
  loadLibraryWithPermissionCheck("eventEmitter.z", "@ohos.events.emitter");

  export interface EventData {
    data?: Record<string, RecordData> | ESValue;
  }

  class EventDataInner implements EventData {
    public data?: Record<string, RecordData> | ESValue;
  }

  export interface GenericEventData<T> {
    data?: T | undefined | ESValue;
  }

  class GenericEventDataInner<T> implements GenericEventData<T> {
    public data?: T | undefined | ESValue;
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

  export native function EmitterConstructor(emitter: Emitter): void
  export native function EmitterOnDataSync(emitterId: long, eventId: string, callback: Callback<EventData>, dataType: string): void;
  export native function EmitterOnceDataSync(emitterId: long, eventId: string, callback: Callback<EventData>, dataType: string): void;
  export native function EmitterOnGenericSync<T>(emitterId: long, eventId: string, callback: Callback<GenericEventData<T>>, dataType: string): void;
  export native function EmitterOnceGenericSync<T>(emitterId: long, eventId: string, callback: Callback<GenericEventData<T>>, dataType: string): void;
  export native function EmitterOffSync(emitterId: long, eventId: string): void;
  export native function EmitterOffDataSync(emitterId: long, eventId: string, callback: Callback<EventData>): void;
  export native function EmitterOffGenericSync<T>(emitterId: long, eventId: string, callback: Callback<GenericEventData<T>>): void;
  export native function EmitterEmitSync(emitterId: long, eventId: string): void;
  export native function EmitterEmitDataSync(emitterId: long, eventId: string, data: EventData): void;
  export native function EmitterEmitGenericSync<T>(emitterId: long, eventId: string, data: GenericEventData<T>): void;
  export native function EmitterEmitOptionsSync(emitterId: long, eventId: string, options: Options): void;
  export native function EmitterEmitOptionsDataSync(emitterId: long, eventId: string, options: Options, data: EventData): void;
  export native function EmitterEmitOptionsGenericSync<T>(emitterId: long, eventId: string, options: Options, data: GenericEventData<T>): void;
  export native function EmitterGetListenerCountSync(emitterId: long, eventId: string): long;
  export native function EmitterTransferToDynamic(input: Object): Any;
  export native function EmitterTransferToStatic(input: Any): Object;

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

  export class Cleaner {
    private emitterId: long = 0

    constructor(emitterId:long) {
        this.emitterId = emitterId
    }
    native clean(): void
  }

  export function callback(cleaner: Cleaner): void {
    cleaner.clean()
  }
  let destroyRegister = new FinalizationRegistry<Cleaner>(callback)
  let unregisterToken = new object()

  export class Emitter {
    constructor() {
      EmitterConstructor(this);
      this.registerCleaner(this.emitterId);
    }

    static transferDynamic(input: Object): Any {
      return EmitterTransferToDynamic(input);
    }

    static transferStatic(input: Any): Object {
      return EmitterTransferToStatic(input);
    }

    public onEventData(eventId: string, callback: Callback<EventData>): void {
      if (eventId === '') {
        return;
      }
      EmitterOnDataSync(this.emitterId, eventId, callback, "eventData")
    }

    public onceEventData(eventId: string, callback: Callback<EventData>): void {
      if (eventId === '') {
        return;
      }
      EmitterOnceDataSync(this.emitterId, eventId, callback, "eventData")
    }

    public onGenericEventData<T>(eventId: string, callback: Callback<GenericEventData<T>>): void {
      if (eventId === '') {
        return;
      }
      EmitterOnGenericSync(this.emitterId, eventId, callback, "genericEventData");
    }

    public onceGenericEventData<T>(eventId: string, callback: Callback<GenericEventData<T>>): void {
      if (eventId === '') {
        return;
      }
      EmitterOnceGenericSync(this.emitterId, eventId, callback, "genericEventData");
    }

    public off(eventId: string): void {
      if (eventId === '') {
        return;
      }
      EmitterOffSync(this.emitterId, eventId);
    }

    public offEventData(eventId: string, callback: Callback<EventData>): void {
      if (eventId === '') {
        return;
      }
      EmitterOffDataSync(this.emitterId, eventId, callback);
    }

    public offGenericEventData<T>(eventId: string, callback: Callback<GenericEventData<T>>): void {
      if (eventId === '') {
        return;
      }
      EmitterOffGenericSync(this.emitterId, eventId, callback);
    }

    public emit(eventId: string, options: Options, data?: EventData): void {
      if (eventId === '') {
        return;
      }
      if (data != undefined) {
        EmitterEmitOptionsDataSync(this.emitterId, eventId, options, data as EventData);
      } else {
        EmitterEmitOptionsSync(this.emitterId, eventId, options);
      }
    }

    public emit<T>(eventId: string, options: Options, data?: GenericEventData<T>): void {
      if (eventId === '') {
        return;
      }
      if (data != undefined) {
        EmitterEmitOptionsGenericSync(this.emitterId, eventId, options, data as GenericEventData<T>);
      } else {
        EmitterEmitOptionsSync(this.emitterId, eventId, options);
      }
    }

    public emit(eventId: string, data?: EventData): void {
      if (eventId === '') {
        return;
      }
      if (data != undefined) {
        EmitterEmitDataSync(this.emitterId, eventId, data as EventData);
      } else {
        EmitterEmitSync(this.emitterId, eventId);
      }
    }

    public emit<T>(eventId: string, data?: GenericEventData<T>): void {
      if (eventId === '') {
        return;
      }
      if (data != undefined) {
        EmitterEmitGenericSync(this.emitterId, eventId, data as GenericEventData<T>);
      } else {
        EmitterEmitSync(this.emitterId, eventId);
      }
    }

    public getListenerCount(eventId: string): long {
      let count : long = 0;
      if (eventId === '') {
        return count;
      }
      return EmitterGetListenerCountSync(this.emitterId, eventId);
    }

    private emitterId: long;

    private cleaner: Cleaner | null = null;

    registerCleaner(emitterId: long): void {
        this.cleaner = new Cleaner(this.emitterId)
        destroyRegister.register(this, this.cleaner!, unregisterToken);
    }
  }
}
export default emitter;
