# S3 Quad Dog

基于 ESP32-S3-N16R8（16MB Flash + 8MB 八线 PSRAM）的四足机器狗开发板，小智 AI 语音助手固件。

## 硬件

| 功能 | 器件 | 引脚 |
| --- | --- | --- |
| 麦克风 | INMP441 | SCK 5, WS 4, SD 6（L/R 接地，左声道） |
| 功放 | NS4168 | BCLK 15, LRCLK 16, SDATA 7（无使能脚） |
| 左前舵机 | SG90/MG90 | 17 |
| 左后舵机 | | 18 |
| 右前舵机 | | 13 |
| 右后舵机 | | 14 |
| BOOT 按键 | | 0 |

- 采样率：输入 16000Hz，输出 24000Hz，麦克风与功放各占一个 I2S 通道（Simplex）。
- 4 路舵机由 LEDC（50Hz，13bit）驱动，不占用 I2S 与 USB。
- 无显示屏、无触摸按键：固件使用 `Board::GetDisplay()` 的 `NoDisplay` 回退，不影响语音功能。
- 4 个舵机请用独立 5V 电源供电并与主板共地，不要从 3.3V LDO 取电（堵转电流可达 1A 以上）。

## 编译

固件默认（本板推荐的参数）是**中文界面 + “Hi, ESP” 唤醒词**：

```bash
python3 scripts/build.py urie/s3-quad-dog --name s3-quad-dog \
  --language zh-CN --wake-word hiesp
```

`--language` / `--wake-word` 是用户构建选项，不能写进 `config.json`
（见 `docs/custom-board.md`），所以每次重新编译都要带上这两个参数，否则会退回
`en-US` + “Hi, ESP”（英文）的默认组合。

其它常用组合：

```bash
--language zh-CN                         # 中文，唤醒词自动变成“你好小智”
--language zh-CN --wake-word disabled    # 中文，关闭唤醒词
--list-wake-words                        # 查看本芯片可用的全部模型
```

16MB Flash 与 `partitions/v2/16m.csv` 已由项目默认 `sdkconfig.defaults` 覆盖，本板
`config.json` 无需额外追加选项。

烧录：

```bash
idf.py flash monitor
```

## 动作控制

`quad_dog_controller.cc` 通过 MCP 注册了以下工具，AI 可直接调用：

| 工具 | 说明 |
| --- | --- |
| `self.dog.walk` | 行走（steps / speed / direction） |
| `self.dog.turn` | 转身（steps / speed / direction） |
| `self.dog.sit` / `self.dog.stand` | 坐下 / 站立 |
| `self.dog.stretch` / `self.dog.shake` | 伸懒腰 / 摇身体 |
| `self.dog.lift_*_leg` | 抬起指定一条腿 |
| `self.dog.home` | 回到初始姿态 |
| `self.dog.set_trim` / `self.dog.get_trims` | 舵机角度微调（写入 NVS 持久保存） |
| `self.dog.set_angle` | **直接**把某个舵机转到指定角度，用于检查接线/舵机编号 |
| `self.dog.servo_test` | 逐个舵机跑 45 → 135 → 90 度，排查接线和供电 |
| `self.dog.get_status` | 返回 `moving` / `idle` |

首次上电会依次执行 `HOME` 姿态，若腿部安装方向不一致，可用 `self.dog.set_trim`
（或修改 `quad_dog_controller.cc` 中的默认 trim）逐条校准。

## 舵机排查

- 启动日志里每个舵机都会打印
  `Oscillator: GPIO17 attached to LEDC channel 0 (timer 1, 50 Hz, 13 bit)`，
  串口里出现这一行说明 PWM 已经配置好；此后如果舵机纹丝不动，先怀疑接线/供电。
- 舵机在第一次 `SetPosition()` 之前是 0 duty（**没有脉冲**），所以上电后不动是正常的，
  真正的第一动是动作任务里的 `HOME`（四腿到 90°）。
- `坐下` 只动两条后腿（`QuadDog::Sit` 把 `LEFT_REAR` 打到 0°、`RIGHT_REAR` 打到 180°），
  前腿不动；`站起来` 就是 `Home()`，而 `Home()` 在“已经处于静止姿态”时直接返回，
  所以拿前腿试“坐下/站起来”看起来会像毫无反应。要单独验证某个舵机，用
  `self.dog.set_angle` 或 `self.dog.servo_test`。
- 4 个舵机请用独立 5V 电源并与主板共地；只接一个舵机时也必须共地，否则信号没有回路。

## 代码来源

步态算法与控制器派生自 `lceda-course-examples/eda-robot-pro`（Oscillator / 四足步态 /
MCP 控制器），音频接法与 `bread-compact-wifi` 的标准 Simplex 接法一致。
