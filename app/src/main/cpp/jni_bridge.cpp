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

extern "C" JNIEXPORT void JNICALL
Java_com_voxelgame_NativeBridge_nativeSetMove(JNIEnv*, jobject, jfloat x, jfloat y) {
    rendererSetMove(x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxelgame_NativeBridge_nativeLook(JNIEnv*, jobject, jfloat dYaw, jfloat dPitch) {
    rendererLook(dYaw, dPitch);
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxelgame_NativeBridge_nativeSetJump(JNIEnv*, jobject, jboolean pressed) {
    rendererSetJump(pressed == JNI_TRUE);
}
