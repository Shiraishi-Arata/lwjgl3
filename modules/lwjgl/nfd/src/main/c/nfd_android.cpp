/*
  Native File Dialog Extended
  Repository: https://github.com/btzy/nativefiledialog-extended
  License: Zlib

  Android backend - bridges to Java via JNI to use Android's Storage Access Framework.
 */

#include <jni.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "nfd.h"

#define NFD_ANDROID_HELPER_CLASS "org/lwjgl/util/nfd/NFDAndroid"

static JavaVM* g_jvm = nullptr;
static jclass g_helperClass = nullptr;
static const char* g_errorstr = nullptr;

static void NFDi_SetError(const char* msg) {
    g_errorstr = msg;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    jclass clazz = env->FindClass(NFD_ANDROID_HELPER_CLASS);
    if (!clazz) {
        env->ExceptionClear();
        return JNI_VERSION_1_6;
    }

    g_helperClass = static_cast<jclass>(env->NewGlobalRef(clazz));
    env->DeleteLocalRef(clazz);

    return JNI_VERSION_1_6;
}

static JNIEnv* GetEnv() {
    JNIEnv* env = nullptr;
    jint result = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        if (g_jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
            return nullptr;
        }
    } else if (result != JNI_OK) {
        return nullptr;
    }
    return env;
}

static jstring strToJString(JNIEnv* env, const nfdnchar_t* str) {
    if (!str) return nullptr;
    return env->NewStringUTF(str);
}

static nfdnchar_t* jstringToStr(JNIEnv* env, jstring jstr) {
    if (!jstr) return nullptr;
    const char* utf = env->GetStringUTFChars(jstr, nullptr);
    if (!utf) return nullptr;
    size_t len = strlen(utf);
    nfdnchar_t* result = static_cast<nfdnchar_t*>(malloc(len + 1));
    if (result) {
        memcpy(result, utf, len + 1);
    }
    env->ReleaseStringUTFChars(jstr, utf);
    return result;
}

const char* NFD_GetError(void) {
    return g_errorstr;
}

void NFD_ClearError(void) {
    NFDi_SetError(nullptr);
}

void NFD_FreePathN(nfdnchar_t* filePath) {
    assert(filePath);
    free(filePath);
}

void NFD_FreePathU8(nfdu8char_t* filePath) {
    NFD_FreePathN(filePath);
}

nfdresult_t NFD_Init(void) {
    if (!g_jvm || !g_helperClass) {
        NFDi_SetError("NFD not available on Android without NFDAndroid helper class.");
        return NFD_ERROR;
    }
    return NFD_OKAY;
}

void NFD_Quit(void) {
}

static nfdresult_t CallDialogMethod(JNIEnv* env, const char* methodName,
                                    const char* methodSig, jstring jDefaultPath,
                                    jstring jDefaultName, jstring* outResult) {
    jmethodID method = env->GetStaticMethodID(g_helperClass, methodName, methodSig);
    if (!method) {
        env->ExceptionClear();
        NFDi_SetError("Failed to find NFDAndroid helper method.");
        return NFD_ERROR;
    }

    jstring result = static_cast<jstring>(env->CallStaticObjectMethod(g_helperClass, method,
        jDefaultPath, jDefaultName));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        NFDi_SetError("Android file dialog threw an exception.");
        return NFD_ERROR;
    }

    if (result == nullptr) {
        return NFD_CANCEL;
    }

    *outResult = result;
    return NFD_OKAY;
}

