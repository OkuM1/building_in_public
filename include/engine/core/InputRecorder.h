#pragma once
#include <vector>
#include <string>
#include "game/components/GameComponents.h"

namespace engine {

class InputRecorder {
public:
    enum class State {
        IDLE,
        RECORDING,
        PLAYBACK
    };

    InputRecorder();

    void StartRecording();
    void StopRecording(const std::string& filename);
    void StartPlayback(const std::string& filename);
    void StopPlayback();

    // Returns true if we are in playback mode and overwrote the input
    bool ProcessInput(game::PlayerInput& input);

    State GetState() const { return state; }
    size_t GetFrameCount() const { return frames.size(); }
    size_t GetCurrentFrame() const { return currentFrame; }

private:
    State state;
    std::vector<game::PlayerInput> frames;
    size_t currentFrame;
};

} // namespace engine
