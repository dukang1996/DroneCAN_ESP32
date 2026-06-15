#ifndef DRONECAN_ESP32_H
#define DRONECAN_ESP32_H

#include <Arduino.h>
#include <ACAN_ESP32.h>  // ESP32 CAN库

// 包含Libcanard核心文件
#include "canard.h"

#include "dronecan_msgs/dronecan_msgs.h" // DroneCAN消息定义

class DroneCAN_ESP32 {
public:
    DroneCAN_ESP32();
    ~DroneCAN_ESP32();

    // 初始化DroneCAN
    bool begin(uint32_t bitrate, uint8_t node_id, size_t pool_size = 4096);

    // 处理CAN接收和发送
    void update();

    // 发送广播消息
    int16_t broadcast(uint64_t data_type_signature,
                      uint16_t data_type_id,
                      uint8_t priority,
                      const void* payload,
                      uint16_t payload_len);

    // 发送请求或响应
    int16_t requestOrRespond(uint8_t destination_node_id,
                            uint64_t data_type_signature,
                            uint8_t data_type_id,
                            uint8_t priority,
                            CanardRequestResponse kind,
                            const void* payload,
                            uint16_t payload_len);

    // 设置接收回调
    void setReceiveCallback(void (*callback)(CanardInstance* ins, CanardRxTransfer* transfer));

    // 设置接受传输回调
    void setAcceptCallback(bool (*callback)(const CanardInstance* ins,
                                          uint64_t* out_data_type_signature,
                                          uint16_t data_type_id,
                                          CanardTransferType transfer_type,
                                          uint8_t source_node_id));

private:
    CanardInstance canard_instance_;
    void* memory_pool_;
    size_t pool_size_;
    uint8_t transfer_id_;

    static bool defaultShouldAccept(const CanardInstance* ins,
                                   uint64_t* out_data_type_signature,
                                   uint16_t data_type_id,
                                   CanardTransferType transfer_type,
                                   uint8_t source_node_id);

    static void defaultOnReception(CanardInstance* ins, CanardRxTransfer* transfer);

    void processCANFrames();
    void sendQueuedFrames();
    uint16_t getTxQueueCount();
};

#endif
