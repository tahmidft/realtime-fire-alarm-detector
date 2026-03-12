#include <alsa/asoundlib.h>
#include <fftw3.h>
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <sstream>

const int ALARM_FREQ_MIN = 3000;
const int ALARM_FREQ_MAX = 3600;
const float ALARM_THRESHOLD = -20.0;
const float SILENCE_THRESHOLD = -45.0;
const int MIN_BEEP_DURATION = 3;
const int MAX_BEEP_DURATION = 15;

const std::string LOG_FILE = "/home/pizero/Projects/RTFireAlarmDetectionSystem/detections.jsonl";
const std::string STATUS_FILE = "/home/pizero/Projects/RTFireAlarmDetectionSystem/status.json";

// Get ISO 8601 timestamp
std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%dT%H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// Append detection event to JSONL log (one JSON object per line)
void logDetection(const std::string& event, int frequency, float magnitude_db, int beep_count) {
    std::ofstream log(LOG_FILE, std::ios::app);
    if (log.is_open()) {
        log << "{\"timestamp\":\"" << getTimestamp() << "\","
            << "\"event\":\"" << event << "\","
            << "\"frequency\":" << frequency << ","
            << "\"magnitude_db\":" << std::fixed << std::setprecision(1) << magnitude_db << ","
            << "\"beep_count\":" << beep_count << "}\n";
        log.close();
    }
}

// Write current status (Flask reads this for live dashboard)
void updateStatus(const std::string& state, int frequency, float magnitude_db, int beep_count, bool alarm_active) {
    std::ofstream status(STATUS_FILE);
    if (status.is_open()) {
        status << "{\"timestamp\":\"" << getTimestamp() << "\","
               << "\"state\":\"" << state << "\","
               << "\"frequency\":" << frequency << ","
               << "\"magnitude_db\":" << std::fixed << std::setprecision(1) << magnitude_db << ","
               << "\"beep_count\":" << beep_count << ","
               << "\"alarm_active\":" << (alarm_active ? "true" : "false") << "}";
        status.close();
    }
}

class FireAlarmDetector {
private:
    enum State { IDLE, IN_BEEP, IN_GAP };
    State current_state = IDLE;
    int beep_count = 0;
    int frame_counter = 0;
    std::chrono::steady_clock::time_point last_reset;

public:
    FireAlarmDetector() : last_reset(std::chrono::steady_clock::now()) {}

    bool detectPattern(bool alarm_present) {
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_reset).count() > 10) {
            reset();
        }

        switch (current_state) {
            case IDLE:
                if (alarm_present) {
                    current_state = IN_BEEP;
                    frame_counter = 1;
                    last_reset = now;
                }
                break;

            case IN_BEEP:
                if (alarm_present) {
                    frame_counter++;
                    if (frame_counter > MAX_BEEP_DURATION) {
                        reset();
                    }
                } else {
                    if (frame_counter >= MIN_BEEP_DURATION) {
                        beep_count++;
                        current_state = IN_GAP;
                        frame_counter = 0;
                        last_reset = now;
                    } else {
                        reset();
                    }
                }
                break;

            case IN_GAP:
                if (alarm_present) {
                    current_state = IN_BEEP;
                    frame_counter = 1;
                    last_reset = now;
                } else {
                    frame_counter++;
                    if (frame_counter > 30) {
                        reset();
                    }
                }
                break;
        }

        return beep_count >= 3;
    }

    void reset() {
        current_state = IDLE;
        beep_count = 0;
        frame_counter = 0;
    }

    int getBeepCount() const { return beep_count; }
    
    std::string getStateString() const {
        switch (current_state) {
            case IDLE: return "idle";
            case IN_BEEP: return "beep";
            case IN_GAP: return "gap";
            default: return "unknown";
        }
    }
};

