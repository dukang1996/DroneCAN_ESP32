#include "DroneCAN_ESP32.h"

// 静态成员变量定义
static bool (*user_should_accept_)(const CanardInstance*, uint64_t*, uint16_t, CanardTransferType, uint8_t) = nullptr;
static void (*user_on_reception_)(CanardInstance*, CanardRxTransfer*) = nullptr;

DroneCAN_ESP32::DroneCAN_ESP32()
    : memory_pool_(nullptr), pool_size_(4096), transfer_id_(0) {
}

DroneCAN_ESP32::~DroneCAN_ESP32() {
    if (memory_pool_ != nullptr) {
        free(memory_pool_);
    }
}

bool DroneCAN_ESP32::begin(uint32_t bitrate, uint8_t node_id, size_t pool_size) {
    // 初始化CAN控制器
    ACAN_ESP32_Settings settings(bitrate);
    settings.mRequestedCANMode = ACAN_ESP32_Settings::NormalMode;

    const uint32_t errorCode = ACAN_ESP32::can.begin(settings);
    if (errorCode != 0) {
        Serial.print("CAN initialization failed with error: 0x");
        Serial.println(errorCode, HEX);
        return false;
    }

    // 分配内存池
    pool_size_ = pool_size;
    memory_pool_ = malloc(pool_size_);
    if (memory_pool_ == nullptr) {
        Serial.println("Memory allocation failed");
        return false;
    }

    Serial.print("Pool size: ");
    Serial.println(pool_size_);

    // 设置回调函数
    auto should_accept = (user_should_accept_ != nullptr) ? user_should_accept_ : defaultShouldAccept;
    auto on_reception = (user_on_reception_ != nullptr) ? user_on_reception_ : defaultOnReception;
    // Serial.print("Using reception callback: ");
    // Serial.println((on_reception == defaultOnReception) ? "Default" : "User custom");

    // 初始化Libcanard实例
    canardInit(&canard_instance_,
               memory_pool_,
               pool_size_,
               on_reception,
               should_accept,
               this);  // 将this指针作为user_reference

    // 设置节点ID
    if (node_id != 0) {
        canardSetLocalNodeID(&canard_instance_, node_id);
    }

    Serial.println("DroneCAN_ESP32 initialized successfully");
    return true;
}

void DroneCAN_ESP32::update() {
    processCANFrames();  // 处理接收到的CAN帧
    sendQueuedFrames();  // 发送队列中的CAN帧

    // 清理过时的传输
    canardCleanupStaleTransfers(&canard_instance_, micros());
}

int16_t DroneCAN_ESP32::broadcast(uint64_t data_type_signature,
                                 uint16_t data_type_id,
                                 uint8_t priority,
                                 const void* payload,
                                 uint16_t payload_len) {

    return canardBroadcast(&canard_instance_,
                          data_type_signature,
                          data_type_id,
                          &transfer_id_,
                          priority,
                          payload,
                          payload_len
#if CANARD_ENABLE_DEADLINE
                          , micros() + 1000000  // 1秒超时
#endif
#if CANARD_MULTI_IFACE
                          , 1  // 使用接口1
#endif
#if CANARD_ENABLE_CANFD
                          , false  // 不使用CAN FD
#endif
                          );
}


int16_t DroneCAN_ESP32::requestOrRespond(uint8_t destination_node_id,
                                        uint64_t data_type_signature,
                                        uint8_t data_type_id,
                                        uint8_t priority,
                                        CanardRequestResponse kind,
                                        const void* payload,
                                        uint16_t payload_len) {

    // Serial.println("=== requestOrRespond Called ===");
    // Serial.print("Destination Node: ");
    // Serial.println(destination_node_id);
    // Serial.print("Data Type ID: 0x");
    // Serial.println(data_type_id, HEX);
    // Serial.print("Payload Length: ");
    // Serial.println(payload_len);

    int16_t result = canardRequestOrRespond(&canard_instance_,
                                 destination_node_id,
                                 data_type_signature,
                                 data_type_id,
                                 &transfer_id_,
                                 priority,
                                 kind,
                                 payload,
                                 payload_len
#if CANARD_ENABLE_DEADLINE
                                 , micros() + 1000000
#endif
#if CANARD_MULTI_IFACE
                                 , 1
#endif
#if CANARD_ENABLE_CANFD
                                 , false
#endif
                                 );

    // Serial.print("canardRequestOrRespond result: ");
    // Serial.println(result);  //result为正数时表示成功，表示多帧传输的数量，为负数时表示错误

    if (result < 0) {
        Serial.print("Error code: ");
        switch (-result) {
            case CANARD_ERROR_INVALID_ARGUMENT:
                Serial.println("CANARD_ERROR_INVALID_ARGUMENT - Invalid argument");
                break;
            case CANARD_ERROR_OUT_OF_MEMORY:
                Serial.println("CANARD_ERROR_OUT_OF_MEMORY - Out of memory");
                break;
            case CANARD_ERROR_INTERNAL:
                Serial.println("CANARD_ERROR_INTERNAL - Internal error");
                break;
            default:
                Serial.println("Unknown error");
                break;
        }
    }

    return result;
}

