#include "engine/core/InputRecorder.h"
#include "engine/core/Logger.h"
#include <fstream>

namespace engine {

InputRecorder::InputRecorder() : state(State::IDLE), currentFrame(0) {}

void InputRecorder::StartRecording() {
    state = State::RECORDING;
    frames.clear();
    currentFrame = 0;
    Logger::Info("STARTED RECORDING input...");
}

void InputRecorder::StopRecording(const std::string& filename) {
    if (state != State::RECORDING) return;

    state = State::IDLE;
    
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        Logger::Error("Failed to open file for recording: ", filename);
        return;
    }

    // Write number of frames
    size_t count = frames.size();
    outFile.write(reinterpret_cast<const char*>(&count), sizeof(count));

    // Write data
    outFile.write(reinterpret_cast<const char*>(frames.data()), frames.size() * sizeof(game::PlayerInput));
    
    Logger::Info("STOPPED RECORDING. Saved ", count, " frames to ", filename);
}

void InputRecorder::StartPlayback(const std::string& filename) {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        Logger::Error("Failed to open file for playback: ", filename);
        return;
    }

    // Read number of frames
    size_t count = 0;
    inFile.read(reinterpret_cast<char*>(&count), sizeof(count));

    frames.resize(count);
    inFile.read(reinterpret_cast<char*>(frames.data()), count * sizeof(game::PlayerInput));

    state = State::PLAYBACK;
    currentFrame = 0;
    Logger::Info("STARTED PLAYBACK. Loaded ", count, " frames from ", filename);
}

void InputRecorder::StopPlayback() {
    if (state == State::PLAYBACK) {
        state = State::IDLE;
        Logger::Info("STOPPED PLAYBACK.");
    }
}

bool InputRecorder::ProcessInput(game::PlayerInput& input) {
    if (state == State::RECORDING) {
        frames.push_back(input);
        return false; // We didn't modify the input, just recorded it
    } else if (state == State::PLAYBACK) {
        if (currentFrame < frames.size()) {
            input = frames[currentFrame];
            currentFrame++;
            return true; // We overwrote the input
        } else {
            StopPlayback();
            return false;
        }
    }
    return false;
}

} // namespace engine