static nfdresult_t CallDialogMethodWithFilters(JNIEnv* env, const char* methodName,
                                                const char* methodSig,
                                                const nfdnfilteritem_t* filterList,
                                                nfdfiltersize_t filterCount, jstring jDefaultPath,
                                                jstring jDefaultName, jstring* outResult) {
    jobjectArray jFilters = nullptr;
    if (filterCount > 0 && filterList) {
        jclass stringClass = env->FindClass("java/lang/String");
        jFilters = env->NewObjectArray(static_cast<jsize>(filterCount * 2), stringClass, nullptr);
        env->DeleteLocalRef(stringClass);
        if (!jFilters) {
            NFDi_SetError("Out of memory creating filter array.");
            return NFD_ERROR;
        }
        for (nfdfiltersize_t i = 0; i < filterCount; i++) {
            jstring jName = strToJString(env, filterList[i].name);
            jstring jSpec = strToJString(env, filterList[i].spec);
            env->SetObjectArrayElement(jFilters, static_cast<jsize>(i * 2), jName);
            env->SetObjectArrayElement(jFilters, static_cast<jsize>(i * 2 + 1), jSpec);
            env->DeleteLocalRef(jName);
            env->DeleteLocalRef(jSpec);
        }
    }

    jmethodID method = env->GetStaticMethodID(g_helperClass, methodName, methodSig);
    if (!method) {
        env->ExceptionClear();
        env->DeleteLocalRef(jFilters);
        NFDi_SetError("Failed to find NFDAndroid helper method.");
        return NFD_ERROR;
    }

    jstring result = static_cast<jstring>(env->CallStaticObjectMethod(g_helperClass, method,
        jFilters, jDefaultPath, jDefaultName));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(jFilters);
        NFDi_SetError("Android file dialog threw an exception.");
        return NFD_ERROR;
    }

    env->DeleteLocalRef(jFilters);

    if (result == nullptr) {
        return NFD_CANCEL;
    }

    *outResult = result;
    return NFD_OKAY;
}

static nfdresult_t CallDialogMultipleMethodWithFilters(JNIEnv* env, const char* methodName,
                                                        const char* methodSig,
                                                        const nfdnfilteritem_t* filterList,
                                                        nfdfiltersize_t filterCount,
                                                        jstring jDefaultPath,
                                                        const nfdpathset_t** outPaths) {
    jobjectArray jFilters = nullptr;
    if (filterCount > 0 && filterList) {
        jclass stringClass = env->FindClass("java/lang/String");
        jFilters = env->NewObjectArray(static_cast<jsize>(filterCount * 2), stringClass, nullptr);
        env->DeleteLocalRef(stringClass);
        if (!jFilters) {
            NFDi_SetError("Out of memory creating filter array.");
            return NFD_ERROR;
        }
        for (nfdfiltersize_t i = 0; i < filterCount; i++) {
            jstring jName = strToJString(env, filterList[i].name);
            jstring jSpec = strToJString(env, filterList[i].spec);
            env->SetObjectArrayElement(jFilters, static_cast<jsize>(i * 2), jName);
            env->SetObjectArrayElement(jFilters, static_cast<jsize>(i * 2 + 1), jSpec);
            env->DeleteLocalRef(jName);
            env->DeleteLocalRef(jSpec);
        }
    }

    jmethodID method = env->GetStaticMethodID(g_helperClass, methodName, methodSig);
    if (!method) {
        env->ExceptionClear();
        env->DeleteLocalRef(jFilters);
        NFDi_SetError("Failed to find NFDAndroid helper method.");
        return NFD_ERROR;
    }

    jobjectArray result = static_cast<jobjectArray>(env->CallStaticObjectMethod(g_helperClass,
        method, jFilters, jDefaultPath));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(jFilters);
        NFDi_SetError("Android file dialog threw an exception.");
        return NFD_ERROR;
    }

    env->DeleteLocalRef(jFilters);

    if (result == nullptr) {
        return NFD_CANCEL;
    }

    jsize count = env->GetArrayLength(result);
    char** pathArray = static_cast<char**>(malloc(sizeof(char*) * (count + 1)));
    if (!pathArray) {
        env->DeleteLocalRef(result);
        NFDi_SetError("Out of memory.");
        return NFD_ERROR;
    }
    pathArray[count] = nullptr;

    for (jsize i = 0; i < count; i++) {
        jstring jPath = static_cast<jstring>(env->GetObjectArrayElement(result, i));
        const char* utf = env->GetStringUTFChars(jPath, nullptr);
        if (utf) {
            pathArray[i] = static_cast<char*>(malloc(strlen(utf) + 1));
            if (pathArray[i]) {
                memcpy(pathArray[i], utf, strlen(utf) + 1);
            }
            env->ReleaseStringUTFChars(jPath, utf);
        } else {
            pathArray[i] = nullptr;
        }
        env->DeleteLocalRef(jPath);
    }

    env->DeleteLocalRef(result);

    *outPaths = static_cast<const void*>(pathArray);
    return NFD_OKAY;
}

