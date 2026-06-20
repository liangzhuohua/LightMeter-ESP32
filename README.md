# LightMeter-ESP32

基于 ESP32-S3 的专业胶卷相机入射式测光表，配备 460×460 AMOLED 触摸屏。

[English Version](README_EN.md)

## 功能特性

**核心测光**
- VEML7700 环境光传感器入射式测光（内置 Vishay AN 84323 官方多项式校正）
- 四种工作模式：手动、自动（光圈优先）、风光（f/11 偏置）、人像（大光圈偏置）
- 支持 1/3 EV 和 1/2 EV 档位步进
- 标准化光圈表（f/0.5–f/128）和快门表（30s–1/8000s）
- 光圈/快门超限警告、过曝/欠曝提示、慢速快门警告（建议三脚架）

**相机/镜头管理**
- 多相机配置，记录快门范围与闪光同步速度
- 多镜头配置，记录光圈范围与焦距
- T9 键盘自定义命名

**网络功能**
- WiFi 自动连接与重连，支持多 SSID 记忆
- WiFi AP 扫描自动定位（cellocation API）
- SNTP 时间同步，根据经度自动计算时区
- 和风天气 API：3 天天气预报、月相、日出日落

**电源管理**
- MAX17055 电量计实时监测电池电量和电压
- TP4056 充电检测（充电中 / 已充满 / 未充电）
- 深度休眠（RTC 保持时间），短按 GPIO9 按键进入，再次短按唤醒
- TP4056 检测到充满后自动校准 MAX17055 SOC 至 100%

**用户界面**
- 460×460 AMOLED 电容触摸屏
- 滚轮选择器（ISO、光圈、快门、EV 补偿）
- 实时 WiFi 信号、电量、光照强度显示
- OTA 固件升级（AP 热点 + HTTP 上传）

## 界面截图

![主界面](docs/GUI1.png)

![设置界面](docs/GUI2.png)

## 硬件配置

| 组件 | 型号 |
|------|------|
| MCU | ESP32-S3N16R8（16MB Flash，8MB Octal PSRAM） |
| 显示屏 | 460×460 QSPI AMOLED（[CO5300](https://yuyinglcd.com/ch/products/2/5/512)） |
| 触摸 IC | CST820（I2C） |
| 光照传感器 | VEML7700（I2C） |
| 电量计 | MAX17055（I2C） |
| 充电芯片 | TP4056 |
| 电源转换 | SY8088 DC-DC 降压 |
| 电池 | 300mAh 锂电池 |

### 原理图

![原理图](docs/schematic.png)

完整原理图 PDF 和各芯片数据手册见 [`Datasheet/`](Datasheet/) 目录。PCB 工程文件见 [`hardware/`](hardware/) 目录。

## 编译与烧录

需安装 [ESP-IDF v5.2](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s3/index.html)。

```bash
idf.py set-target esp32s3   # 首次使用
idf.py build
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 flash monitor
```

快捷脚本：`./idf.sh B`（编译+烧录），`./idf.sh BM`（编译+烧录+监视）。

## 外部依赖

- [lvgl/lvgl](https://github.com/lvgl/lvgl) ^8 — 图形界面框架
- [espressif/esp_lcd_touch](https://components.espressif.com/components/espressif/esp_lcd_touch) ^1.1.2 — 触摸驱动
- [esp-idf-lib/veml7700](https://components.espressif.com/components/esp-idf-lib/veml7700) ^1.0.7 — 光照传感器驱动

由 ESP-IDF 组件管理器（`idf_component.yml`）统一管理。

## 项目结构

```
main/
├── main.c                    # 入口 — 外设初始化、LVGL 初始化
├── app_controller.c/h        # 中央调度器 — 任务、队列、信号量
├── app_ui.c/h                # LVGL UI 实现
├── app_ui_*_port.c/h         # UI 端口层（桥接 LVGL 回调与业务逻辑）
├── app_exposure_calc.c/h     # 曝光计算引擎
├── app_location.c/h          # cellocation API 客户端 (WiFi AP BSSID 扫描)
├── app_weather.c/h           # 和风天气 API 客户端
├── app_time.c/h              # SNTP 时间同步 + RTC 持久化
├── app_battery.c/h           # 统一电池 API（MAX17055 + TP4056）
├── app_http_requests.c/h     # HTTP 请求辅助
├── app_nvs_storage.c/h       # NVS 持久化存储层
├── hw_oled.c/h               # QSPI AMOLED 显示驱动
├── hw_wifi.c/h               # WiFi 状态机（扫描、连接、断开）
├── hw_veml7700.c/h           # VEML7700 光照传感器驱动
├── hw_max17055.c/h           # MAX17055 电量计驱动
├── hw_tp4056.c/h             # TP4056 充电检测
├── hw_ota.c/h                # OTA 固件升级（HTTP）
├── hw_wakeup_key.c/h         # GPIO9 唤醒按键
├── bsp_i2c_init.c/h          # I2C 总线初始化
├── img_*.c                   # 嵌入式图片资源
├── clock_icon.c              # 模拟时钟表盘
├── qweather_icons.c          # 天气图标
└── SourceHanSansCN_Regular.c # 思源黑体中文字体
```

## 架构说明

**调度器模式** — `app_controller.c` 为中央枢纽，所有任务间通信通过 FreeRTOS 队列和信号量完成。

**串行请求链** — WiFi 连接成功后通过信号量触发串行请求：

```
WiFi 已连接
  → 定位（cellocation API — WiFi AP BSSID 扫描）
    → 时间同步（SNTP + 根据经度自动时区）
      → 天气更新（和风天气 3 天预报 + 月相）
```

每步最多重试 3 次。启动时优先显示缓存数据，确保 UI 不会空白。

**LVGL 线程安全** — 非 LVGL 任务更新 UI 必须使用 `lvgl_lock(-1)` / `lvgl_unlock()` 包裹。

详细架构文档见 [docs/软件系统组成.md](docs/软件系统组成.md) 和 [CLAUDE.md](CLAUDE.md)。

## 许可证

MIT
