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

#include "sts_events_json_common.h"
#include <cstring>
#include "event_logger.h"
#include "securec.h"

namespace OHOS {
namespace AppExecFwk {
namespace {
DEFINE_EH_HILOG_LABEL("EventsEmitter");
}
constexpr const char* CLASSNAME_DOUBLE = "Lstd/core/Double;";
constexpr const char* CLASSNAME_BOOLEAN = "Lstd/core/Boolean;";

bool GetIntByName(ani_env *env, ani_object param, const char *name, int &value)
{
    ani_int res;
    ani_status status;

    status = env->Object_GetFieldByName_Int(param, name, &res);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    value = static_cast<int>(res);
    return true;
}

bool GetDoubleOrUndefined(ani_env *env, ani_object param, const char *name, ani_double &value)
{
    ani_ref obj = nullptr;
    ani_boolean isUndefined = true;
    ani_status status = ANI_ERROR;

    if ((status = env->Object_GetFieldByName_Ref(param, name, &obj)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    if ((status = env->Reference_IsUndefined(obj, &isUndefined)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    if (isUndefined) {
        HILOGI("%{public}s : undefined", name);
        return false;
    }
    if ((status = env->Object_CallMethodByName_Double(
        reinterpret_cast<ani_object>(obj), "doubleValue", nullptr, &value)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    return true;
}

bool GetBoolOrUndefined(ani_env *env, ani_object param, const char *name)
{
    ani_ref obj = nullptr;
    ani_boolean isUndefined = true;
    ani_status status = ANI_ERROR;
    ani_boolean res = 0.0;

    if ((status = env->Object_GetFieldByName_Ref(param, name, &obj)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return res;
    }
    if ((status = env->Reference_IsUndefined(obj, &isUndefined)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return res;
    }
    if (isUndefined) {
        HILOGI("%{public}s : undefined", name);
        return res;
    }
    if ((status = env->Object_CallMethodByName_Boolean(
        reinterpret_cast<ani_object>(obj), "booleanValue", nullptr, &res)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return res;
    }
    return res;
}

bool GetStringOrUndefined(ani_env *env, ani_object param, const char *name, std::string &res)
{
    ani_ref obj = nullptr;
    ani_boolean isUndefined = true;
    ani_status status = ANI_ERROR;

    if ((status = env->Object_GetFieldByName_Ref(param, name, &obj)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    if ((status = env->Reference_IsUndefined(obj, &isUndefined)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    if (isUndefined) {
        HILOGI("%{public}s : undefined", name);
        return false;
    }
    if (!GetStdString(env, reinterpret_cast<ani_string>(obj), res)) {
        HILOGI("GetStdString failed");
        return false;
    }
    return true;
}

bool GetFixedStringArrayOrUndefined(ani_env *env, ani_object param, const char *name, std::vector<std::string> &res)
{
    ani_ref obj = nullptr;
    ani_boolean isUndefined = true;
    ani_status status;
    ani_size size = 0;
    ani_size i;
    ani_ref ref;
    std::string str;

    if ((status = env->Object_GetFieldByName_Ref(param, name, &obj)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    if ((status = env->Reference_IsUndefined(obj, &isUndefined)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    if (isUndefined) {
        HILOGI("%{public}s : undefined", name);
        return false;
    }

    if ((status = env->Array_GetLength(reinterpret_cast<ani_array>(obj), &size)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    for (i = 0; i < size; i++) {
        if ((status = env->Array_Get_Ref(reinterpret_cast<ani_array_ref>(obj), i, &ref)) != ANI_OK) {
            HILOGI("status : %{public}d, index: %{public}zu", status, i);
            return false;
        }

        str = "";
        if (!GetStdString(env, reinterpret_cast<ani_string>(ref), str)) {
            HILOGI("GetStdString failed, index: %{public}zu", i);
            return false;
        }

        res.push_back(str);
    }

    return true;
}

bool SetFieldFixedArrayString(ani_env *env, ani_class cls, ani_object object, const std::string &fieldName,
    const std::vector<std::string> &values)
{
    ani_field field = nullptr;
    ani_array_ref array = nullptr;
    ani_class stringCls = nullptr;
    ani_string string = nullptr;
    ani_ref undefinedRef = nullptr;
    ani_status status = env->Class_FindField(cls, fieldName.c_str(), &field);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    status = env->FindClass("Lstd/core/String;", &stringCls);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    status = env->GetUndefined(&undefinedRef);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    status = env->Array_New_Ref(stringCls, values.size(), undefinedRef, &array);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    for (size_t i = 0; i < values.size(); ++i) {
        string = nullptr;
        status = env->String_NewUTF8(values[i].c_str(), values[i].size(), &string);
        if (status != ANI_OK) {
            HILOGI("status : %{public}d", status);
            return false;
        }
        status = env->Array_Set_Ref(array, i, string);
        if (status != ANI_OK) {
            HILOGI("status : %{public}d", status);
            return false;
        }
    }
    status = env->Object_SetField_Ref(object, field, array);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    return true;
}

bool GetStringArrayOrUndefined(ani_env *env, ani_object param, const char *name, std::vector<std::string> &res)
{
    ani_ref arrayObj = nullptr;
    ani_boolean isUndefined = true;
    ani_status status;
    ani_double length;
    std::string str;

    if ((status = env->Object_GetFieldByName_Ref(param, name, &arrayObj)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    if ((status = env->Reference_IsUndefined(arrayObj, &isUndefined)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    if (isUndefined) {
        HILOGI("%{public}s : undefined", name);
        return false;
    }

    status = env->Object_GetPropertyByName_Double(reinterpret_cast<ani_object>(arrayObj), "length", &length);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    for (int i = 0; i < static_cast<int>(length); i++) {
        ani_ref stringEntryRef;
        status = env->Object_CallMethodByName_Ref(reinterpret_cast<ani_object>(arrayObj),
            "$_get", "I:Lstd/core/Object;", &stringEntryRef, (ani_int)i);
        if (status != ANI_OK) {
            HILOGI("status : %{public}d, index: %{public}d", status, i);
            return false;
        }

        str = "";
        if (!GetStdString(env, reinterpret_cast<ani_string>(stringEntryRef), str)) {
            HILOGI("GetStdString failed, index: %{public}d", i);
            return false;
        }

        res.push_back(str);
    }

    return true;
}


bool SetFieldArrayString(ani_env *env, ani_class cls, ani_object object, const std::string &fieldName,
    const std::vector<std::string> &values)
{
    ani_field field = nullptr;
    ani_class arrayCls = nullptr;
    ani_method arrayCtor;
    ani_object arrayObj;
    ani_string string = nullptr;

    ani_status status = env->Class_FindField(cls, fieldName.c_str(), &field);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    status = env->FindClass("Lescompat/Array;", &arrayCls);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    status = env->Class_FindMethod(arrayCls, "<ctor>", "I:V", &arrayCtor);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    status = env->Object_New(arrayCls, arrayCtor, &arrayObj, values.size());
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    for (size_t i = 0; i < values.size(); i++) {
        string = nullptr;
        status = env->String_NewUTF8(values[i].c_str(), values[i].size(), &string);
        if (status != ANI_OK) {
            HILOGI("status : %{public}d", status);
            return false;
        }

        status = env->Object_CallMethodByName_Void(arrayObj, "$_set", "ILstd/core/Object;:V", i, string);
        if (status != ANI_OK) {
            HILOGI("status : %{public}d", status);
            return false;
        }
    }
    status = env->Object_SetField_Ref(object, field, arrayObj);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    return true;
}

bool GetStdString(ani_env *env, ani_string str, std::string &res)
{
    ani_size sz {};
    ani_status status = ANI_ERROR;
    if ((status = env->String_GetUTF8Size(str, &sz)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    res.resize(sz + 1);
    if ((status = env->String_GetUTF8SubString(str, 0, sz, res.data(), res.size(), &sz)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    res.resize(sz);
    return true;
}

ani_string GetAniString(ani_env *env, const std::string &str)
{
    ani_string aniStr = nullptr;
    ani_status status = env->String_NewUTF8(str.c_str(), str.size(), &aniStr);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return nullptr;
    }
    return aniStr;
}

ani_array_ref GetAniArrayString(ani_env *env, const std::vector<std::string> &values)
{
    ani_array_ref aArrayRef = nullptr;
    return aArrayRef;
}

bool GetRefPropertyByName(ani_env *env, ani_object param, const char *name, ani_ref &ref)
{
    ani_status status = ANI_ERROR;
    if ((status = env->Object_GetPropertyByName_Ref(param, name, &ref)) != ANI_OK) {
        HILOGI("Object_GetFieldByName_Ref failed, status : %{public}d", status);
        return false;
    }

    ani_boolean isUndefined = true;
    if ((status = env->Reference_IsUndefined(ref, &isUndefined)) != ANI_OK) {
        HILOGI("Reference_IsUndefined failed, status : %{public}d", status);
        return false;
    }
    if (isUndefined) {
        HILOGI("wantParams is undefined");
        return false;
    }
    isUndefined = true;

    if ((status = env->Reference_IsNull(ref, &isUndefined)) != ANI_OK) {
        HILOGI("Reference_IsNull failed, status : %{public}d", status);
        return false;
    }

    if (isUndefined) {
        HILOGI("wantParams is null");
        return false;
    }
    isUndefined = true;

    if ((status = env->Reference_IsNullishValue(ref, &isUndefined)) != ANI_OK) {
        HILOGI("Reference_IsNullishValue failed, status : %{public}d", status);
        return false;
    }

    if (isUndefined) {
        HILOGI("wantParams is nullish");
        return false;
    }
    return true;
}

ani_object createDouble(ani_env *env, ani_double value)
{
    ani_class persion_cls;
    ani_status status = ANI_ERROR;
    if ((status = env->FindClass(CLASSNAME_DOUBLE, &persion_cls)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return nullptr;
    }
    ani_method personInfoCtor;
    if ((status = env->Class_FindMethod(persion_cls, "<ctor>", "D:V", &personInfoCtor)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return nullptr;
    }
    ani_object personInfoObj;
    if ((status = env->Object_New(persion_cls, personInfoCtor, &personInfoObj, value)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return nullptr;
    }
    return personInfoObj;
}

ani_object createBoolean(ani_env *env, ani_boolean value)
{
    ani_class persion_cls;
    ani_status status = ANI_ERROR;
    if ((status = env->FindClass(CLASSNAME_BOOLEAN, &persion_cls)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return nullptr;
    }
    ani_method personInfoCtor;
    if ((status = env->Class_FindMethod(persion_cls, "<ctor>", "Z:V", &personInfoCtor)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return nullptr;
    }
    ani_object personInfoObj;
    if ((status = env->Object_New(persion_cls, personInfoCtor, &personInfoObj, value)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return nullptr;
    }
    return personInfoObj;
}

bool SetFieldString(ani_env *env, ani_class cls, ani_object object,
    const std::string &fieldName, const std::string &value)
{
    ani_field field = nullptr;
    ani_string string = nullptr;
    ani_status status = env->Class_FindField(cls, fieldName.c_str(), &field);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    if (value.empty()) {
        ani_ref nullRef = nullptr;
        if ((status = env->GetNull(&nullRef)) != ANI_OK) {
            HILOGI("status : %{public}d", status);
            return false;
        }
        if ((status = env->Object_SetField_Ref(object, field, nullRef)) != ANI_OK) {
            HILOGI("status : %{public}d", status);
            return false;
        }
        return true;
    }

    if ((status = env->String_NewUTF8(value.c_str(), value.size(), &string)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }

    if ((status = env->Object_SetField_Ref(object, field, string)) != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    return true;
}

bool SetFieldDouble(ani_env *env, ani_class cls, ani_object object, const std::string &fieldName, double value)
{
    ani_field field = nullptr;
    ani_status status = env->Class_FindField(cls, fieldName.c_str(), &field);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    ani_object obj = createDouble(env, value);
    if (obj == nullptr) {
        HILOGI("createDouble failed");
        return false;
    }
    status = env->Object_SetField_Ref(object, field, reinterpret_cast<ani_ref>(obj));
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    return true;
}

bool SetFieldBoolean(ani_env *env, ani_class cls, ani_object object, const std::string &fieldName, bool value)
{
    ani_field field = nullptr;
    ani_status status = env->Class_FindField(cls, fieldName.c_str(), &field);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    status = env->Object_SetField_Boolean(object, field, value);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        return false;
    }
    return true;
}

bool SetFieldInt(ani_env *env, ani_class cls, ani_object object, const std::string &fieldName, int value)
{
    ani_field field = nullptr;
    ani_status status = env->Class_FindField(cls, fieldName.c_str(), &field);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        HILOGI("status : %{public}s", fieldName.c_str());
        return false;
    }
    status = env->Object_SetField_Int(object, field, value);
    if (status != ANI_OK) {
        HILOGI("status : %{public}d", status);
        HILOGI("status : %{public}s", fieldName.c_str());
        return false;
    }
    return true;
}

bool SetFieldRef(ani_env *env, ani_class cls, ani_object object, const std::string &fieldName, ani_ref value)
{
    ani_field field = nullptr;
    ani_status status = env->Class_FindField(cls, fieldName.c_str(), &field);
    if (status != ANI_OK) {
        HILOGI("FindField %{public}s failed, status: %{public}d", fieldName.c_str(), status);
        return false;
    }
    status = env->Object_SetField_Ref(object, field, value);
    if (status != ANI_OK) {
        HILOGI("SetField_Ref %{public}s failed, status: %{public}d", fieldName.c_str(), status);
        return false;
    }
    return true;
}
}  // namespace AppExecFwk
}  // namespace OHOS