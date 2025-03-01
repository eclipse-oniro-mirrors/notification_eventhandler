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

enum EventPriority {
  LOW = 0,
  NORMAL = 1,
  HIGH = 2,
  IMMEDIATE = 3,
}

class InnerEvent {
  eventId: number;
  priority?: EventPriority;
}

type Callback<T> = (data: T) => void;

class EventData {}
class GenericEventData<T> {}

export class EventsEmitter {  
  static { loadLibrary("emitter.z"); }
  
  public native OffStringSync(eventId: string, callback: Callback<EventData>): void;
  public native OffGenericEventSync<T>(eventId: string, callback: Callback<GenericEventData<T>>): void;
  public native OnOrOnceStringSync(eventId: string, once : boolean, callback: Callback<EventData>): void;
  public native OnOrOnceGenericEventSync<T>(eventId: string, once : boolean, callback: Callback<GenericEventData<T>>): void;
  public native OnOrOnceSync(eventId: number, once : boolean, callback: Callback<EventData>): void;
  
  on(event: InnerEvent, callback: Callback<EventData>): void {
    if (event.eventId) {
      this.OnOrOnceSync(event.eventId, false, callback);
    }
  }

  off<T>(eventId: string, callback: Callback<GenericEventData<T>> | Callback<EventData>): void {
    if (callback instanceof Callback<EventData>) {
      this.OffStringSync(eventId, callback as Callback<EventData>);
    } else {
      this.OffGenericEventSync(eventId, callback as Callback<GenericEventData<T>>);
    }
  }

  on<T>(eventId: string, callback: Callback<GenericEventData<T>> | Callback<EventData>): void {
    if (callback instanceof Callback<EventData>) {
      this.OnOrOnceStringSync(eventId, false, callback as Callback<EventData>);
    } else {
      this.OnOrOnceGenericEventSync(eventId, false, callback as Callback<GenericEventData<T>>);
    }
  }

}
