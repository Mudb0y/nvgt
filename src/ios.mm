/* ios.mm - code included only on the IOS platform
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

#include <string>
using namespace std;

bool voice_over_is_running() {
	return false;
}

bool voice_over_speak(const std::string& message, bool interrupt) {
	return false;
}

void voice_over_window_created() {

}

void voice_over_speech_shutdown() {

}

std::string apple_input_box(const std::string& title, const std::string& message, const std::string& default_value, bool secure, bool readonly) {
	return "";
}

