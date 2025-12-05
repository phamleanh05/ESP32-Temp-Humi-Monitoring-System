# ESP32 Temperature-Humidity Monitoring System
# Video Demo Script

---

## 📋 **THÔNG TIN CHUNG**

**Dự án:** ESP32 Temperature-Humidity Monitoring System  
**Thời lượng video:** 8-12 phút (chia làm 2 phần)  
**Mục tiêu:** Trình bày toàn diện hệ thống giám sát nhiệt độ, độ ẩm với giao diện web

---

# 🎬 **PHẦN 1: DEMO PHẦN CỨNG (4-6 phút)**

## **1.1 Mở đầu và Giới thiệu tổng quan (30 giây)**

**[Góc quay: Wide shot toàn bộ setup]**

**Script:**
> "Xin chào, hôm nay tôi sẽ demo hệ thống ESP32 Temperature-Humidity Monitoring System. Đây là một hệ thống IoT hoàn chỉnh để giám sát nhiệt độ, độ ẩm với giao diện web và cảnh báo tự động."

**Action:**
- Pan camera từ trái qua phải để show toàn bộ setup
- Zoom vào board ESP32 làm center point

---

## **1.2 Giới thiệu các thành phần phần cứng (90 giây)**

**[Góc quay: Close-up từng component]**

**Script:**
> "Hệ thống bao gồm các thành phần chính sau:"

### **ESP32 Development Board**
**Action:** Point vào ESP32 board
> "Đây là ESP32, bộ vi xử lý chính với WiFi tích hợp, chạy hệ điều hành FreeRTOS đa tác vụ."

### **DHT11 Temperature-Humidity Sensor**  
**Action:** Point vào cảm biến DHT11
> "Cảm biến DHT11 đo nhiệt độ và độ ẩm với độ chính xác cao, giao tiếp qua giao thức digital."

### **Light Sensor (LDR)**
**Action:** Point vào cảm biến ánh sáng
> "Cảm biến ánh sáng LDR phát hiện mức độ ánh sáng môi trường, điều khiển LED tự động."

### **LCD Display I2C**
**Action:** Point vào màn hình LCD
> "Màn hình LCD 16x2 hiển thị thông tin real-time: nhiệt độ, độ ẩm, ánh sáng và IP address."

### **NeoPixel LED**
**Action:** Point vào NeoPixel
> "NeoPixel RGB LED cảnh báo bằng màu sắc và nhấp nháy khi nhiệt độ vượt ngưỡng."

### **Manual LED Controls**
**Action:** Point vào các LED thường
> "Hai LED thường có thể điều khiển thủ công và tự động qua web interface."

---

## **1.3 Demo kết nối phần cứng (60 giây)**

**[Góc quay: Close-up connections]**

**Script:**
> "Bây giờ tôi sẽ show các kết nối chính:"

**Action:**
- Trace dây từ DHT11 đến ESP32
- Show kết nối I2C của LCD (SDA, SCL)
- Point GPIO connections: GPIO45 (NeoPixel), GPIO48, GPIO2 (LEDs)
- Show power connections

> "Tất cả sensor được kết nối qua GPIO pins với pull-up resistors phù hợp."

---

## **1.4 Khởi động hệ thống (45 giây)**

**[Góc quay: Wide shot, zoom vào màn hình Serial Monitor]**

**Script:**
> "Bây giờ tôi sẽ khởi động hệ thống."

**Action:**
1. Cắm nguồn vào ESP32
2. Show Serial Monitor on computer
3. Point out các thông báo khởi động:
   - "Temperature/Humidity monitoring started"
   - "Light sensor task started"  
   - "LCD Display initialized"
   - "WiFi Configuration Server started"

> "Hệ thống khởi động thành công với 4 task chạy song song trên FreeRTOS."

---

## **1.5 Demo cảm biến hoạt động (90 giây)**

**[Góc quay: Split screen giữa LCD và Serial Monitor]**

### **Temperature & Humidity Test**
**Script:**
> "Đầu tiên test cảm biến nhiệt độ và độ ẩm."

**Action:**
- Thở vào cảm biến DHT11 để tăng độ ẩm
- Dùng tay ấm gần cảm biến để tăng nhiệt độ
- Show số liệu thay đổi trên LCD và Serial Monitor

