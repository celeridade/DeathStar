#include "JniUtil.h"

namespace jniutil
{
    namespace
    {
        void Clear(JNIEnv* env)
        {
            if (env->ExceptionCheck())
                env->ExceptionClear();
        }
    }

    jfieldID FieldByName(JNIEnv* env, jclass clazz, const char* name)
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
