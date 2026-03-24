#include <jni.h>
#include <string>
#include "ShmServer.h"

ShmServer server;

extern "C" JNIEXPORT jstring JNICALL
Java_com_kotlinx_shmipcc_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";

    LOGD("init shm server invoke");

    bool initResult = server.initShmServer();

    LOGD("init shm server result is %d: ", initResult);

    return env->NewStringUTF(hello.c_str());
}
extern "C"
JNIEXPORT void JNICALL
Java_com_kotlinx_shmipcc_MainActivity_benchTestSendData(JNIEnv *env, jobject thiz) {
    std::vector<ShmServerSession*> allServerSession = server.getAllShmServerSessionMap();
    LOGD("shm server session size is %d: ", allServerSession.size());
    std::string test = "hello shmipc";

    ShmServerSession* firstServerSession = allServerSession[0];
    if(firstServerSession != nullptr) {
        LOGI("write server data size is %d, data str is %s", test.size(), test.data());
        firstServerSession->writData(reinterpret_cast<const uint8_t*>(test.data()), test.size());
    }
}