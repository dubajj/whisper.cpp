#include "common-audio.h"

#include <SDL.h>
#include <SDL_audio.h>

namespace {
    void data_callback(void * userdata, uint8_t * stream, int len) {
        audio_async * audio = (audio_async *) userdata; 
        const size_t n_samples = len / sizeof(float);
        audio_data_callback(audio->internal_audio_data(), reinterpret_cast<float*>(stream), n_samples);
    }
}
struct audio_async::impl {
    SDL_AudioDeviceID dev_id_in = 0;

    ~impl() {
        if (dev_id_in) {
            SDL_CloseAudioDevice(dev_id_in);
        }
    }
};

audio_async::audio_async(int len_ms)
    : m_impl_detail(new audio_async::impl())
 {
    m_len_ms = len_ms;
}

audio_async::~audio_async() {
    delete m_impl_detail;
}

bool audio_async::init(int capture_id, int sample_rate) {
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetHintWithPriority(SDL_HINT_AUDIO_RESAMPLING_MODE, "medium", SDL_HINT_OVERRIDE);

    {
        int nDevices = SDL_GetNumAudioDevices(SDL_TRUE);
        fprintf(stderr, "%s: found %d capture devices:\n", __func__, nDevices);
        for (int i = 0; i < nDevices; i++) {
            fprintf(stderr, "%s:    - Capture device #%d: '%s'\n", __func__, i, SDL_GetAudioDeviceName(i, SDL_TRUE));
        }
    }

    SDL_AudioSpec capture_spec_requested;
    SDL_AudioSpec capture_spec_obtained;

    SDL_zero(capture_spec_requested);
    SDL_zero(capture_spec_obtained);

    capture_spec_requested.freq     = sample_rate;
    capture_spec_requested.format   = AUDIO_F32;
    capture_spec_requested.channels = 1;
    capture_spec_requested.samples  = 1024;
    capture_spec_requested.callback = data_callback;
    capture_spec_requested.userdata = this;

    if (capture_id >= 0) {
        fprintf(stderr, "%s: attempt to open capture device %d : '%s' ...\n", __func__, capture_id, SDL_GetAudioDeviceName(capture_id, SDL_TRUE));
        m_impl_detail->dev_id_in = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(capture_id, SDL_TRUE), SDL_TRUE, &capture_spec_requested, &capture_spec_obtained, 0);
    } else {
        fprintf(stderr, "%s: attempt to open default capture device ...\n", __func__);
        m_impl_detail->dev_id_in = SDL_OpenAudioDevice(nullptr, SDL_TRUE, &capture_spec_requested, &capture_spec_obtained, 0);
    }

    if (!m_impl_detail->dev_id_in) {
        fprintf(stderr, "%s: couldn't open an audio device for capture: %s!\n", __func__, SDL_GetError());
        m_impl_detail->dev_id_in = 0;

        return false;
    } else {
        fprintf(stderr, "%s: obtained spec for input device (SDL Id = %d):\n", __func__, m_impl_detail->dev_id_in);
        fprintf(stderr, "%s:     - sample rate:       %d\n",                   __func__, capture_spec_obtained.freq);
        fprintf(stderr, "%s:     - format:            %d (required: %d)\n",    __func__, capture_spec_obtained.format,
                capture_spec_requested.format);
        fprintf(stderr, "%s:     - channels:          %d (required: %d)\n",    __func__, capture_spec_obtained.channels,
                capture_spec_requested.channels);
        fprintf(stderr, "%s:     - samples per frame: %d\n",                   __func__, capture_spec_obtained.samples);
    }

    m_sample_rate = capture_spec_obtained.freq;

    m_audio_data.audio.resize((m_sample_rate*m_len_ms)/1000);

    return true;
}

bool audio_async::resume() {
    if (!m_impl_detail->dev_id_in) {
        fprintf(stderr, "%s: no audio device to resume!\n", __func__);
        return false;
    }

    if (m_audio_data.running) {
        fprintf(stderr, "%s: already running!\n", __func__);
        return false;
    }

    SDL_PauseAudioDevice(m_impl_detail->dev_id_in, 0);

    m_audio_data.running = true;

    return true;
}

bool audio_async::pause() {
    if (!m_impl_detail->dev_id_in) {
        fprintf(stderr, "%s: no audio device to pause!\n", __func__);
        return false;
    }

    if (!m_audio_data.running) {
        fprintf(stderr, "%s: already paused!\n", __func__);
        return false;
    }

    SDL_PauseAudioDevice(m_impl_detail->dev_id_in, 1);

    m_audio_data.running = false;

    return true;
}

bool audio_async::clear() {
    if (!m_impl_detail->dev_id_in) {
        fprintf(stderr, "%s: no audio device to clear!\n", __func__);
        return false;
    }

    if (!m_audio_data.running) {
        fprintf(stderr, "%s: not running!\n", __func__);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_audio_data.copy_mutex);

        m_audio_data.audio_pos = 0;
        m_audio_data.audio_len = 0;
    }

    return true;
}

void audio_async::get(int ms, std::vector<float> & result) {
    if (!m_impl_detail->dev_id_in) {
        fprintf(stderr, "%s: no audio device to get audio from!\n", __func__);
        return;
    }
    if (ms <= 0) {
        ms = m_len_ms;
    }
    
    size_t n_samples = (m_sample_rate * ms) / 1000;
    m_audio_data.copy_to(n_samples, result);
}

bool poll_keep_running() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                {
                    return false;
                } break;
            default:
                break;
        }
    }

    return true;
}