nfdresult_t NFD_OpenDialogN(nfdnchar_t** outPath,
                             const nfdnfilteritem_t* filterList,
                             nfdfiltersize_t filterCount,
                             const nfdnchar_t* defaultPath) {
    JNIEnv* env = GetEnv();
    if (!env) {
        NFDi_SetError("JNI environment not available.");
        return NFD_ERROR;
    }

    jstring jDefaultPath = strToJString(env, defaultPath);
    jstring jResult = nullptr;

    nfdresult_t res = CallDialogMethodWithFilters(env, "openDialog",
        "([Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
        filterList, filterCount, jDefaultPath, nullptr, &jResult);

    env->DeleteLocalRef(jDefaultPath);

    if (res != NFD_OKAY) return res;

    *outPath = jstringToStr(env, jResult);
    env->DeleteLocalRef(jResult);
    return NFD_OKAY;
}

nfdresult_t NFD_OpenDialogU8(nfdu8char_t** outPath,
                              const nfdu8filteritem_t* filterList,
                              nfdfiltersize_t filterCount,
                              const nfdu8char_t* defaultPath) {
    return NFD_OpenDialogN(outPath, filterList, filterCount, defaultPath);
}

nfdresult_t NFD_OpenDialogN_With_Impl(nfdversion_t version,
                                       nfdnchar_t** outPath,
                                       const nfdopendialognargs_t* args) {
    (void)version;
    return NFD_OpenDialogN(outPath, args->filterList, args->filterCount, args->defaultPath);
}

nfdresult_t NFD_OpenDialogU8_With_Impl(nfdversion_t version,
                                        nfdu8char_t** outPath,
                                        const nfdopendialogu8args_t* args) {
    return NFD_OpenDialogN_With_Impl(version, outPath, args);
}

nfdresult_t NFD_OpenDialogMultipleN(const nfdpathset_t** outPaths,
                                     const nfdnfilteritem_t* filterList,
                                     nfdfiltersize_t filterCount,
                                     const nfdnchar_t* defaultPath) {
    JNIEnv* env = GetEnv();
    if (!env) {
        NFDi_SetError("JNI environment not available.");
        return NFD_ERROR;
    }

    jstring jDefaultPath = strToJString(env, defaultPath);

    nfdresult_t res = CallDialogMultipleMethodWithFilters(env, "openDialogMultiple",
        "([Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;",
        filterList, filterCount, jDefaultPath, outPaths);

    env->DeleteLocalRef(jDefaultPath);
    return res;
}

nfdresult_t NFD_OpenDialogMultipleU8(const nfdpathset_t** outPaths,
                                      const nfdu8filteritem_t* filterList,
                                      nfdfiltersize_t filterCount,
                                      const nfdu8char_t* defaultPath) {
    return NFD_OpenDialogMultipleN(outPaths, filterList, filterCount, defaultPath);
}

nfdresult_t NFD_OpenDialogMultipleN_With_Impl(nfdversion_t version,
                                               const nfdpathset_t** outPaths,
                                               const nfdopendialognargs_t* args) {
    (void)version;
    return NFD_OpenDialogMultipleN(outPaths, args->filterList, args->filterCount, args->defaultPath);
}

nfdresult_t NFD_OpenDialogMultipleU8_With_Impl(nfdversion_t version,
                                                const nfdpathset_t** outPaths,
                                                const nfdopendialogu8args_t* args) {
    return NFD_OpenDialogMultipleN_With_Impl(version, outPaths, args);
}

