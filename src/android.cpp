/* android.cpp - module containing functions only applicable to the Android platform, usually wrapping java counterparts via JNI
 *
 * NVGT - NonVisual Gaming Toolkit
 * Copyright (c) 2022-2025 Sam Tupy
 * https://nvgt.dev
 * This software is provided "as-is", without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
*/

#ifdef __ANDROID__
#include "android.h"
#include "UI.h"
#include <jni.h>
#include <Poco/Exception.h>
#include <Poco/Format.h>
#include <SDL3/SDL.h>
#include <stdexcept>
#include <memory>

static jclass TTSClass = nullptr;
static jmethodID midIsScreenReaderActive = nullptr;
static jmethodID midScreenReaderDetect = nullptr;
static jmethodID midScreenReaderSpeak = nullptr;
static jmethodID midScreenReaderSilence = nullptr;
static jmethodID midTTSGetEnginePackages = nullptr;
void android_setup_jni() {
	if (TTSClass) return;
	JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
	if (!env) throw Poco::Exception("cannot retrieve JNI environment");
	TTSClass = env->FindClass("com/samtupy/nvgt/TTS");
	if (!TTSClass) throw Poco::Exception("cannot find TTS class");
	TTSClass = (jclass)env->NewGlobalRef(TTSClass);
	midIsScreenReaderActive = env->GetStaticMethodID(TTSClass, "isScreenReaderActive", "()Z");
	midScreenReaderDetect = env->GetStaticMethodID(TTSClass, "screenReaderDetect", "()Ljava/lang/String;");
	midScreenReaderSpeak = env->GetStaticMethodID(TTSClass, "screenReaderSpeak", "(Ljava/lang/String;Z)Z");
	midScreenReaderSilence = env->GetStaticMethodID(TTSClass, "screenReaderSilence", "()Z");
	midTTSGetEnginePackages = env->GetStaticMethodID(TTSClass, "getEnginePackages", "()Ljava/util/List;");
}

bool android_is_screen_reader_active() {
	android_setup_jni();
	JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
	return env->CallStaticBooleanMethod(TTSClass, midIsScreenReaderActive);
}

std::string android_screen_reader_detect() {
	android_setup_jni();
	JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
	std::string result;
	jstring jreader = (jstring)env->CallStaticObjectMethod(TTSClass, midScreenReaderDetect);
	if (!jreader) return "";
	const char* utf = env->GetStringUTFChars(jreader, 0);
	if (utf) {
		result = utf;
		env->ReleaseStringUTFChars(jreader, utf);
	}
	env->DeleteLocalRef(jreader);
	return result;
}

bool android_screen_reader_speak(const std::string& text, bool interrupt) {
	android_setup_jni();
	JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
	jstring jtext = env->NewStringUTF(text.c_str());
	bool result = env->CallStaticBooleanMethod(TTSClass, midScreenReaderSpeak, jtext, interrupt);
	env->DeleteLocalRef(jtext);
	return result;
}

bool android_screen_reader_silence() {
	android_setup_jni();
	JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
	return env->CallStaticBooleanMethod(TTSClass, midScreenReaderSilence);
}

std::vector<std::string> android_get_tts_engine_packages() {
	try {
		android_setup_jni();
	} catch (...) {
		return {};
	}
	JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
	if (!env || !midTTSGetEnginePackages) return {};
	jobject jpackageList = env->CallStaticObjectMethod(TTSClass, midTTSGetEnginePackages);
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		return {};
	}
	if (!jpackageList) return {};
	jclass listClass = env->FindClass("java/util/List");
	if (!listClass) {
		env->DeleteLocalRef(jpackageList);
		return {};
	}
	jmethodID midSize = env->GetMethodID(listClass, "size", "()I");
	jmethodID midGet = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");
	if (!midSize || !midGet) {
		env->DeleteLocalRef(listClass);
		env->DeleteLocalRef(jpackageList);
		return {};
	}
	jint size = env->CallIntMethod(jpackageList, midSize);
	std::vector<std::string> result;
	for (int i = 0; i < size; i++) {
		jstring jpackage = (jstring)env->CallObjectMethod(jpackageList, midGet, i);
		if (jpackage) {
			const char *pkg = env->GetStringUTFChars(jpackage, NULL);
			if (pkg) {
				result.push_back(std::string(pkg));
				env->ReleaseStringUTFChars(jpackage, pkg);
			}
			env->DeleteLocalRef(jpackage);
		}
	}
	env->DeleteLocalRef(listClass);
	env->DeleteLocalRef(jpackageList);
	return result;
}

