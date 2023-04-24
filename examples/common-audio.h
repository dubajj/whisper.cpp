#pragma once

#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>

//
// Audio capture
//

struct audio_data {
    audio_data() {
        running = false;
    }
    void copy_to(size_t n_samples, std::vector<float> & result);

    std::mutex       copy_mutex;
    std::atomic_bool running;

    std::vector<float> audio;
    size_t             audio_pos = 0;
    size_t             audio_len = 0;
};

class audio_async {
public:
    audio_async(int len_ms);
    ~audio_async();

    bool init(int capture_id, int sample_rate);

    // start capturing audio
    // keep last len_ms seconds of audio in a circular buffer
    bool resume();
    bool pause();
    bool clear();

    inline audio_data* internal_audio_data() { return &m_audio_data; }

    // get audio data from the circular buffer
    void get(int ms, std::vector<float> & audio);

private:
    struct impl;
    impl* m_impl_detail;

    int m_len_ms = 0;
    int m_sample_rate = 0;

    audio_data m_audio_data;
};

// callback to be called by Audio Engine
void audio_data_callback(audio_data * ad, const float * stream, size_t len);

// Return false if need to quit
bool poll_keep_running();