nfdresult_t NFD_SaveDialogN(nfdnchar_t** outPath,
                             const nfdnfilteritem_t* filterList,
                             nfdfiltersize_t filterCount,
                             const nfdnchar_t* defaultPath,
                             const nfdnchar_t* defaultName) {
    JNIEnv* env = GetEnv();
    if (!env) {
        NFDi_SetError("JNI environment not available.");
        return NFD_ERROR;
    }

    jstring jDefaultPath = strToJString(env, defaultPath);
    jstring jDefaultName = strToJString(env, defaultName);
    jstring jResult = nullptr;

    nfdresult_t res = CallDialogMethodWithFilters(env, "saveDialog",
        "([Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
        filterList, filterCount, jDefaultPath, jDefaultName, &jResult);

    env->DeleteLocalRef(jDefaultPath);
    env->DeleteLocalRef(jDefaultName);

    if (res != NFD_OKAY) return res;

    *outPath = jstringToStr(env, jResult);
    env->DeleteLocalRef(jResult);
    return NFD_OKAY;
}

nfdresult_t NFD_SaveDialogU8(nfdu8char_t** outPath,
                              const nfdu8filteritem_t* filterList,
                              nfdfiltersize_t filterCount,
                              const nfdu8char_t* defaultPath,
                              const nfdu8char_t* defaultName) {
    return NFD_SaveDialogN(outPath, filterList, filterCount, defaultPath, defaultName);
}

nfdresult_t NFD_SaveDialogN_With_Impl(nfdversion_t version,
                                       nfdnchar_t** outPath,
                                       const nfdsavedialognargs_t* args) {
    (void)version;
    return NFD_SaveDialogN(outPath, args->filterList, args->filterCount,
                           args->defaultPath, args->defaultName);
}

nfdresult_t NFD_SaveDialogU8_With_Impl(nfdversion_t version,
                                        nfdu8char_t** outPath,
                                        const nfdsavedialogu8args_t* args) {
    return NFD_SaveDialogN_With_Impl(version, outPath, args);
}

nfdresult_t NFD_PickFolderN(nfdnchar_t** outPath, const nfdnchar_t* defaultPath) {
    JNIEnv* env = GetEnv();
    if (!env) {
        NFDi_SetError("JNI environment not available.");
        return NFD_ERROR;
    }

    jstring jDefaultPath = strToJString(env, defaultPath);
    jstring jResult = nullptr;

    nfdresult_t res = CallDialogMethod(env, "pickFolder",
        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
        jDefaultPath, nullptr, &jResult);

    env->DeleteLocalRef(jDefaultPath);

    if (res != NFD_OKAY) return res;

    *outPath = jstringToStr(env, jResult);
    env->DeleteLocalRef(jResult);
    return NFD_OKAY;
}

nfdresult_t NFD_PickFolderU8(nfdu8char_t** outPath, const nfdu8char_t* defaultPath) {
    return NFD_PickFolderN(outPath, defaultPath);
}

nfdresult_t NFD_PickFolderN_With_Impl(nfdversion_t version,
                                       nfdnchar_t** outPath,
                                       const nfdpickfoldernargs_t* args) {
    (void)version;
    return NFD_PickFolderN(outPath, args->defaultPath);
}

nfdresult_t NFD_PickFolderU8_With_Impl(nfdversion_t version,
                                        nfdu8char_t** outPath,
                                        const nfdpickfolderu8args_t* args) {
    return NFD_PickFolderN_With_Impl(version, outPath, args);
}

nfdresult_t NFD_PickFolderMultipleN(const nfdpathset_t** outPaths,
                                     const nfdnchar_t* defaultPath) {
    JNIEnv* env = GetEnv();
    if (!env) {
        NFDi_SetError("JNI environment not available.");
        return NFD_ERROR;
    }

    jstring jDefaultPath = strToJString(env, defaultPath);

    nfdresult_t res = CallDialogMultipleMethodWithFilters(env, "pickFolderMultiple",
        "([Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;",
        nullptr, 0, jDefaultPath, outPaths);

    env->DeleteLocalRef(jDefaultPath);
    return res;
}