int main() {
    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params;

    const char *device = "plughw:0";
    unsigned int rate = 48000;
    int channels = 1;
    const int BUFFER_SIZE = 4096;
    int32_t buffer[BUFFER_SIZE];

    double *fft_in = (double*)fftw_malloc(sizeof(double) * BUFFER_SIZE);
    fftw_complex *fft_out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (BUFFER_SIZE/2 + 1));
    fftw_plan plan = fftw_plan_dft_r2c_1d(BUFFER_SIZE, fft_in, fft_out, FFTW_ESTIMATE);

    FireAlarmDetector detector;
    bool alarm_was_active = false;

    if (snd_pcm_open(&capture_handle, device, SND_PCM_STREAM_CAPTURE, 0) < 0) {
        std::cerr << "Cannot open audio device" << std::endl;
        return 1;
    }

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

    std::cout << "Fire Alarm Detector Active" << std::endl;
    std::cout << "Monitoring: " << ALARM_FREQ_MIN << "-" << ALARM_FREQ_MAX << " Hz" << std::endl;
    std::cout << "Threshold: " << ALARM_THRESHOLD << " dB" << std::endl;
    std::cout << "Log file: " << LOG_FILE << std::endl;
    std::cout << "Status file: " << STATUS_FILE << "\n" << std::endl;

    int frame_count = 0;

    while (true) {
        int frames_read = snd_pcm_readi(capture_handle, buffer, BUFFER_SIZE);

        if (frames_read < 0) {
            frames_read = snd_pcm_recover(capture_handle, frames_read, 0);
            if (frames_read < 0) break;
        }

        for (int i = 0; i < BUFFER_SIZE; i++) {
            fft_in[i] = buffer[i] / 2147483648.0;
        }

        fftw_execute(plan);

        float max_magnitude = 0.0;
        int max_freq = 0;

        for (int i = 1; i < BUFFER_SIZE/2; i++) {
            float freq = (float)i * rate / BUFFER_SIZE;

            if (freq >= ALARM_FREQ_MIN && freq <= ALARM_FREQ_MAX) {
                float real = fft_out[i][0];
                float imag = fft_out[i][1];
                float magnitude = sqrt(real*real + imag*imag);

                if (magnitude > max_magnitude) {
                    max_magnitude = magnitude;
                    max_freq = (int)freq;
                }
            }
        }

        float magnitude_db = 20.0 * log10(max_magnitude + 1e-10);
        bool alarm_present = (magnitude_db > ALARM_THRESHOLD && max_freq > 0);
        bool fire_alarm = detector.detectPattern(alarm_present);

        // Log fire alarm detection (only once when triggered)
        if (fire_alarm && !alarm_was_active) {
            logDetection("fire_alarm", max_freq, magnitude_db, detector.getBeepCount());
            std::cout << "\n*** LOGGED: Fire alarm detection ***\n" << std::endl;
        }
        alarm_was_active = fire_alarm;

        // Update status file every 10 frames (~1 second) for live dashboard
        if (frame_count % 10 == 0) {
            updateStatus(detector.getStateString(), max_freq, magnitude_db, 
                        detector.getBeepCount(), fire_alarm);

            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

            std::cout << "[" << std::put_time(std::localtime(&time_t_now), "%H:%M:%S")
                      << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";

            std::cout << std::setfill(' ') << std::setw(4) << max_freq << " Hz | "
                      << std::fixed << std::setprecision(1) << std::setw(6) << magnitude_db
                      << " dB | Beeps: " << detector.getBeepCount() << " | ";

            if (fire_alarm) {
                std::cout << "FIRE ALARM DETECTED!";
            } else if (alarm_present) {
                std::cout << "Beep detected";
            } else {
                std::cout << "Monitoring";
            }

            std::cout << std::endl;
        }

        frame_count++;
    }

    fftw_destroy_plan(plan);
    fftw_free(fft_in);
    fftw_free(fft_out);
    snd_pcm_close(capture_handle);

    return 0;
}
