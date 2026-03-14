#include <jni.h>
#include <string>
#include "ShmServer.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_kotlinx_shmipcc_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";

    LOGD("init shm server invoke");

    ShmServer server;
    bool initResult = server.initShmServer();

    LOGD("init shm server result is %d: ", initResult);

    return env->NewStringUTF(hello.c_str());
}