nfdresult_t NFD_PickFolderMultipleU8(const nfdpathset_t** outPaths,
                                      const nfdu8char_t* defaultPath) {
    return NFD_PickFolderMultipleN(outPaths, defaultPath);
}

nfdresult_t NFD_PickFolderMultipleN_With_Impl(nfdversion_t version,
                                               const nfdpathset_t** outPaths,
                                               const nfdpickfoldernargs_t* args) {
    (void)version;
    return NFD_PickFolderMultipleN(outPaths, args->defaultPath);
}

nfdresult_t NFD_PickFolderMultipleU8_With_Impl(nfdversion_t version,
                                                const nfdpathset_t** outPaths,
                                                const nfdpickfolderu8args_t* args) {
    return NFD_PickFolderMultipleN_With_Impl(version, outPaths, args);
}

nfdresult_t NFD_PathSet_GetCount(const nfdpathset_t* pathSet, nfdpathsetsize_t* count) {
    assert(pathSet);
    const char** pathArray = static_cast<const char**>(const_cast<nfdpathset_t*>(pathSet));
    *count = 0;
    while (pathArray[*count]) {
        (*count)++;
    }
    return NFD_OKAY;
}

nfdresult_t NFD_PathSet_GetPathN(const nfdpathset_t* pathSet,
                                  nfdpathsetsize_t index,
                                  nfdnchar_t** outPath) {
    assert(pathSet);
    const char** pathArray = static_cast<const char**>(const_cast<nfdpathset_t*>(pathSet));
    const char* path = pathArray[index];
    if (!path) {
        NFDi_SetError("Path index out of bounds.");
        return NFD_ERROR;
    }
    *outPath = static_cast<nfdnchar_t*>(malloc(strlen(path) + 1));
    if (*outPath) {
        memcpy(*outPath, path, strlen(path) + 1);
    }
    return NFD_OKAY;
}

nfdresult_t NFD_PathSet_GetPathU8(const nfdpathset_t* pathSet,
                                   nfdpathsetsize_t index,
                                   nfdu8char_t** outPath) {
    return NFD_PathSet_GetPathN(pathSet, index, outPath);
}

void NFD_PathSet_FreePathN(const nfdnchar_t* filePath) {
    NFD_FreePathN(const_cast<nfdnchar_t*>(filePath));
}

void NFD_PathSet_FreePathU8(const nfdu8char_t* filePath) {
    NFD_FreePathU8(const_cast<nfdu8char_t*>(filePath));
}

nfdresult_t NFD_PathSet_GetEnum(const nfdpathset_t* pathSet, nfdpathsetenum_t* outEnumerator) {
    outEnumerator->ptr = const_cast<void*>(pathSet);
    return NFD_OKAY;
}

void NFD_PathSet_FreeEnum(nfdpathsetenum_t*) {
}

nfdresult_t NFD_PathSet_EnumNextN(nfdpathsetenum_t* enumerator, nfdnchar_t** outPath) {
    const char** pathArray = static_cast<const char**>(enumerator->ptr);
    if (pathArray && *pathArray) {
        *outPath = static_cast<nfdnchar_t*>(malloc(strlen(*pathArray) + 1));
        if (*outPath) {
            memcpy(*outPath, *pathArray, strlen(*pathArray) + 1);
        }
        enumerator->ptr = static_cast<void*>(pathArray + 1);
    } else {
        *outPath = nullptr;
    }
    return NFD_OKAY;
}

nfdresult_t NFD_PathSet_EnumNextU8(nfdpathsetenum_t* enumerator, nfdu8char_t** outPath) {
    return NFD_PathSet_EnumNextN(enumerator, outPath);
}

void NFD_PathSet_Free(const nfdpathset_t* pathSet) {
    if (!pathSet) return;
    const char** pathArray = static_cast<const char**>(const_cast<nfdpathset_t*>(pathSet));
    for (const char** p = pathArray; *p; ++p) {
        free(const_cast<char*>(*p));
    }
    free(pathArray);
}
