#include "common-audio.h"


void audio_data_callback(audio_data * ad, const float * stream, size_t n_samples) {
    if (!ad || !ad->running) { return; }

    //fprintf(stderr, "%s: %zu samples, pos %zu, len %zu\n", __func__, n_samples, ad->audio_pos, ad->audio_len);
    {
        std::lock_guard<std::mutex> lock(ad->copy_mutex);

        if (ad->audio_pos + n_samples > ad->audio.size()) {
            const size_t n0 = ad->audio.size() - ad->audio_pos;

            memcpy(&ad->audio[ad->audio_pos], stream, n0 * sizeof(float));
            memcpy(&ad->audio[0], &stream[n0], (n_samples - n0) * sizeof(float));

            ad->audio_pos = (ad->audio_pos + n_samples) % ad->audio.size();
            ad->audio_len = ad->audio.size();
        } else {
            memcpy(&ad->audio[ad->audio_pos], stream, n_samples * sizeof(float));

            ad->audio_pos = (ad->audio_pos + n_samples) % ad->audio.size();
            ad->audio_len = std::min(ad->audio_len + n_samples, ad->audio.size());
        }
    }
}

void audio_data::copy_to(size_t n_samples, std::vector<float> &result) {
    if (!running) {
        fprintf(stderr, "%s: not running!\n", __func__);
        return;
    }

    result.clear();

    {
        std::lock_guard<std::mutex> lock(copy_mutex);

        if (n_samples > audio_len) {
            n_samples = audio_len;
        }

        result.resize(n_samples);

        auto s0 = audio_pos - n_samples;
        if (s0 < 0) {
            s0 += audio.size();
        }

        if (s0 + n_samples > audio.size()) {
            const size_t n0 = audio.size() - s0;

            memcpy(result.data(), &audio[s0], n0 * sizeof(float));
            memcpy(&result[n0], &audio[0], (n_samples - n0) * sizeof(float));
        } else {
            memcpy(result.data(), &audio[s0], n_samples * sizeof(float));
        }
    }
}