void register_native_tts() {
	std::vector<std::string> android_engines = android_get_tts_engine_packages();
	for (const auto& engine_pkg : android_engines) tts_engine_register(engine_pkg, [engine_pkg]() -> std::shared_ptr<tts_engine> { return std::make_shared<android_tts_engine>(engine_pkg); });
}

android_tts_engine::android_tts_engine(const std::string& enginePkg) : tts_engine_impl(enginePkg.empty()? "Android" : enginePkg), engine_package(enginePkg) {
	env = (JNIEnv *)SDL_GetAndroidJNIEnv();
	if (!env) throw std::runtime_error("Cannot retrieve JNI environment");
	TTSClass = env->FindClass("com/samtupy/nvgt/TTS");
	if (!TTSClass) throw std::runtime_error("Cannot find NVGT TTS class!");
	constructor = env->GetMethodID(TTSClass, "<init>", "(Ljava/lang/String;)V");
	if (!constructor) throw std::runtime_error("Cannot find NVGT TTS constructor!");
	jstring jengine = engine_package.empty()? nullptr : env->NewStringUTF(engine_package.c_str());
	TTSObj = env->NewObject(TTSClass, constructor, jengine);
	if (jengine) env->DeleteLocalRef(jengine);
	if (!TTSObj) throw std::runtime_error("Can't instantiate TTS object!");
	midIsActive = env->GetMethodID(TTSClass, "isActive", "()Z");
	midIsSpeaking = env->GetMethodID(TTSClass, "isSpeaking", "()Z");
	midSpeak = env->GetMethodID(TTSClass, "speak", "(Ljava/lang/String;Z)Z");
	midSilence = env->GetMethodID(TTSClass, "silence", "()Z");
	midGetVoice = env->GetMethodID(TTSClass, "getVoice", "()Ljava/lang/String;");
	midSetRate = env->GetMethodID(TTSClass, "setRate", "(F)Z");
	midSetPitch = env->GetMethodID(TTSClass, "setPitch", "(F)Z");
	midSetPan = env->GetMethodID(TTSClass, "setPan", "(F)V");
	midSetVolume = env->GetMethodID(TTSClass, "setVolume", "(F)V");
	midGetVoices = env->GetMethodID(TTSClass, "getVoices", "()Ljava/util/List;");
	midSetVoice = env->GetMethodID(TTSClass, "setVoice", "(Ljava/lang/String;)Z");
	midGetMaxSpeechInputLength = env->GetMethodID(TTSClass, "getMaxSpeechInputLength", "()I");
	midGetRate = env->GetMethodID(TTSClass, "getRate", "()F");
	midGetPitch = env->GetMethodID(TTSClass, "getPitch", "()F");
	midGetPan = env->GetMethodID(TTSClass, "getPan", "()F");
	midGetVolume = env->GetMethodID(TTSClass, "getVolume", "()F");
	midSpeakPcm = env->GetMethodID(TTSClass, "speakPcm", "(Ljava/lang/String;)[B");
	midGetPcmSampleRate = env->GetMethodID(TTSClass, "getPcmSampleRate", "()I");
	midGetPcmAudioFormat = env->GetMethodID(TTSClass, "getPcmAudioFormat", "()I");
	midGetPcmChannelCount = env->GetMethodID(TTSClass, "getPcmChannelCount", "()I");
	midGetVoiceCount = env->GetMethodID(TTSClass, "getVoiceCount", "()I");
	midGetVoiceName = env->GetMethodID(TTSClass, "getVoiceName", "(I)Ljava/lang/String;");
	midGetVoiceLanguage = env->GetMethodID(TTSClass, "getVoiceLanguage", "(I)Ljava/lang/String;");
	midSetVoiceByIndex = env->GetMethodID(TTSClass, "setVoiceByIndex", "(I)Z");
	midGetCurrentVoiceIndex = env->GetMethodID(TTSClass, "getCurrentVoiceIndex", "()I");
	if (!midIsActive || !midIsSpeaking || !midSpeak || !midSilence || !midGetVoice || !midSetRate || !midSetPitch || !midSetPan || !midSetVolume || !midGetVoices || !midSetVoice || !midGetMaxSpeechInputLength || !midGetPitch || !midGetPan || !midGetRate || !midGetVolume || !midSpeakPcm || !midGetPcmSampleRate || !midGetPcmAudioFormat || !midGetPcmChannelCount || !midGetVoiceCount || !midGetVoiceName || !midGetVoiceLanguage || !midSetVoiceByIndex || !midGetCurrentVoiceIndex) throw std::runtime_error("One or more methods on the TTS class could not be retrieved from JNI!");
	if (!env->CallBooleanMethod(TTSObj, midIsActive)) throw std::runtime_error("TTS engine could not be initialized!");
}

