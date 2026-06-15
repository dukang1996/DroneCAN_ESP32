#include <DroneCAN_ESP32.h>

DroneCAN_ESP32 dronecan;

// 自定义接收回调函数
void onTransferReceived(CanardInstance* ins, CanardRxTransfer* transfer) {
    Serial.print("Received transfer: data_type=");
    Serial.print(transfer->data_type_id);
    Serial.print(", length=");
    Serial.println(transfer->payload_len);
    
    // 处理接收到的数据...
}

// 自定义接受回调函数
bool shouldAcceptTransfer(const CanardInstance* ins, 
                         uint64_t* out_data_type_signature,
                         uint16_t data_type_id,
                         CanardTransferType transfer_type,
                         uint8_t source_node_id) {
    
    // 根据数据类型ID决定是否接受该传输
    if (data_type_id == 123) {  // 示例数据类型ID
        *out_data_type_signature = 0x1234567890ABCDEF;  // 设置正确的数据类型签名
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    
    // 设置回调函数
    dronecan.setReceiveCallback(onTransferReceived);
    dronecan.setAcceptCallback(shouldAcceptTransfer);
    
    // 初始化DroneCAN（1Mbps，节点ID 100）
    if (!dronecan.begin(1000000, 100)) {
        Serial.println("Failed to initialize DroneCAN!");
        while(1);
    }
    
    Serial.println("DroneCAN example started");
}

void loop() {
    dronecan.update();  // 必须定期调用以处理CAN通信
    
    // 示例：每秒发送一次广播消息
    static uint32_t last_send = 0;
    if (millis() - last_send > 1000) {
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        dronecan.broadcast(0x1234567890ABCDEF, 123, 
                          CANARD_TRANSFER_PRIORITY_MEDIUM, data, sizeof(data));
        last_send = millis();
    }
    
    delay(10);
}