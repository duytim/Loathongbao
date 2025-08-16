# 🎵 Loa Thông Báo Chuyển Khoản Tự Động

Hệ thống loa thông báo chuyển khoản tự động với nhiều tính năng cao cấp cho ESP32.

## ✨ Tính Năng Chính

### 🔔 Thông Báo Tự Động
- Tự động phát âm thanh khi có chuyển khoản mới
- Chuyển đổi số tiền thành chữ tiếng Việt
- Âm thanh thông báo + giọng nói tự nhiên

### 🌐 Web Interface
- Giao diện web đẹp mắt để cấu hình
- Cài đặt WiFi, API token, âm lượng
- Xem lịch sử giao dịch real-time
- Test âm thanh trực tiếp từ web

### 📱 Cấu Hình Qua WiFi
- Chế độ Access Point tự động khi mất WiFi
- Cấu hình WiFi mới qua web interface
- Lưu trữ cài đặt vào bộ nhớ flash

### 💾 Lưu Trữ Lịch Sử
- Lưu trữ 50 giao dịch gần nhất
- Thông tin chi tiết: ngày giờ, số tiền, người gửi, ghi chú
- Lưu vào SPIFFS để không mất khi khởi động lại

### 🎛️ Điều Khiển Vật Lý
- Nút bấm để bật/tắt LED
- LED indicator khi phát âm thanh
- Điều khiển qua Serial Monitor

### ⚙️ Cài Đặt Nâng Cao
- Âm lượng có thể điều chỉnh (0-21)
- Tin nhắn tùy chỉnh
- Bật/tắt LED
- Tự động khởi động lại sau 24h
- Timeout kết nối WiFi

## 🚀 Các Tính Năng Cao Cấp

### 1. **Web Dashboard**
```
http://[ESP32_IP]
```
- Giao diện responsive, đẹp mắt
- Cập nhật real-time
- Thống kê giao dịch
- Cài đặt trực quan

### 2. **Lệnh Serial Monitor**
```
cmd:restart    - Khởi động lại hệ thống
cmd:ap         - Chuyển sang chế độ AP
cmd:wifi       - Kết nối lại WiFi
cmd:volume     - Xem âm lượng hiện tại
cmd:history    - Xem lịch sử giao dịch
```

### 3. **Chế Độ Access Point**
- Tự động tạo WiFi hotspot khi không kết nối được
- SSID: `Loa_Thong_Bao`
- Password: `12345678`
- Timeout 5 phút

### 4. **Lưu Trữ Thông Minh**
- Sử dụng Preferences để lưu cài đặt
- SPIFFS để lưu lịch sử giao dịch
- Không mất dữ liệu khi khởi động lại

### 5. **Xử Lý Lỗi Nâng Cao**
- Kiểm tra kết nối WiFi liên tục
- Tự động kết nối lại khi mất kết nối
- Xử lý lỗi API và JSON parsing
- Timeout cho các thao tác mạng

### 6. **Tối Ưu Hiệu Suất**
- Delay nhỏ trong loop để tránh watchdog
- Kiểm tra trạng thái trước khi phát âm thanh
- Tối ưu bộ nhớ với StaticJsonDocument
- Xử lý non-blocking cho web server

## 📋 Cài Đặt

### Phần Cứng
- ESP32
- MAX98357A I2S Amplifier
- Loa 8Ω
- Nút bấm (tùy chọn)
- LED (tùy chọn)

### Kết Nối
```
ESP32 Pin 25 -> MAX98357A DIN
ESP32 Pin 27 -> MAX98357A BCLK  
ESP32 Pin 26 -> MAX98357A LRC
ESP32 Pin 2  -> LED (tùy chọn)
ESP32 Pin 0  -> Nút bấm (tùy chọn)
```

### Thư Viện Cần Thiết
```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Audio.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>
```

## 🔧 Cấu Hình

### 1. Thông Tin WiFi và API
```cpp
const char* DEFAULT_SSID = "Your_WiFi_SSID";
const char* DEFAULT_PASSWORD = "Your_WiFi_Password";
const char* DEFAULT_API_TOKEN = "Your_API_Token";
const char* DEFAULT_ACCOUNT = "Your_Account_Number";
```

### 2. Cài Đặt Qua Web
1. Kết nối WiFi với ESP32
2. Mở trình duyệt: `http://[ESP32_IP]`
3. Cấu hình WiFi, API, âm lượng
4. Lưu cài đặt

### 3. Cài Đặt Qua Serial
```
cmd:ap         // Chuyển sang chế độ AP
// Kết nối WiFi "Loa_Thong_Bao" với password "12345678"
// Mở http://192.168.4.1 để cấu hình
```

## 📊 API Endpoints

### Web Interface
- `GET /` - Trang chủ
- `POST /wifi` - Cài đặt WiFi
- `POST /api` - Cài đặt API
- `POST /settings` - Cài đặt hệ thống
- `POST /test` - Test âm thanh
- `GET /history` - Lấy lịch sử giao dịch
- `POST /restart` - Khởi động lại

## 🎯 Tính Năng Đặc Biệt

### 1. **Tin Nhắn Tùy Chỉnh**
- Có thể thay đổi nội dung thông báo
- Hỗ trợ tiếng Việt hoàn toàn
- Kết hợp tin nhắn tùy chỉnh + số tiền

### 2. **Thông Minh**
- Tự động phát hiện giao dịch mới
- Tránh phát chồng lấp âm thanh
- Xử lý số tiền âm
- Lưu trữ thông tin người gửi

### 3. **Bảo Mật**
- Lưu trữ an toàn thông tin WiFi
- API token được bảo vệ
- Timeout cho các thao tác

### 4. **Dễ Sử Dụng**
- Giao diện web thân thiện
- Hướng dẫn rõ ràng
- Tự động phục hồi khi gặp lỗi

## 🔮 Tính Năng Tương Lai

- [ ] Kết nối Bluetooth
- [ ] App mobile
- [ ] Thông báo qua Telegram
- [ ] Báo cáo thống kê chi tiết
- [ ] Hỗ trợ nhiều tài khoản
- [ ] Lọc giao dịch theo mức tiền
- [ ] Lịch sử dài hạn
- [ ] Backup/restore cài đặt

## 📞 Hỗ Trợ

Nếu gặp vấn đề, hãy kiểm tra:
1. Kết nối phần cứng
2. Thông tin WiFi và API
3. Serial Monitor để debug
4. Web interface để cấu hình

---

**Phiên bản:** 2.0  
**Tác giả:** AI Assistant  
**Ngày cập nhật:** 2024 