#include <jni.h>
#include <string>
#include "ShmServer.h"
#include "ShmBenchTester.h"

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

    for (int i = 0; i < 500; ++i) {
        ShmBenchTester::benchTest(firstServerSession, 1024);
    }
}