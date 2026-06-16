# DroneCAN_ESP32

ESP32 的 DroneCAN 协议实现库，基于 [Libcanard](https://github.com/UAVCAN/libcanard) 和 [ACAN_ESP32](https://github.com/seeed-studio/ACAN_ESP32)。

## 简介

DroneCAN_ESP32 为 ESP32 微控制器提供完整的 DroneCAN 协议支持，支持广播、请求/响应等多种传输类型，并预置了丰富的 UAVCAN 消息类型定义。

## 功能特性

- **完整的 DroneCAN 协议栈** — 基于 Libcanard，支持 UAVCAN V0 协议
- **灵活的传输类型** — 支持广播、请求、响应三种传输模式
- **可配置内存池** — 支持自定义 CAN 收发缓冲区大小
- **回调机制** — 支持自定义接收回调和传输过滤回调
- **丰富的消息类型** — 预置 50+ 常用 UAVCAN 消息类型，覆盖：
  - GNSS 相关（Fix、RTCMStream、Auxiliary）
  - 电源管理（BatteryInfo、CircuitStatus、PrimaryPowerSupplyStatus）
  - 指示灯控制（LightsCommand、BeepCommand、RGB565）
  - 文件操作（Read、Write、GetDirectoryEntryInfo）
  - 参数管理（GetSet、ExecuteOpcode）
  - 节点管理（GetNodeInfo、RestartNode）
  - 调试功能（LogMessage、KeyValue）
  - 以及更多...

## 安装

### 通过 Arduino Library Manager

1. 打开 Arduino IDE → **Sketch** → **Include Library** → **Manage Libraries**
2. 搜索 `DroneCAN_ESP32`
3. 点击 Install

### 手动安装

将本仓库克隆到你的 Arduino libraries 目录：

```bash
cd ~/Arduino/libraries
git clone https://github.com/dukang1996/DroneCAN_ESP32.git
```

## 快速开始

### 硬件连接

```
ESP32          CAN Transceiver          DroneCAN Bus
--------       --------------          ------------
  TX  ───────►   TX                     CAN_H ────────► 其他节点
  RX  ◄───────   RX                     CAN_L ────────► 其他节点
          ┌───── VCC (3.3V)             120Ω 终端电阻（总线两端）
          └───── GND
```

> **注意**：请在总线两端各接一个 120Ω 终端电阻。

### 基础示例

```cpp
#include <DroneCAN_ESP32.h>

DroneCAN_ESP32 dronecan;

// 接收回调
void onTransferReceived(CanardInstance* ins, CanardRxTransfer* transfer) {
    Serial.print("Received data_type=");
    Serial.print(transfer->data_type_id);
    Serial.print(", length=");
    Serial.println(transfer->payload_len);
}

// 传输过滤回调
bool shouldAcceptTransfer(const CanardInstance* ins,
                         uint64_t* out_data_type_signature,
                         uint16_t data_type_id,
                         CanardTransferType transfer_type,
                         uint8_t source_node_id) {
    if (data_type_id == 123) {
        *out_data_type_signature = 0x1234567890ABCDEF;
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);

    // 设置回调
    dronecan.setReceiveCallback(onTransferReceived);
    dronecan.setAcceptCallback(shouldAcceptTransfer);

    // 初始化：1Mbps，节点ID 100
    if (!dronecan.begin(1000000, 100)) {
        Serial.println("DroneCAN init failed!");
        while(1);
    }

    Serial.println("DroneCAN started");
}

void loop() {
    dronecan.update();

    // 每秒广播一次
    static uint32_t last_send = 0;
    if (millis() - last_send > 1000) {
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        dronecan.broadcast(0x1234567890ABCDEF, 123,
                          CANARD_TRANSFER_PRIORITY_MEDIUM, data, sizeof(data));
        last_send = millis();
    }

    delay(10);
}
```

### 使用预置消息类型

```cpp
#include <DroneCAN_ESP32.h>
#include "dronecan_msgs/uavcan.equipment.power.BatteryInfo.h"

DroneCAN_ESP32 dronecan;

void setup() {
    dronecan.begin(1000000, 100);
}

void loop() {
    dronecan.update();

    static uint32_t last_send = 0;
    if (millis() - last_send > 1000) {
        // 构造电池信息消息
        BatteryInfo msg;
        msg.voltage = 12000;     // mV
        msg.current = 5000;      // mA
        msg.temperature = 300;   // K (开氏度)
        msg.state_of_charge_pct = 85;

        uint8_t buffer[64];
        uint16_t len = BatteryInfo_encode(&msg, buffer);

        dronecan.broadcast(BATTERYINFO_DATA_TYPE_SIGNATURE,
                           BATTERYINFO_DATA_TYPE_ID,
                           CANARD_TRANSFER_PRIORITY_HIGH,
                           buffer, len);
        last_send = millis();
    }
}
```

## API 参考

### 初始化

| 方法 | 描述 |
|------|------|
| `bool begin(uint32_t bitrate, uint8_t node_id, size_t pool_size = 4096)` | 初始化 CAN 总线和 DroneCAN 协议栈。返回 `true` 表示成功。 |

### 消息传输

| 方法 | 描述 |
|------|------|
| `int16_t broadcast(data_type_signature, data_type_id, priority, payload, payload_len)` | 发送广播消息 |
| `int16_t requestOrRespond(destination_node_id, data_type_signature, data_type_id, priority, kind, payload, payload_len)` | 发送请求或响应 |

### 回调设置

| 方法 | 描述 |
|------|------|
| `void setReceiveCallback(callback)` | 设置接收到数据时的回调函数 |
| `void setAcceptCallback(callback)` | 设置传输过滤回调，决定是否接受某个传输 |

### 周期调用

| 方法 | 描述 |
|------|------|
| `void update()` | 必须定期调用，处理 CAN 接收和发送队列 |

## 预置消息类型

位于 `src/dronecan_msgs/` 目录，常用消息类型：

| 消息类型 | 文件 | 数据类型 ID |
|----------|------|-------------|
| NodeStatus | `uavcan.protocol.NodeStatus.h` | 341 |
| GetNodeInfo | `uavcan.protocol.GetNodeInfo.h` | 384 |
| BatteryInfo | `uavcan.equipment.power.BatteryInfo.h` | 1514 |
| GlobalNavigationSolution | `uavcan.navigation.GlobalNavigationSolution.h` | 201 |
| LogMessage | `uavcan.protocol.debug.LogMessage.h` | 16383 |

完整列表请查看 [dronecan_msgs.h](src/dronecan_msgs/dronecan_msgs.h)。

## 依赖

- [ACAN_ESP32](https://github.com/seeed-studio/ACAN_ESP32) — ESP32 CAN 总线驱动
- [Libcanard](https://github.com/UAVCAN/libcanard) — UAVCAN 协议核心库（已内嵌）

## 许可证

- Libcanard 部分：MIT License（原作者 UAVCAN Team）
- ESP32 适配层：MIT License
