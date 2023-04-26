#include "common-audio.h"
#ifdef __APPLE__
    #define MA_NO_RUNTIME_LINKING
#endif
#define MA_NO_WAV
#define MINIAUDIO_IMPLEMENTATION
// #define MA_DEBUG_OUTPUT
#include "miniaudio.h"

#include <csignal>

namespace {
    std::atomic_bool shutdown_request = false;
}

void data_callback(ma_device* device, void* /*pOutput*/, const void* input, ma_uint32 num_frames) {
    audio_async * audio = reinterpret_cast<audio_async*>(device->pUserData);
    MA_ASSERT(audio != NULL);
    audio_data_callback(audio->internal_audio_data(), reinterpret_cast<const float*>(input), num_frames);
}
void signal_handler(int /*signal*/) {
    shutdown_request = true; // any signal (INT / ABRT / ...) shuts down the stream
}

struct audio_async::impl {
    ma_context context;
    ma_device device;

    ~impl() {
        ma_device_uninit(&device);
        ma_context_uninit(&context);
    }
};

audio_async::audio_async(int len_ms)
    : m_impl_detail(new audio_async::impl())
 {
    m_len_ms = len_ms;
    
    std::signal(SIGINT, signal_handler);
}

audio_async::~audio_async() {
    delete m_impl_detail;
}

bool audio_async::init(int capture_id, int sample_rate) {
    
    ma_result result;

    ma_device_info* playback_infos;
    ma_uint32 playback_count;
    ma_device_info* capture_infos;
    ma_uint32 capture_count;

    result = ma_context_init(NULL, 0, NULL, &m_impl_detail->context);

    if (result != MA_SUCCESS) {
        fprintf(stderr, "%s: couldn't open audio context: %d!\n", __func__, result);
        return false;
    }
    result = ma_context_get_devices(&m_impl_detail->context, &playback_infos, &playback_count, &capture_infos, &capture_count);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "%s: couldn't retrieve device information: %d!\n", __func__, result);
        return false;
    }

    ma_device_config device_config = ma_device_config_init(ma_device_type_capture);

    
    fprintf(stderr, "%s: found %d capture devices:\n", __func__, capture_count);
    for (auto device_idx = 0u; device_idx < capture_count; ++device_idx) {
        fprintf(stderr, "%s:    - Capture device #%d: '%s'\n", __func__, device_idx, capture_infos[device_idx].name);
    }
    for (auto device_idx = 0u; device_idx < capture_count; ++device_idx) {
        if (device_idx == capture_id) {
            fprintf(stderr, "%s: attempt to open capture device %d : '%s' ...\n", __func__, capture_id, capture_infos[device_idx].name);
            device_config.capture.pDeviceID = &capture_infos[device_idx].id;
            break;
        }
    }

    device_config.capture.format = ma_format_f32;
    device_config.capture.channels = 1;
    device_config.sampleRate = sample_rate;
    device_config.dataCallback = data_callback;
    device_config.pUserData = this;
    // device_config.capture.pDeviceID = &device_id;

    result = ma_device_init(NULL, &device_config, &m_impl_detail->device);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "%s: couldn't open an audio device for capture: %d!\n", __func__, result);
        return false;
    }
    

    
    // } else {
    //     fprintf(stderr, "%s: obtained spec for input device (SDL Id = %d):\n", __func__, m_impl_detail->dev_id_in);
    //     fprintf(stderr, "%s:     - sample rate:       %d\n",                   __func__, capture_spec_obtained.freq);
    //     fprintf(stderr, "%s:     - format:            %d (required: %d)\n",    __func__, capture_spec_obtained.format,
    //             capture_spec_requested.format);
    //     fprintf(stderr, "%s:     - channels:          %d (required: %d)\n",    __func__, capture_spec_obtained.channels,
    //             capture_spec_requested.channels);
    //     fprintf(stderr, "%s:     - samples per frame: %d\n",                   __func__, capture_spec_obtained.samples);
    // }

    m_sample_rate = sample_rate;

    m_audio_data.audio.resize((m_sample_rate*m_len_ms)/1000);

    return true;
}

bool audio_async::resume() {
    auto device_state = ma_device_get_state(&m_impl_detail->device);
    if (device_state == ma_device_state_uninitialized) {
        fprintf(stderr, "%s: no audio device to resume!\n", __func__);
        return false;
    }

    if (device_state != ma_device_state_stopped) {
        fprintf(stderr, "%s: already running!\n", __func__);
        return false;
    }

    ma_device_start(&m_impl_detail->device);

    m_audio_data.running = true;

    return true;
}

bool audio_async::pause() {
    auto device_state = ma_device_get_state(&m_impl_detail->device);
    if (device_state == ma_device_state_uninitialized) {
        fprintf(stderr, "%s: no audio device to pause!\n", __func__);
        return false;
    }

    if (device_state == ma_device_state_stopped) {
        fprintf(stderr, "%s: already paused!\n", __func__);
        return false;
    }
    ma_device_stop(&m_impl_detail->device);

    m_audio_data.running = false;

    return true;
}

bool audio_async::clear() {
    auto device_state = ma_device_get_state(&m_impl_detail->device);
    if (device_state == ma_device_state_uninitialized) {
        fprintf(stderr, "%s: no audio device to clear!\n", __func__);
        return false;
    }

    if (device_state != ma_device_state_started) {
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
    if (ma_device_get_state(&m_impl_detail->device) == ma_device_state_uninitialized) {
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
    return !shutdown_request;
}