> "Các giá trị được cập nhật mỗi 2 giây và hiển thị real-time."

### **Light Sensor Test**
**Script:**
> "Tiếp theo test cảm biến ánh sáng."

**Action:**
- Che cảm biến ánh sáng bằng tay
- Show LED GPIO2 tự động bật khi tối
- Bỏ tay ra, LED tự động tắt khi sáng
- Point out thông báo trên Serial: "Dark detected - LED ON"

> "LED tự động bật khi ánh sáng dưới 500, tắt khi trên ngưỡng này."

---

## **1.6 Demo WiFi và NeoPixel (60 giây)**

**[Góc quay: Focus vào NeoPixel và LCD]**

**Script:**
> "Hệ thống tự động kết nối WiFi và hiển thị IP address."

**Action:**
- Point vào IP address trên LCD
- Show NeoPixel sáng màu xanh (normal mode)
- Heat cảm biến DHT11 bằng lighter/hair dryer từ xa
- Show NeoPixel chuyển sang nhấp nháy đỏ khi nhiệt độ > 30°C

> "NeoPixel nhấp nháy đỏ cảnh báo khi nhiệt độ vượt ngưỡng, tự động về màu bình thường khi hạ nhiệt."

---

# 🖥️ **PHẦN 2: DEMO PHẦN MỀM (4-6 phút)**

## **2.1 Giới thiệu Web Interface (30 giây)**

**[Screen recording: Web browser]**

**Script:**
> "Bây giờ tôi sẽ demo giao diện web của hệ thống. Truy cập qua địa chỉ IP hiển thị trên LCD."

**Action:**
- Open web browser
- Navigate to ESP32 IP address
- Show dashboard loading

---

## **2.2 Dashboard Overview (45 giây)**

**[Screen recording: Full dashboard view]**

**Script:**
> "Đây là dashboard chính với layout responsive, hiển thị tất cả thông tin sensor real-time."

**Action:**
- Scroll through dashboard
- Point out các sections:
  - WiFi connection status
  - Temperature card với threshold setting
  - Humidity display  
  - Light level với auto LED status
  - LED Controls section

> "Interface cập nhật tự động qua WebSocket, không cần refresh trang."

---

## **2.3 Temperature Monitoring & Alerts (90 giây)**

**[Screen recording: Temperature section + Network tab]**

**Script:**
> "Phần quan trọng nhất là giám sát nhiệt độ với cảnh báo tự động."

**Action:**

### **Normal Temperature**
- Show temperature card với background bình thường
- Open browser Developer Tools → Network tab
- Show WebSocket messages với temp_alert: false

### **Temperature Threshold Configuration**
- Change threshold từ 30 xuống 25°C trong temperature card
- Click Save button
- Show confirmation message

### **High Temperature Alert**
- Heat DHT11 sensor (off-camera)
- Show temperature value tăng lên
- **Demo background color change**: Temperature card chuyển từ màu trắng sang màu đỏ nhạt
- Show temp_alert: true trong WebSocket messages

> "Khi nhiệt độ vượt ngưỡng, card đổi background màu đỏ và NeoPixel nhấp nháy cảnh báo."

---

## **2.4 LED Controls Demo (75 giây)**

**[Screen recording: LED Controls section]**

**Script:**
> "Hệ thống có 3 loại LED với chức năng khác nhau."

**Action:**

### **Manual LED Control**
- Toggle Manual LED (GPIO 48) ON/OFF
- Show button color change và status update
- Cross-reference với hardware LED thực tế

### **NeoPixel Color Configuration**  
- Open color picker trong NeoPixel section
- Change color từ green sang blue
- Click Save Color
- Show NeoPixel hardware đổi màu tương ứng
- Explain: "Manual control chỉ hoạt động khi không có cảnh báo nhiệt độ"

### **Auto LED (Light Sensor)**
- Show Auto LED status (controlled by light sensor)
- Che light sensor để trigger auto LED
- Show status update trên dashboard

> "Auto LED được điều khiển hoàn toàn bởi light sensor, không thể control thủ công."

---

## **2.5 Real-time Updates Demo (60 giây)**

