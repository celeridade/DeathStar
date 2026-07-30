#include "JniUtil.h"
#include <string>

namespace jniutil
{
    namespace
    {
        void Clear(JNIEnv* env)
        {
            if (env->ExceptionCheck())
                env->ExceptionClear();
        }

        // Reads java.lang.reflect.Field.getType().getName(), which is "int",
        // "float", "boolean", ... for primitives and the class name otherwise.
        bool FieldTypeMatches(JNIEnv* env, jobject field, FieldKind kind)
        {
            if (kind == FieldKind::Any)
                return true;

            jclass fieldCls = env->GetObjectClass(field);
            if (!fieldCls) { Clear(env); return false; }

            jmethodID getType = env->GetMethodID(fieldCls, "getType", "()Ljava/lang/Class;");
            env->DeleteLocalRef(fieldCls);
            if (!getType) { Clear(env); return false; }

            jclass type = (jclass)env->CallObjectMethod(field, getType);
            if (env->ExceptionCheck()) { env->ExceptionClear(); type = nullptr; }
            if (!type) return false;

            jclass classCls = env->GetObjectClass(type);
            jmethodID getName = classCls ? env->GetMethodID(classCls, "getName", "()Ljava/lang/String;") : nullptr;
            if (classCls) env->DeleteLocalRef(classCls);
            if (!getName) { Clear(env); env->DeleteLocalRef(type); return false; }

            jstring nameObj = (jstring)env->CallObjectMethod(type, getName);
            if (env->ExceptionCheck()) { env->ExceptionClear(); nameObj = nullptr; }
            env->DeleteLocalRef(type);
            if (!nameObj) return false;

            std::string typeName;
            if (const char* utf = env->GetStringUTFChars(nameObj, nullptr))
            {
                typeName = utf;
                env->ReleaseStringUTFChars(nameObj, utf);
            }
            env->DeleteLocalRef(nameObj);

            switch (kind)
            {
            case FieldKind::Int:     return typeName == "int";
            case FieldKind::Float:   return typeName == "float";
            case FieldKind::Boolean: return typeName == "boolean";
            case FieldKind::Object:
                // Anything that is not one of the eight primitives is a reference.
                return typeName != "int" && typeName != "float" && typeName != "boolean" &&
                       typeName != "long" && typeName != "double" && typeName != "short" &&
                       typeName != "byte" && typeName != "char" && typeName != "void";
            default:
                return true;
            }
        }
    }

    jfieldID FieldByName(JNIEnv* env, jclass clazz, const char* name, FieldKind kind)
    {
        if (!env || !clazz || !name || !*name)
            return nullptr;

        jclass classCls = env->FindClass("java/lang/Class");
        if (!classCls) { Clear(env); return nullptr; }

        jmethodID getDeclaredField = env->GetMethodID(classCls, "getDeclaredField", "(Ljava/lang/String;)Ljava/lang/reflect/Field;");
        jmethodID getSuperclass = env->GetMethodID(classCls, "getSuperclass", "()Ljava/lang/Class;");
        if (!getDeclaredField || !getSuperclass) { Clear(env); env->DeleteLocalRef(classCls); return nullptr; }

        jstring jname = env->NewStringUTF(name);
        if (!jname) { Clear(env); env->DeleteLocalRef(classCls); return nullptr; }

        jfieldID result = nullptr;
        jclass current = (jclass)env->NewLocalRef(clazz);

        // getDeclaredField does not look at superclasses, so walk them by hand.
        for (int depth = 0; current && depth < 12; depth++)
        {
            jobject field = env->CallObjectMethod(current, getDeclaredField, jname);
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
                field = nullptr;
            }

            if (field)
            {
                // A mapping can name a real field of the wrong type. Writing an
                // int into an object slot corrupts the heap, so refuse it here.
                if (!FieldTypeMatches(env, field, kind))
                {
                    env->DeleteLocalRef(field);
                    field = nullptr;
                }
            }

            if (field)
            {
                jclass fieldCls = env->GetObjectClass(field);
                jmethodID setAccessible = env->GetMethodID(fieldCls, "setAccessible", "(Z)V");
                if (setAccessible)
                {
                    env->CallVoidMethod(field, setAccessible, JNI_TRUE);
                    Clear(env);
                }
                env->DeleteLocalRef(fieldCls);

                result = env->FromReflectedField(field);
                Clear(env);
                env->DeleteLocalRef(field);

                if (result)
                    break;
            }

            jclass parent = (jclass)env->CallObjectMethod(current, getSuperclass);
            if (env->ExceptionCheck()) { env->ExceptionClear(); parent = nullptr; }

            env->DeleteLocalRef(current);
            current = parent;
        }

        if (current)
            env->DeleteLocalRef(current);

        env->DeleteLocalRef(jname);
        env->DeleteLocalRef(classCls);

        return result;
    }

    jobject StaticSelfInstance(JNIEnv* env, jclass clazz)
    {
        if (!env || !clazz)
            return nullptr;

        jclass classCls = env->FindClass("java/lang/Class");
        if (!classCls) { Clear(env); return nullptr; }

        jmethodID getDeclaredFields = env->GetMethodID(classCls, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
        if (!getDeclaredFields) { Clear(env); env->DeleteLocalRef(classCls); return nullptr; }

        jobjectArray fields = (jobjectArray)env->CallObjectMethod(clazz, getDeclaredFields);
        if (env->ExceptionCheck()) { env->ExceptionClear(); fields = nullptr; }
        if (!fields) { env->DeleteLocalRef(classCls); return nullptr; }

        jclass fieldCls = env->FindClass("java/lang/reflect/Field");
        jmethodID getType = fieldCls ? env->GetMethodID(fieldCls, "getType", "()Ljava/lang/Class;") : nullptr;
        jmethodID getModifiers = fieldCls ? env->GetMethodID(fieldCls, "getModifiers", "()I") : nullptr;
        jmethodID setAccessible = fieldCls ? env->GetMethodID(fieldCls, "setAccessible", "(Z)V") : nullptr;
        Clear(env);

        jobject result = nullptr;

        if (getType && getModifiers)
        {
            const jsize count = env->GetArrayLength(fields);

            for (jsize i = 0; i < count && !result; i++)
            {
                jobject field = env->GetObjectArrayElement(fields, i);
                if (!field) { Clear(env); continue; }

                const jint modifiers = env->CallIntMethod(field, getModifiers);
                Clear(env);

                // 0x8 is java.lang.reflect.Modifier.STATIC.
                if ((modifiers & 0x8) != 0)
                {
                    jclass type = (jclass)env->CallObjectMethod(field, getType);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); type = nullptr; }

                    if (type && env->IsSameObject(type, clazz))
                    {
                        if (setAccessible)
                        {
                            env->CallVoidMethod(field, setAccessible, JNI_TRUE);
                            Clear(env);
                        }

                        jfieldID id = env->FromReflectedField(field);
                        Clear(env);

                        if (id)
                        {
                            jobject value = env->GetStaticObjectField(clazz, id);
                            Clear(env);
                            if (value)
                                result = value;
                        }
                    }

                    if (type)
                        env->DeleteLocalRef(type);
                }

                env->DeleteLocalRef(field);
            }
        }

        if (fieldCls)
            env->DeleteLocalRef(fieldCls);
        env->DeleteLocalRef(fields);
        env->DeleteLocalRef(classCls);

        return result;
    }
}
