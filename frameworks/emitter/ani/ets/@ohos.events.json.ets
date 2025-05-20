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

type valueType = NullishType;

class RecordWriter {
  private buffer = new StringBuilder();
  private store = new Set<Object>();

  public write(obj: Object): String {
    this.writeObject(obj);
    return this.buffer.toString();
  }

  private writeObject(obj: NullishType): void {
    if (obj === null) {
      this.buffer.append('null');
    } else if (obj === undefined) {
      this.buffer.append('undefined');
    } else if (obj instanceof String) {
      this.buffer.append(JSON.stringify(obj as String));
    } else if (this.writeValueType(obj as int)) {
      // nothing to do
    } else if (obj instanceof Array) {
      this.writeArray(obj as Object as Array<valueType>);
    } else if (obj instanceof Record) {
      this.writeRecord(obj as Object as Record<string, valueType>);
    } else {
      const objType = Type.of(obj);
      if (objType instanceof ArrayType) {
        this.writeBuildArray(obj as int, Value.of(obj) as ArrayValue);
      } else {
        this.buffer.append('null');
      }
    }
  }

  private writeValueType(obj: Object): boolean {
    if (obj instanceof Boolean) {
      this.buffer.append(JSON.stringify(obj.unboxed()));
      return true;
    } else if (obj instanceof Byte) {
      this.buffer.append(JSON.stringify(obj.unboxed()));
      return true;
    } else if (obj instanceof Char) {
      this.buffer.append(JSON.stringify(obj.unboxed()));
      return true;
    } else if (obj instanceof Short) {
      this.buffer.append(JSON.stringify(obj.unboxed()));
      return true;
    } else if (obj instanceof Int) {
      this.buffer.append(JSON.stringify(obj.unboxed()));
      return true;
    } else if (obj instanceof Long) {
      this.buffer.append(JSON.stringify(obj.unboxed()));
      return true;
    } else if (obj instanceof Float) {
      this.buffer.append(JSON.stringify(obj.unboxed()));
      return true;
    } else if (obj instanceof Double) {
      this.buffer.append(JSON.stringify(obj.unboxed()));
      return true;
    } else if (obj instanceof BigInt) {
      this.buffer.append(JSON.stringify(obj));
      return true;
    } else {
      return false;
    }
  }

  private writeArray(arr: Array<valueType>): void {
    this.buffer.append('[');
    const length = arr.length.toInt();
    const length = arr.length.toInt();
    this.checkReferencesCycle(arr);
    this.store.add(arr);
    for (let idx = 0; idx < length; idx++) {
      if (arr[idx] == null) {
        this.buffer.append('null');
      } else {
        this.writeObject(arr[idx]);
      }
      if (idx < length - 1) {
        this.buffer.append(',');
      }
    }
    this.store.delete(arr);
    this.buffer.append(']');
  }

  private writeBuildArray(arr: Object, arrayValue: ArrayValue): void {
    this.buffer.append('[');
    const length = arrayValue.getLength().toInt();
    const length = arrayValue.getLength().toInt();
    this.checkReferencesCycle(arr);
    this.store.add(arr);
    for (let idx = 0; idx < length; idx++) {
      let member = arrayValue.getElement(idx).getData();
      if (member == null) {
        this.buffer.append('null');
      } else {
        this.writeObject(member);
      }
      if (idx < length - 1) {
        this.buffer.append(',');
      }
    }
    this.store.delete(arr);
    this.buffer.append(']');
  }

  private writeRecord(rec: Record<string, valueType>): void {
    this.buffer.append('{');
    this.checkReferencesCycle(rec);
    this.store.add(rec);
    let isFirst = true;
    for (let key of rec.keys()) {
      if (rec[key] !== undefined) {
        if (!isFirst) {
          this.buffer.append(',');
        } else {
          isFirst = false;
        }
        this.buffer.append(JSON.stringify(key as String));
        this.buffer.append(':');
        this.writeObject(rec[key]);
      }
    }
    this.store.delete(rec);
    this.buffer.append('}');
  }

  private checkReferencesCycle(obj: Object): void {
    if (this.store.has(obj)) {
      throw new TypeError('cyclic object value');
    }
  }
}

export class RecordSerializeTool {
  public static stringifyNoThrow(obj: Record<string, Object>): String {
    try {
      return RecordSerializeTool.stringify(obj as Object as Record<string, NullishType>);
    } catch (err) {
      return err.toString();
    }
  }

  public static parseNoThrow(text: string): Record<string, Object> {
    try {
      return RecordSerializeTool.parse(text) as Object as Record<string, Object>;
    } catch (err) {
      console.log('err: ' + err.toString());
      return new Record<string, Object>();
    }
  }

  public static stringify(obj: Record<string, valueType>): String {
    return new RecordWriter().write(obj);
  }

  public static parse(text: string): Record<string, valueType> {
    let jsonValue = JSONParser.parse(text);
    let res = RecordSerializeTool.jsonValue2Object(jsonValue);
    if (!(res instanceof Record)) {
      throw new TypeError('RecordSerializeTool parse only used for Record');
    }
    return res as Record<string, valueType>;
  }

  private static jsonValue2Object(value: JSONValue): string | number | boolean | null |
    Array<valueType> | Record<string, valueType> {
    if (value instanceof JSONString) {
      return value.value;
    } else if (value instanceof JSONNumber) {
      return new Double(value.value);
    } else if (value instanceof JSONTrue) {
      return new Boolean(true);
    } else if (value instanceof JSONFalse) {
      return new Boolean(false);
    } else if (value instanceof JSONNull) {
      return null;
    } else if (value instanceof JSONArray) {
      let obj = value as JSONArray;
      let values = obj.values;
      let result: Array<valueType> = new Array<valueType>();
      for (let i: int = 0; i < values.length; i++) {
        result.push(RecordSerializeTool.jsonValue2Object(values[i]));
      }
      return result;
    } else if (value instanceof JSONObject) {
      let obj = value as JSONObject;
      let keys: Array<JSONString> = obj.keys_;
      let values: Array<JSONValue> = obj.values;
      let result: Record<string, valueType> = new Record<string, valueType>();
      for (let i: int = 0; i < keys.length; i++) {
        result[keys[i].value] = RecordSerializeTool.jsonValue2Object(values[i]);
      }
      return result;
    } else {
      throw new TypeError('unknown JSONValue');
    }
  }
}
