#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "mcp_server.h"
#include "system_reset.h"
#include "wifi_board.h"

#include <esp_log.h>

#define TAG "S3QuadDog"

// 四足步态 / 舵机控制 / MCP 工具（self.dog.*）在该文件中注册
extern void InitializeQuadDogController();

class S3QuadDog : public WifiBoard {
private:
    Button boot_button_;
    Button touch_button_;

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        touch_button_.OnPressDown([this]() { Application::GetInstance().StartListening(); });
        touch_button_.OnPressUp([this]() { Application::GetInstance().StopListening(); });
    }

public:
    S3QuadDog() : boot_button_(BOOT_BUTTON_GPIO), touch_button_(TOUCH_BUTTON_GPIO) {
        InitializeQuadDogController();
        InitializeButtons();
        ESP_LOGI(TAG, "S3 quad dog initialized");
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                               AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
                                               AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK,
                                               AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
                                              AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    // 没有显示屏：沿用 Board::GetDisplay() 的 NoDisplay 默认实现。
};

DECLARE_BOARD(S3QuadDog);