**[Screen recording: Multi-tab demo]**

**Script:**
> "Hệ thống hỗ trợ multiple clients và cập nhật real-time."

**Action:**
- Open second browser tab với cùng dashboard
- Make changes trên tab 1 (change LED, color, threshold)
- Show changes reflect ngay lập tức trên tab 2
- Open Developer Tools → Console
- Show WebSocket messages flowing real-time

> "Mọi thay đổi được đồng bộ real-time qua WebSocket giữa tất cả clients."

---

## **2.6 REST API Demo (45 giây)**

**[Screen recording: Browser + API testing tool]**

**Script:**
> "Hệ thống cung cấp REST API để integration với external systems."

**Action:**
- Open new tab, navigate to: `/sensors`
- Show JSON response với tất cả sensor data
- Navigate to `/status` → show WiFi status
- Navigate to `/leds` → show LED status
- Navigate to `/alert` → show alert configuration

> "REST API cho phép external systems đọc data và integrate với hệ thống."

---

## **2.7 WiFi Configuration Demo (30 giây)**

**[Screen recording: WiFi config page]**

**Script:**
> "Hệ thống có WiFi configuration interface để setup mạng mới."

**Action:**
- Navigate to `/wifi-config`
- Show network scan results
- Show configuration form (không thực sự connect)
- Back to main dashboard

> "User có thể reconfigure WiFi mà không cần access physical device."

---

## **2.8 Tổng kết và Features (45 giây)**

**[Screen recording: Dashboard overview]**

**Script:**
> "Tổng kết các tính năng chính của hệ thống:"

**Action:** Scroll through dashboard while listing features

> "✅ Real-time monitoring nhiệt độ, độ ẩm, ánh sáng  
> ✅ Cảnh báo tự động với NeoPixel và background color  
> ✅ LED controls thủ công và tự động  
> ✅ Web interface responsive với WebSocket updates  
> ✅ REST API cho external integration  
> ✅ WiFi configuration không cần physical access  
> ✅ Persistent settings storage  
> ✅ Multi-client support"

> "Đây là một hệ thống IoT hoàn chỉnh, ready for deployment trong smart home hoặc industrial monitoring."

---

# 📝 **GHI CHÚ KỸ THUẬT CHO VIDEO**

## **Equipment Setup:**
- **Camera chính**: Quay overhead cho hardware demo
- **Screen recorder**: OBS Studio hoặc tương tự cho software demo  
- **Audio**: External microphone cho voice-over
- **Lighting**: Đủ sáng để thấy rõ components và LCD display

## **Pre-demo Checklist:**
- [ ] ESP32 đã flash firmware mới nhất
- [ ] WiFi credentials đã configure sẵn
- [ ] Browser bookmarks prepared cho các URLs
- [ ] DHT11 sensor positioned tốt cho camera
- [ ] NeoPixel visible trong frame
- [ ] LCD display readable từ camera angle
- [ ] Serial Monitor window setup với appropriate baud rate

## **Technical Notes:**
- Heat source để test temperature alert (hair dryer, lighter, tay)
- Dark cloth để test light sensor
- Multiple browser tabs prepared
- Developer tools setup sẵn
- Network monitoring tools ready

## **Timing Guidelines:**
- **Total video**: 8-12 minutes
- **Hardware demo**: 4-6 minutes (practical, hands-on)
- **Software demo**: 4-6 minutes (screen recording)
- **Transition time**: ~30 seconds giữa 2 phần

## **Post-production:**
- Add text overlays cho technical specifications
- Zoom vào các UI elements quan trọng
- Add arrows pointing đến specific components
- Background music (optional, low volume)
- Color correction cho screen recordings

---

# 🎯 **POINTS TO EMPHASIZE**

1. **Real-time capabilities**: WebSocket updates, sensor readings
2. **Professional UI**: Responsive design, modern interface  
3. **Automatic alerts**: Temperature threshold, color changes
4. **Multi-functionality**: Manual + automatic controls
5. **IoT integration**: REST API, WiFi configuration
6. **Robust architecture**: FreeRTOS multi-tasking, error handling

---

**Kết thúc script. Chúc bạn quay video demo thành công! 🎬✨**