void DroneCAN_ESP32::setReceiveCallback(void (*callback)(CanardInstance* ins, CanardRxTransfer* transfer)) {
    user_on_reception_ = callback;
}

void DroneCAN_ESP32::setAcceptCallback(bool (*callback)(const CanardInstance* ins,
                                                      uint64_t* out_data_type_signature,
                                                      uint16_t data_type_id,
                                                      CanardTransferType transfer_type,
                                                      uint8_t source_node_id)) {
    user_should_accept_ = callback;
}

bool DroneCAN_ESP32::defaultShouldAccept(const CanardInstance* ins,
                                        uint64_t* out_data_type_signature,
                                        uint16_t data_type_id,
                                        CanardTransferType transfer_type,
                                        uint8_t source_node_id) {
    // 默认接受所有传输（生产环境中应根据需要实现过滤逻辑）
    *out_data_type_signature = 0;  // 需要根据实际数据类型设置正确的签名
    return true;
}

void DroneCAN_ESP32::defaultOnReception(CanardInstance* ins, CanardRxTransfer* transfer) {
    // 默认接收处理函数
    Serial.println("Default reception handler - override this with setReceiveCallback()");
}

void DroneCAN_ESP32::processCANFrames() {
    // 处理接收到的CAN帧
    CANMessage rx_frame;  // 使用ACAN_ESP32的CANMessage类型

    while (ACAN_ESP32::can.receive(rx_frame)) {
        // 打印接收到的CAN帧（调试用）
        // Serial.print("CAN Frame - ID: 0x");
        // Serial.print(rx_frame.id, HEX);
        // Serial.print(" Len: ");
        // Serial.print(rx_frame.len);
        // Serial.print(" Data: ");

        // for (int i = 0; i < rx_frame.len; i++) {
        //     if (rx_frame.data[i] < 0x10) Serial.print("0");
        //     Serial.print(rx_frame.data[i], HEX);
        //     Serial.print(" ");
        // }
        // Serial.println();

        CanardCANFrame canard_frame = {};
        if(rx_frame.ext) {
            canard_frame.id = rx_frame.id | CANARD_CAN_FRAME_EFF;  // 设置扩展ID标志
        }
        canard_frame.data_len = rx_frame.len;
        memcpy(canard_frame.data, rx_frame.data, rx_frame.len);
        canard_frame.iface_id = 0;  // 假设只有一个CAN接口

        //canardHandleRxFrame(&canard_instance_, &canard_frame, micros());
        //Serial.println("Calling canardHandleRxFrame...");
        int8_t result = canardHandleRxFrame(&canard_instance_, &canard_frame, micros());
        // Serial.print("canardHandleRxFrame result: ");
        // Serial.println(result);

        //Serial.println("=====================");
    }
}

void DroneCAN_ESP32::sendQueuedFrames() {
    CanardCANFrame* tx_frame = canardPeekTxQueue(&canard_instance_);
    while (tx_frame != nullptr) {
        CANMessage acan_frame;
        acan_frame.id = tx_frame->id & CANARD_CAN_EXT_ID_MASK;
        acan_frame.ext = true;
        acan_frame.rtr = false;
        acan_frame.len = tx_frame->data_len;
        memcpy(acan_frame.data, tx_frame->data, tx_frame->data_len);

        // 添加发送尝试计数和超时机制
        static uint8_t send_attempts = 0;
        const uint8_t max_attempts = 3;

        if (ACAN_ESP32::can.tryToSend(acan_frame)) {
            canardPopTxQueue(&canard_instance_);
            send_attempts = 0; // 重置尝试计数
            //Serial.print("✓ Frame sent, remaining in queue: ");
            //Serial.println(getTxQueueCount());
        } else {
            send_attempts++;
            if (send_attempts >= max_attempts) {
                // 多次发送失败，丢弃该帧避免阻塞
                //Serial.println("✗ Frame send failed multiple times, dropping");
                canardPopTxQueue(&canard_instance_);
                send_attempts = 0;
            }
            break; // 本次发送失败，等待下一周期
        }

        tx_frame = canardPeekTxQueue(&canard_instance_);
    }
}

// 添加队列统计方法
uint16_t DroneCAN_ESP32::getTxQueueCount() {
    uint16_t count = 0;
    CanardCANFrame* frame = canardPeekTxQueue(&canard_instance_);
    while (frame != nullptr) {
        count++;
        canardPopTxQueue(&canard_instance_); // 临时弹出计数
        frame = canardPeekTxQueue(&canard_instance_);
    }
    return count;
}
