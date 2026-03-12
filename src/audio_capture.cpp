#include <alsa/asoundlib.h>
#include <iostream>
#include <cmath>

int main() {
    // ALSA setup
    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params;
    
    // Your working audio parameters
    const char *device = "plughw:0";
    unsigned int rate = 48000;  // 48kHz sample rate
    int channels = 1;            // mono
    
    // Open audio device
    if (snd_pcm_open(&capture_handle, device, SND_PCM_STREAM_CAPTURE, 0) < 0) {
        std::cerr << "Cannot open audio device " << device << std::endl;
        return 1;
    }
    
    // Configure audio parameters
    snd_pcm_hw_params_malloc(&hw_params);
    snd_pcm_hw_params_any(capture_handle, hw_params);
    snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S32_LE);
    snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0);
    snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels);
    
    if (snd_pcm_hw_params(capture_handle, hw_params) < 0) {
        std::cerr << "Cannot set parameters" << std::endl;
        return 1;
    }
    
    snd_pcm_hw_params_free(hw_params);
    snd_pcm_prepare(capture_handle);
    
    std::cout << "Audio capture started. Press Ctrl+C to stop." << std::endl;
    
    // Buffer for audio samples (0.1 second chunks)
    const int frames = rate / 10;  // 4800 frames = 0.1 sec
    int32_t buffer[frames];
    
    // Capture loop
    int loop_count = 0;
    while (true) {
        int err = snd_pcm_readi(capture_handle, buffer, frames);
        
        if (err < 0) {
            std::cerr << "\nBuffer underrun, recovering..." << std::endl;
            snd_pcm_prepare(capture_handle);
            continue;
        }
        
        // Debug: Check first few samples every 10 loops
        if (loop_count % 10 == 0) {
            std::cout << "\nSample values: " << buffer[0] << ", " << buffer[100] << ", " << buffer[1000] << std::endl;
        }
        
        // Calculate DC offset (average value)
        int64_t dc_sum = 0;
        for (int i = 0; i < frames; i++) {
            dc_sum += buffer[i];
        }
        int32_t dc_offset = dc_sum / frames;
        
        // Calculate volume with DC offset removed
        double sum = 0;
        int32_t min_sample = buffer[0] - dc_offset;
        int32_t max_sample = buffer[0] - dc_offset;
        
        for (int i = 0; i < frames; i++) {
            int32_t corrected = buffer[i] - dc_offset;  // Remove DC offset
            
            if (corrected < min_sample) min_sample = corrected;
            if (corrected > max_sample) max_sample = corrected;
            
            double sample = corrected / 2147483648.0;  // Normalize
            sum += sample * sample;
        }
        
        double rms = sqrt(sum / frames);
        double volume_db = 20 * log10(rms + 1e-10);
        int32_t peak_amplitude = (max_sample - min_sample) / 2;  // Peak-to-peak / 2
        
        // Display volume bar with better info
        std::cout << "Volume: " << volume_db << " dB | Peak: " << peak_amplitude 
                  << " | RMS: " << rms << " |";
        int bars = (int)((volume_db + 60) / 2);  // Scale for display
        for (int i = 0; i < bars && i < 40; i++) std::cout << "█";
        std::cout << "          \r" << std::flush;
        
        loop_count++;
    }
    
    snd_pcm_close(capture_handle);
    return 0;
}