android_tts_engine::~android_tts_engine() {
	if (env && TTSObj) {
		env->DeleteLocalRef(TTSObj);
		TTSObj = nullptr;
	}
}

bool android_tts_engine::is_available() { return env && TTSObj && env->CallBooleanMethod(TTSObj, midIsActive); }
tts_pcm_generation_state android_tts_engine::get_pcm_generation_state() { return PCM_SUPPORTED; }

bool android_tts_engine::speak(const std::string &text, bool interrupt, bool blocking) {
	if (!env || !TTSObj || text.empty()) return false;
	jstring jtext = env->NewStringUTF(text.c_str());
	bool result = env->CallBooleanMethod(TTSObj, midSpeak, jtext, interrupt ? JNI_TRUE : JNI_FALSE);
	env->DeleteLocalRef(jtext);
	if (blocking) while (is_speaking()) wait(10);
	return result;
}

bool android_tts_engine::is_speaking() {
	if (!env || !TTSObj) return false;
	return env->CallBooleanMethod(TTSObj, midIsSpeaking) == JNI_TRUE;
}

bool android_tts_engine::stop() {
	if (!env || !TTSObj) return false;
	return env->CallBooleanMethod(TTSObj, midSilence);
}

float android_tts_engine::get_rate() {
	if (!env || !TTSObj) return 1;
	return env->CallFloatMethod(TTSObj, midGetRate);
}

float android_tts_engine::get_pitch() {
	if (!env || !TTSObj) return 1;
	return env->CallFloatMethod(TTSObj, midGetPitch);
}

float android_tts_engine::get_volume() {
	if (!env || !TTSObj) return 0;
	return env->CallFloatMethod(TTSObj, midGetVolume);
}

void android_tts_engine::set_rate(float rate) {
	if (env && TTSObj) env->CallBooleanMethod(TTSObj, midSetRate, rate);
}

void android_tts_engine::set_pitch(float pitch) {
	if (env && TTSObj) env->CallBooleanMethod(TTSObj, midSetPitch, pitch);
}

void android_tts_engine::set_volume(float volume) {
	if (env && TTSObj) env->CallBooleanMethod(TTSObj, midSetVolume, volume);
}

bool android_tts_engine::get_rate_range(float& minimum, float& midpoint, float& maximum) { minimum = 0.25; midpoint = 1; maximum = 4; return true; }
bool android_tts_engine::get_pitch_range(float& minimum, float& midpoint, float& maximum) { minimum = 0.25; midpoint = 1; maximum = 4; return true; }
bool android_tts_engine::get_volume_range(float& minimum, float& midpoint, float& maximum) { minimum = 0; midpoint = 0.5; maximum = 1; return true; }

int android_tts_engine::get_voice_count() {
	if (!env || !TTSObj) return 0;
	return env->CallIntMethod(TTSObj, midGetVoiceCount);
}

