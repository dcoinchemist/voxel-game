#include <jni.h>
#include <string>
#include "engine/renderer.h"

extern "C" JNIEXPORT void JNICALL
Java_com_voxelgame_NativeBridge_nativeInit(JNIEnv*, jobject) {
    rendererInit();
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxelgame_NativeBridge_nativeResize(JNIEnv*, jobject, jint width, jint height) {
    rendererResize(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxelgame_NativeBridge_nativeFrame(JNIEnv*, jobject) {
    rendererFrame();
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxelgame_NativeBridge_nativeToggleWireframe(JNIEnv*, jobject) {
    rendererToggleWireframe();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_voxelgame_NativeBridge_nativeStats(JNIEnv* env, jobject) {
    std::string s = rendererStats();
    return env->NewStringUTF(s.c_str());
}
