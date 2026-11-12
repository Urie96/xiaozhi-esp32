//--------------------------------------------------------------
//-- Oscillator.pde
//-- Generate sinusoidal oscillations in the servos
//--------------------------------------------------------------
//-- (c) Juan Gonzalez-Gomez (Obijuan), Dec 2011
//-- (c) txp666 for esp32, 202503
//-- GPL license
//--------------------------------------------------------------
#include "oscillator.h"

#include <driver/ledc.h>
#include <esp_timer.h>

#include <algorithm>
#include <cmath>

static const char* TAG = "Oscillator";

extern unsigned long IRAM_ATTR millis();

// 一个 LEDC 通道驱动一个舵机，通道全用 LEDC_LOW_SPEED_MODE + timer 1。
// 这里记录占用情况，避免重复 Attach 时把通道漏掉（原来用
// `last_channel = (last_channel + 1) % 7 + 1` 轮转，第二次 Attach 会重新
// 分配通道并让旧通道一直被占用，4 个舵机挂两次就把 8 个通道吃光）。
static bool channel_used[LEDC_CHANNEL_MAX] = {};

Oscillator::Oscillator(int trim) {
    trim_ = trim;
    diff_limit_ = 0;
    is_attached_ = false;

    sampling_period_ = 30;
    period_ = 2000;
    number_samples_ = period_ / sampling_period_;
    inc_ = 2 * M_PI / number_samples_;

    amplitude_ = 45;
    phase_ = 0;
    phase0_ = 0;
    offset_ = 0;
    stop_ = false;
    rev_ = false;

    pos_ = 90;
    previous_millis_ = 0;
}

Oscillator::~Oscillator() { Detach(); }

uint32_t Oscillator::AngleToCompare(int angle) {
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) /
               (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) +
           SERVO_MIN_PULSEWIDTH_US;
}

bool Oscillator::NextSample() {
    current_millis_ = millis();

    if (current_millis_ - previous_millis_ > sampling_period_) {
        previous_millis_ = current_millis_;
        return true;
    }

    return false;
}

void Oscillator::Attach(int pin, bool rev) {
    if (is_attached_) {
        Detach();
    }

    pin_ = pin;
    rev_ = rev;

    ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                      .duty_resolution = LEDC_TIMER_13_BIT,
                                      .timer_num = LEDC_TIMER_1,
                                      .freq_hz = 50,
                                      .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_t channel = LEDC_CHANNEL_MAX;
    for (int i = 0; i < LEDC_CHANNEL_MAX; i++) {
        if (!channel_used[i]) {
            channel = (ledc_channel_t)i;
            channel_used[i] = true;
            break;
        }
    }
    if (channel == LEDC_CHANNEL_MAX) {
        ESP_LOGE(TAG, "no free LEDC channel left for GPIO%d", (int)pin_);
        ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
    }
    ledc_channel_ = channel;

    ledc_channel_config_t ledc_channel = {.gpio_num = pin_,
                                          .speed_mode = LEDC_LOW_SPEED_MODE,
                                          .channel = ledc_channel_,
                                          .intr_type = LEDC_INTR_DISABLE,
                                          .timer_sel = LEDC_TIMER_1,
                                          .duty = 0,
                                          .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_speed_mode_ = LEDC_LOW_SPEED_MODE;

    // pos_ = 90;
    // Write(pos_);
    previous_servo_command_millis_ = millis();

    is_attached_ = true;

    // 舵机在收到第一个 SetPosition() 之前是 0 duty（没脉冲）。所以看到
    // 这行日志但舵机完全不抖，问题一定在接线/供电，而不是固件。
    ESP_LOGI(TAG, "GPIO%d attached to LEDC channel %d (timer 1, 50 Hz, 13 bit)", (int)pin_,
             (int)ledc_channel_);
}

void Oscillator::Detach() {
    if (!is_attached_)
        return;

    ESP_ERROR_CHECK(ledc_stop(ledc_speed_mode_, ledc_channel_, 0));
    channel_used[ledc_channel_] = false;

    is_attached_ = false;
}

void Oscillator::SetT(unsigned int T) {
    period_ = T;

    number_samples_ = period_ / sampling_period_;
    inc_ = 2 * M_PI / number_samples_;
}

void Oscillator::SetPosition(int position) { Write(position); }

void Oscillator::Refresh() {
    if (NextSample()) {
        if (!stop_) {
            int pos = std::round(amplitude_ * std::sin(phase_ + phase0_) + offset_);
            if (rev_)
                pos = -pos;
            Write(pos + 90);
        }

        phase_ = phase_ + inc_;
    }
}

void Oscillator::Write(int position) {
    if (!is_attached_)
        return;

    long currentMillis = millis();
    if (diff_limit_ > 0) {
        int limit = std::max(
            1, (((int)(currentMillis - previous_servo_command_millis_)) * diff_limit_) / 1000);
        if (abs(position - pos_) > limit) {
            pos_ += position < pos_ ? -limit : limit;
        } else {
            pos_ = position;
        }
    } else {
        pos_ = position;
    }
    previous_servo_command_millis_ = currentMillis;

    int angle = pos_ + trim_;

    angle = std::min(std::max(angle, 0), 180);

    uint32_t duty = (uint32_t)(((angle / 180.0) * 2.0 + 0.5) * 8191 / 20.0);

    ESP_ERROR_CHECK(ledc_set_duty(ledc_speed_mode_, ledc_channel_, duty));
    ESP_ERROR_CHECK(ledc_update_duty(ledc_speed_mode_, ledc_channel_));
}