std::string android_tts_engine::get_voice_name(int index) {
	if (!env || !TTSObj) return "";
	jstring jvoiceName = (jstring)env->CallObjectMethod(TTSObj, midGetVoiceName, index);
	if (!jvoiceName) return "";
	const char *voiceName = env->GetStringUTFChars(jvoiceName, NULL);
	std::string result(voiceName);
	env->ReleaseStringUTFChars(jvoiceName, voiceName);
	env->DeleteLocalRef(jvoiceName);
	return result;
}

std::string android_tts_engine::get_voice_language(int index) {
	if (!env || !TTSObj) return "";
	jstring jlang = (jstring)env->CallObjectMethod(TTSObj, midGetVoiceLanguage, index);
	if (!jlang) return "";
	const char *lang = env->GetStringUTFChars(jlang, NULL);
	std::string result(lang);
	env->ReleaseStringUTFChars(jlang, lang);
	env->DeleteLocalRef(jlang);
	return result;
}

bool android_tts_engine::set_voice(int voice) {
	if (!env || !TTSObj) return false;
	bool result = env->CallBooleanMethod(TTSObj, midSetVoiceByIndex, voice);
	return result;
}

int android_tts_engine::get_current_voice() {
	if (!env || !TTSObj) return -1;
	return env->CallIntMethod(TTSObj, midGetCurrentVoiceIndex);
}

tts_audio_data* android_tts_engine::speak_to_pcm(const std::string &text) {
	if (!env || !TTSObj || text.empty()) return nullptr;

	jstring jtext = env->NewStringUTF(text.c_str());
	jbyteArray jpcmData = (jbyteArray)env->CallObjectMethod(TTSObj, midSpeakPcm, jtext);
	env->DeleteLocalRef(jtext);

	if (!jpcmData) return nullptr;

	// Get audio format information
	int pcmSampleRate = env->CallIntMethod(TTSObj, midGetPcmSampleRate);
	int pcmAudioFormat = env->CallIntMethod(TTSObj, midGetPcmAudioFormat);
	int pcmChannelCount = env->CallIntMethod(TTSObj, midGetPcmChannelCount);

	// Get PCM data
	jsize dataSize = env->GetArrayLength(jpcmData);
	jbyte* pcmBytes = env->GetByteArrayElements(jpcmData, NULL);

	if (!pcmBytes || dataSize <= 0) {
		env->DeleteLocalRef(jpcmData);
		return nullptr;
	}

	// Convert Android AudioFormat to bitsize
	// AudioFormat.ENCODING_PCM_8BIT = 3, AudioFormat.ENCODING_PCM_16BIT = 2, AudioFormat.ENCODING_PCM_FLOAT = 4
	unsigned int bitsize;
	switch (pcmAudioFormat) {
		case 3: bitsize = 8; break;
		case 2: bitsize = 16; break;
		case 4: bitsize = 32; break;
		default: bitsize = 16; break;
	}

	// Create audio data with context as global reference to JNI array
	jobject globalRef = env->NewGlobalRef(jpcmData);
	env->DeleteLocalRef(jpcmData);

	return new tts_audio_data(this, pcmBytes, dataSize, pcmSampleRate, pcmChannelCount, bitsize, globalRef);
}

void android_tts_engine::free_pcm(tts_audio_data* data) {
	if (!data || !data->context) return;
	env->ReleaseByteArrayElements((jbyteArray)data->context, (jbyte*)data->data, 0);
	data->data = nullptr;
	env->DeleteGlobalRef((jbyteArray)data->context);
	data->context = nullptr;
	tts_engine_impl::free_pcm(data);
}

bool screen_reader_load() { return true; }
void screen_reader_unload() {}
std::string screen_reader_detect() { return android_screen_reader_detect(); }
bool screen_reader_has_speech() { return android_is_screen_reader_active(); }
bool screen_reader_has_braille() { return false; }
bool screen_reader_is_speaking() { return false; }
bool screen_reader_output(const std::string& text, bool interrupt) { return android_screen_reader_speak(text, interrupt); }
bool screen_reader_speak(const std::string& text, bool interrupt) { return android_screen_reader_speak(text, interrupt); }
bool screen_reader_braille(const std::string& text) { return false; }
bool screen_reader_silence() { return android_screen_reader_silence(); }

#endif // __ANDROID__
