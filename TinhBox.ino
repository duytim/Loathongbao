#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <Audio.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <time.h>

// Cấu hình I2S cho MAX98357A
#define I2S_DOUT 25 // Data In
#define I2S_BCLK 27 // Bit Clock
#define I2S_LRC 26  // Left/Right Clock (Word Select)

// Cấu hình LED và nút bấm
#define LED_PIN 2
#define BUTTON_PIN 0
#define LED_COUNT 8
#define LED_DATA_PIN 13

// Cấu hình hệ thống
#define WIFI_TIMEOUT 10000      // Timeout kết nối WiFi (ms)
#define API_CHECK_INTERVAL 4000 // Thời gian kiểm tra API (ms)
#define AUDIO_VOLUME 18         // Âm lượng loa
#define JSON_DOC_SIZE 2048      // Kích thước JSON document
#define MAX_HISTORY 50          // Số lượng giao dịch lưu trữ tối đa
#define AP_TIMEOUT 300000       // Timeout Access Point (5 phút)

// Cấu hình mặc định
const char* DEFAULT_SSID = "Nha Nghi 63 -2.4G";
const char* DEFAULT_PASSWORD = "nhanghi63";
const char* DEFAULT_API_TOKEN = "ESAQZACYCTPE9RTZ2YVB7D9ULVS8O5LEQNCWIK3FRKYTOJ11DUPNAF48HN5IMY2B";
const char* DEFAULT_ACCOUNT = "VQRQACNKJ7278";

// Link âm thanh thông báo
const char* notificationSound = "https://tiengdong.com/wp-content/uploads/Tieng-tinh-tinh-www_tiengdong_com.mp3";

// Đối tượng hệ thống
Audio audio;
WebServer server(80);
Preferences preferences;

// Biến toàn cục
unsigned long lastApiCheck = 0;
String lastTransactionDate = "";
long lastAmountIn = 0;
bool isSpeaking = false;
bool wifiConnected = false;
bool isAPMode = false;
unsigned long apStartTime = 0;
int ledBrightness = 128;
bool ledEnabled = true;
bool autoRestart = true;
int volumeLevel = 18;
String customMessage = "";

// Cấu trúc lưu trữ giao dịch
struct Transaction {
  String date;
  long amount;
  String sender;
  String note;
  unsigned long timestamp;
};

Transaction transactionHistory[MAX_HISTORY];
int historyIndex = 0;
int totalTransactions = 0;

// Mảng từ vựng tiếng Việt
const char* const digits[] = {"không", "một", "hai", "ba", "bốn", "năm", "sáu", "bảy", "tám", "chín"};
const char* const units[] = {"", "nghìn", "triệu", "tỷ"};

// Hàm chuyển số thành chữ tiếng Việt (tối ưu)
String numberToVietnamese(long number) {
  if (number == 0) return "không";
  if (number < 0) return "âm " + numberToVietnamese(-number);

  String result = "";
  int unitIndex = 0;

  while (number > 0) {
    int chunk = number % 1000;
    if (chunk > 0) {
      String chunkStr = "";
      int hundreds = chunk / 100;
      int tens = (chunk % 100) / 10;
      int ones = chunk % 10;

      // Xử lý hàng trăm
      if (hundreds > 0) {
        chunkStr += digits[hundreds];
        chunkStr += " trăm";
        if (tens > 0 || ones > 0) chunkStr += " ";
      }

      // Xử lý hàng chục và đơn vị
      if (tens == 0) {
        if (ones > 0 && hundreds > 0) chunkStr += "lẻ ";
        if (ones > 0) chunkStr += digits[ones];
      } else if (tens == 1) {
        chunkStr += "mười";
        if (ones > 0) {
          chunkStr += " ";
          chunkStr += digits[ones];
        }
      } else {
        chunkStr += digits[tens];
        chunkStr += " mươi";
        if (ones > 0) {
          if (ones == 1) chunkStr += " mốt";
          else if (ones == 5) chunkStr += " lăm";
          else {
            chunkStr += " ";
            chunkStr += digits[ones];
          }
        }
      }

      // Thêm đơn vị
      if (chunkStr.length() > 0) {
        if (result.length() > 0) {
          result = chunkStr + " " + units[unitIndex] + " " + result;
        } else {
          result = chunkStr + " " + units[unitIndex];
        }
      }
    }
    number /= 1000;
    unitIndex++;
  }

  return result;
}

// Hàm lưu cài đặt vào bộ nhớ
void saveSettings() {
  preferences.putString("ssid", WiFi.SSID());
  preferences.putString("password", WiFi.psk());
  preferences.putString("api_token", preferences.getString("api_token", DEFAULT_API_TOKEN));
  preferences.putString("account", preferences.getString("account", DEFAULT_ACCOUNT));
  preferences.putInt("volume", volumeLevel);
  preferences.putBool("led_enabled", ledEnabled);
  preferences.putBool("auto_restart", autoRestart);
  preferences.putString("custom_message", customMessage);
  Serial.println("Đã lưu cài đặt");
}

// Hàm đọc cài đặt từ bộ nhớ
void loadSettings() {
  String ssid = preferences.getString("ssid", DEFAULT_SSID);
  String password = preferences.getString("password", DEFAULT_PASSWORD);
  String api_token = preferences.getString("api_token", DEFAULT_API_TOKEN);
  String account = preferences.getString("account", DEFAULT_ACCOUNT);
  
  volumeLevel = preferences.getInt("volume", 18);
  ledEnabled = preferences.getBool("led_enabled", true);
  autoRestart = preferences.getBool("auto_restart", true);
  customMessage = preferences.getString("custom_message", "");
  
  Serial.println("Đã tải cài đặt");
}

// Hàm thêm giao dịch vào lịch sử
void addToHistory(String date, long amount, String sender = "", String note = "") {
  transactionHistory[historyIndex].date = date;
  transactionHistory[historyIndex].amount = amount;
  transactionHistory[historyIndex].sender = sender;
  transactionHistory[historyIndex].note = note;
  transactionHistory[historyIndex].timestamp = millis();
  
  historyIndex = (historyIndex + 1) % MAX_HISTORY;
  if (totalTransactions < MAX_HISTORY) totalTransactions++;
  
  // Lưu vào SPIFFS
  saveHistoryToFile();
}

// Hàm lưu lịch sử vào file
void saveHistoryToFile() {
  File file = SPIFFS.open("/history.json", "w");
  if (!file) {
    Serial.println("Lỗi mở file lịch sử");
    return;
  }
  
  StaticJsonDocument<4096> doc;
  JsonArray history = doc.createNestedArray("history");
  
  for (int i = 0; i < totalTransactions; i++) {
    JsonObject transaction = history.createNestedObject();
    transaction["date"] = transactionHistory[i].date;
    transaction["amount"] = transactionHistory[i].amount;
    transaction["sender"] = transactionHistory[i].sender;
    transaction["note"] = transactionHistory[i].note;
    transaction["timestamp"] = transactionHistory[i].timestamp;
  }
  
  serializeJson(doc, file);
  file.close();
}

// Hàm đọc lịch sử từ file
void loadHistoryFromFile() {
  if (!SPIFFS.exists("/history.json")) return;
  
  File file = SPIFFS.open("/history.json", "r");
  if (!file) return;
  
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.println("Lỗi đọc file lịch sử");
    return;
  }
  
  JsonArray history = doc["history"];
  totalTransactions = min(history.size(), MAX_HISTORY);
  
  for (int i = 0; i < totalTransactions; i++) {
    JsonObject transaction = history[i];
    transactionHistory[i].date = transaction["date"] | "";
    transactionHistory[i].amount = transaction["amount"] | 0;
    transactionHistory[i].sender = transaction["sender"] | "";
    transactionHistory[i].note = transaction["note"] | "";
    transactionHistory[i].timestamp = transaction["timestamp"] | 0;
  }
  
  historyIndex = totalTransactions % MAX_HISTORY;
}

// Hàm lưu cài đặt vào bộ nhớ
void saveSettings() {
  preferences.putString("ssid", WiFi.SSID());
  preferences.putString("password", WiFi.psk());
  preferences.putString("api_token", preferences.getString("api_token", DEFAULT_API_TOKEN));
  preferences.putString("account", preferences.getString("account", DEFAULT_ACCOUNT));
  preferences.putInt("volume", volumeLevel);
  preferences.putBool("led_enabled", ledEnabled);
  preferences.putBool("auto_restart", autoRestart);
  preferences.putString("custom_message", customMessage);
  Serial.println("Đã lưu cài đặt");
}

// Hàm đọc cài đặt từ bộ nhớ
void loadSettings() {
  String ssid = preferences.getString("ssid", DEFAULT_SSID);
  String password = preferences.getString("password", DEFAULT_PASSWORD);
  String api_token = preferences.getString("api_token", DEFAULT_API_TOKEN);
  String account = preferences.getString("account", DEFAULT_ACCOUNT);
  
  volumeLevel = preferences.getInt("volume", 18);
  ledEnabled = preferences.getBool("led_enabled", true);
  autoRestart = preferences.getBool("auto_restart", true);
  customMessage = preferences.getString("custom_message", "");
  
  Serial.println("Đã tải cài đặt");
}

// Hàm thêm giao dịch vào lịch sử
void addToHistory(String date, long amount, String sender = "", String note = "") {
  transactionHistory[historyIndex].date = date;
  transactionHistory[historyIndex].amount = amount;
  transactionHistory[historyIndex].sender = sender;
  transactionHistory[historyIndex].note = note;
  transactionHistory[historyIndex].timestamp = millis();
  
  historyIndex = (historyIndex + 1) % MAX_HISTORY;
  if (totalTransactions < MAX_HISTORY) totalTransactions++;
  
  // Lưu vào SPIFFS
  saveHistoryToFile();
}

// Hàm lưu lịch sử vào file
void saveHistoryToFile() {
  File file = SPIFFS.open("/history.json", "w");
  if (!file) {
    Serial.println("Lỗi mở file lịch sử");
    return;
  }
  
  StaticJsonDocument<4096> doc;
  JsonArray history = doc.createNestedArray("history");
  
  for (int i = 0; i < totalTransactions; i++) {
    JsonObject transaction = history.createNestedObject();
    transaction["date"] = transactionHistory[i].date;
    transaction["amount"] = transactionHistory[i].amount;
    transaction["sender"] = transactionHistory[i].sender;
    transaction["note"] = transactionHistory[i].note;
    transaction["timestamp"] = transactionHistory[i].timestamp;
  }
  
  serializeJson(doc, file);
  file.close();
}

// Hàm đọc lịch sử từ file
void loadHistoryFromFile() {
  if (!SPIFFS.exists("/history.json")) return;
  
  File file = SPIFFS.open("/history.json", "r");
  if (!file) return;
  
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.println("Lỗi đọc file lịch sử");
    return;
  }
  
  JsonArray history = doc["history"];
  totalTransactions = min(history.size(), MAX_HISTORY);
  
  for (int i = 0; i < totalTransactions; i++) {
    JsonObject transaction = history[i];
    transactionHistory[i].date = transaction["date"] | "";
    transactionHistory[i].amount = transaction["amount"] | 0;
    transactionHistory[i].sender = transaction["sender"] | "";
    transactionHistory[i].note = transaction["note"] | "";
    transactionHistory[i].timestamp = transaction["timestamp"] | 0;
  }
  
  historyIndex = totalTransactions % MAX_HISTORY;
}

// Hàm tạo Access Point để cấu hình
void startAPMode() {
  Serial.println("Khởi động chế độ Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Loa_Thong_Bao", "12345678");
  
  isAPMode = true;
  apStartTime = millis();
  
  Serial.println("AP IP: " + WiFi.softAPIP().toString());
  Serial.println("SSID: Loa_Thong_Bao");
  Serial.println("Password: 12345678");
}

// Hàm tạo giao diện web
void setupWebServer() {
  // Trang chủ
  server.on("/", HTTP_GET, []() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Loa Thông Báo Chuyển Khoản</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .header { text-align: center; color: #333; margin-bottom: 30px; }
        .section { margin-bottom: 30px; padding: 20px; border: 1px solid #ddd; border-radius: 5px; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input, textarea, select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }
        button { background: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }
        button:hover { background: #0056b3; }
        .status { padding: 10px; border-radius: 5px; margin-bottom: 15px; }
        .success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        .history-item { padding: 10px; border-bottom: 1px solid #eee; }
        .amount { font-weight: bold; color: #28a745; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🎵 Loa Thông Báo Chuyển Khoản</h1>
            <p>Hệ thống tự động thông báo khi có chuyển khoản</p>
        </div>
        
        <div class="section">
            <h2>⚙️ Cài Đặt WiFi</h2>
            <form action="/wifi" method="POST">
                <div class="form-group">
                    <label>SSID:</label>
                    <input type="text" name="ssid" value=")" + WiFi.SSID() + R"(" required>
                </div>
                <div class="form-group">
                    <label>Password:</label>
                    <input type="password" name="password" required>
                </div>
                <button type="submit">Lưu WiFi</button>
            </form>
        </div>
        
        <div class="section">
            <h2>🔧 Cài Đặt API</h2>
            <form action="/api" method="POST">
                <div class="form-group">
                    <label>API Token:</label>
                    <input type="text" name="api_token" value=")" + preferences.getString("api_token", DEFAULT_API_TOKEN) + R"(" required>
                </div>
                <div class="form-group">
                    <label>Số Tài Khoản:</label>
                    <input type="text" name="account" value=")" + preferences.getString("account", DEFAULT_ACCOUNT) + R"(" required>
                </div>
                <button type="submit">Lưu API</button>
            </form>
        </div>
        
        <div class="section">
            <h2>🎛️ Cài Đặt Hệ Thống</h2>
            <form action="/settings" method="POST">
                <div class="form-group">
                    <label>Âm Lượng:</label>
                    <input type="range" name="volume" min="0" max="21" value=")" + String(volumeLevel) + R"(">
                    <span id="volumeValue">)" + String(volumeLevel) + R"(</span>
                </div>
                <div class="form-group">
                    <label>Tin Nhắn Tùy Chỉnh:</label>
                    <textarea name="custom_message" rows="3">)" + customMessage + R"(</textarea>
                </div>
                <div class="form-group">
                    <label>
                        <input type="checkbox" name="led_enabled" )" + (ledEnabled ? "checked" : "") + R"("> Bật LED
                    </label>
                </div>
                <div class="form-group">
                    <label>
                        <input type="checkbox" name="auto_restart" )" + (autoRestart ? "checked" : "") + R"("> Tự động khởi động lại
                    </label>
                </div>
                <button type="submit">Lưu Cài Đặt</button>
            </form>
        </div>
        
        <div class="section">
            <h2>🧪 Test Hệ Thống</h2>
            <form action="/test" method="POST">
                <div class="form-group">
                    <label>Số Tiền Test:</label>
                    <input type="number" name="amount" placeholder="Nhập số tiền để test">
                </div>
                <button type="submit">Phát Thông Báo</button>
            </form>
        </div>
        
        <div class="section">
            <h2>📊 Lịch Sử Giao Dịch</h2>
            <div id="history">
                <p>Đang tải...</p>
            </div>
        </div>
        
        <div class="section">
            <h2>📈 Thống Kê</h2>
            <p><strong>Tổng giao dịch:</strong> <span id="totalTx">)" + String(totalTransactions) + R"(</span></p>
            <p><strong>Trạng thái WiFi:</strong> <span id="wifiStatus">)" + (wifiConnected ? "Đã kết nối" : "Chưa kết nối") + R"(</span></p>
            <p><strong>IP:</strong> <span id="ipAddress">)" + WiFi.localIP().toString() + R"(</span></p>
        </div>
    </div>
    
    <script>
        // Cập nhật giá trị âm lượng
        document.querySelector('input[name="volume"]').addEventListener('input', function() {
            document.getElementById('volumeValue').textContent = this.value;
        });
        
        // Tải lịch sử giao dịch
        function loadHistory() {
            fetch('/history')
                .then(response => response.json())
                .then(data => {
                    const historyDiv = document.getElementById('history');
                    if (data.history.length === 0) {
                        historyDiv.innerHTML = '<p>Chưa có giao dịch nào</p>';
                        return;
                    }
                    
                    let html = '';
                    data.history.forEach(tx => {
                        html += '<div class="history-item">';
                        html += '<div class="amount">' + tx.amount.toLocaleString() + ' VNĐ</div>';
                        html += '<div>Ngày: ' + tx.date + '</div>';
                        if (tx.sender) html += '<div>Người gửi: ' + tx.sender + '</div>';
                        if (tx.note) html += '<div>Ghi chú: ' + tx.note + '</div>';
                        html += '</div>';
                    });
                    historyDiv.innerHTML = html;
                });
        }
        
        // Tải lịch sử khi trang load
        loadHistory();
        
        // Tự động cập nhật mỗi 30 giây
        setInterval(loadHistory, 30000);
    </script>
</body>
</html>
    )";
    server.send(200, "text/html", html);
  });

  // API lưu cài đặt WiFi
  server.on("/wifi", HTTP_POST, []() {
    String ssid = server.hasArg("ssid") ? server.arg("ssid") : "";
    String password = server.hasArg("password") ? server.arg("password") : "";
    
    if (ssid.length() > 0 && password.length() > 0) {
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
      server.send(200, "text/plain", "Đã lưu cài đặt WiFi. Hệ thống sẽ khởi động lại...");
      delay(1000);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "Thông tin WiFi không hợp lệ");
    }
  });

  // API lưu cài đặt API
  server.on("/api", HTTP_POST, []() {
    String api_token = server.hasArg("api_token") ? server.arg("api_token") : "";
    String account = server.hasArg("account") ? server.arg("account") : "";
    
    if (api_token.length() > 0 && account.length() > 0) {
      preferences.putString("api_token", api_token);
      preferences.putString("account", account);
      server.send(200, "text/plain", "Đã lưu cài đặt API");
    } else {
      server.send(400, "text/plain", "Thông tin API không hợp lệ");
    }
  });

  // API lưu cài đặt hệ thống
  server.on("/settings", HTTP_POST, []() {
    if (server.hasArg("volume")) {
      volumeLevel = server.arg("volume").toInt();
      audio.setVolume(volumeLevel);
    }
    if (server.hasArg("custom_message")) {
      customMessage = server.arg("custom_message");
    }
    ledEnabled = server.hasArg("led_enabled");
    autoRestart = server.hasArg("auto_restart");
    
    saveSettings();
    server.send(200, "text/plain", "Đã lưu cài đặt");
  });

  // API test âm thanh
  server.on("/test", HTTP_POST, []() {
    if (server.hasArg("amount")) {
      long amount = server.arg("amount").toInt();
      if (amount > 0) {
        playNotification(amount);
        server.send(200, "text/plain", "Đang phát thông báo...");
      } else {
        server.send(400, "text/plain", "Số tiền không hợp lệ");
      }
    } else {
      server.send(400, "text/plain", "Thiếu thông tin số tiền");
    }
  });

  // API lấy lịch sử giao dịch
  server.on("/history", HTTP_GET, []() {
    StaticJsonDocument<4096> doc;
    JsonArray history = doc.createNestedArray("history");
    
    for (int i = 0; i < totalTransactions; i++) {
      JsonObject tx = history.createNestedObject();
      tx["date"] = transactionHistory[i].date;
      tx["amount"] = transactionHistory[i].amount;
      tx["sender"] = transactionHistory[i].sender;
      tx["note"] = transactionHistory[i].note;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  // API khởi động lại
  server.on("/restart", HTTP_POST, []() {
    server.send(200, "text/plain", "Đang khởi động lại...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
  Serial.println("Web server đã khởi động");
}

// Hàm kết nối WiFi với timeout
bool connectWiFi() {
  Serial.println("Đang kết nối WiFi...");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(preferences.getString("ssid", DEFAULT_SSID).c_str(), 
             preferences.getString("password", DEFAULT_PASSWORD).c_str());
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi đã kết nối!");
    Serial.println("IP: " + WiFi.localIP().toString());
    wifiConnected = true;
    return true;
  } else {
    Serial.println("\nLỗi: Không thể kết nối WiFi!");
    wifiConnected = false;
    return false;
  }
}

// Hàm phát thông báo
void playNotification(long amount) {
  if (isSpeaking) return; // Tránh phát chồng lấp
  
  isSpeaking = true;
  String message = customMessage.length() > 0 ? 
    customMessage + ", " + numberToVietnamese(amount) + " đồng" :
    "Thanh toán thành công, đã nhận " + numberToVietnamese(amount) + " đồng";
  
  Serial.println("Phát thông báo: " + message);
  
  // Hiệu ứng LED khi phát âm thanh
  if (ledEnabled) {
    digitalWrite(LED_PIN, HIGH);
  }
  
  // Phát âm thanh thông báo trước
  audio.connecttohost(notificationSound);
  while (audio.isRunning()) {
    audio.loop();
    delay(10);
  }
  
  // Phát thông báo bằng giọng nói
  audio.connecttospeech(message.c_str(), "vi");
}

// Hàm kiểm tra giao dịch mới
bool checkNewTransaction() {
  if (!wifiConnected) return false;
  
  HTTPClient http;
  String apiUrl = "https://my.sepay.vn/userapi/transactions/list?account_number=" + 
                  preferences.getString("account", DEFAULT_ACCOUNT) + "&limit=5";
  
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + preferences.getString("api_token", DEFAULT_API_TOKEN));
  
  int httpCode = http.GET();
  Serial.println("HTTP Code: " + String(httpCode));
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.println("Lỗi API: " + String(httpCode));
    http.end();
    return false;
  }
  
  String payload = http.getString();
  http.end();
  
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    Serial.println("Lỗi parse JSON: " + String(error.c_str()));
    return false;
  }
  
  JsonArray transactions = doc["transactions"];
  if (transactions.size() == 0) {
    Serial.println("Không có giao dịch nào");
    return false;
  }
  
  JsonObject transaction = transactions[0];
  String newTransactionDate = transaction["transaction_date"] | "";
  String amountInStr = transaction["amount_in"] | "0";
  String sender = transaction["sender_name"] | "";
  String note = transaction["note"] | "";
  
  // Xử lý số tiền
  amountInStr.replace(".00", "");
  long newAmount = amountInStr.toInt();
  
  // Kiểm tra giao dịch mới
  if (newTransactionDate != lastTransactionDate && lastTransactionDate.length() > 0) {
    lastTransactionDate = newTransactionDate;
    lastAmountIn = newAmount;
    
    // Thêm vào lịch sử
    addToHistory(newTransactionDate, newAmount, sender, note);
    
    return true;
  } else if (lastTransactionDate.length() == 0) {
    // Lưu giao dịch đầu tiên
    lastTransactionDate = newTransactionDate;
    lastAmountIn = newAmount;
    addToHistory(newTransactionDate, newAmount, sender, note);
  }
  
  return false;
}

// Hàm xử lý input từ Serial
void handleSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() == 0) return;
    
    // Xử lý các lệnh đặc biệt
    if (input.startsWith("cmd:")) {
      String command = input.substring(4);
      if (command == "restart") {
        Serial.println("Khởi động lại hệ thống...");
        ESP.restart();
      } else if (command == "ap") {
        Serial.println("Chuyển sang chế độ AP...");
        startAPMode();
      } else if (command == "wifi") {
        Serial.println("Kết nối WiFi...");
        connectWiFi();
      } else if (command == "volume") {
        Serial.println("Âm lượng hiện tại: " + String(volumeLevel));
      } else if (command == "history") {
        Serial.println("Lịch sử giao dịch:");
        for (int i = 0; i < totalTransactions; i++) {
          Serial.println(String(i+1) + ". " + transactionHistory[i].amount + " VNĐ - " + transactionHistory[i].date);
        }
      }
      return;
    }
    
    // Loại bỏ .00 và chuyển thành số
    input.replace(".00", "");
    long amount = input.toInt();
    
    if (amount > 0) {
      Serial.println("Test âm thanh với số: " + String(amount));
      playNotification(amount);
      Serial.println("Nhập số tiền mới để test:");
    } else {
      Serial.println("Lỗi: Số tiền không hợp lệ!");
      Serial.println("Các lệnh có sẵn: cmd:restart, cmd:ap, cmd:wifi, cmd:volume, cmd:history");
    }
  }
}

// Hàm xử lý nút bấm
void handleButton() {
  static unsigned long lastButtonPress = 0;
  static bool buttonPressed = false;
  
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonPressed && (millis() - lastButtonPress) > 200) {
      buttonPressed = true;
      lastButtonPress = millis();
      
      // Chuyển đổi trạng thái LED
      ledEnabled = !ledEnabled;
      digitalWrite(LED_PIN, ledEnabled ? HIGH : LOW);
      Serial.println("LED: " + String(ledEnabled ? "Bật" : "Tắt"));
      
      // Phát âm thanh thông báo
      audio.connecttospeech(ledEnabled ? "Đã bật đèn LED" : "Đã tắt đèn LED", "vi");
    }
  } else {
    buttonPressed = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== LOA THÔNG BÁO CHUYỂN KHOẢN TỰ ĐỘNG ===");
  
  // Khởi tạo SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Lỗi khởi tạo SPIFFS");
  }
  
  // Khởi tạo Preferences
  preferences.begin("loa_thong_bao", false);
  loadSettings();
  
  // Khởi tạo GPIO
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, ledEnabled ? HIGH : LOW);
  
  // Tải lịch sử giao dịch
  loadHistoryFromFile();
  
  // Kết nối WiFi
  if (!connectWiFi()) {
    Serial.println("Không thể kết nối WiFi. Chuyển sang chế độ AP...");
    startAPMode();
  }
  
  // Cấu hình I2S và âm thanh
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volumeLevel);
  Serial.println("I2S đã khởi tạo");
  
  // Khởi động web server
  setupWebServer();
  
  // Phát âm thanh khởi động
  audio.connecttospeech("Xin chào, đây là loa thông báo chuyển khoản tự động! Hệ thống đã khởi động xong!", "vi");
  
  Serial.println("Hệ thống sẵn sàng!");
  Serial.println("IP: " + WiFi.localIP().toString());
  Serial.println("Nhập số tiền để test âm thanh (ví dụ: 15000000)");
  Serial.println("Hoặc sử dụng lệnh: cmd:restart, cmd:ap, cmd:wifi, cmd:volume, cmd:history");
  
  lastApiCheck = millis();
}

void loop() {
  // Xử lý web server
  server.handleClient();
  
  // Kiểm tra chế độ AP
  if (isAPMode) {
    if (millis() - apStartTime > AP_TIMEOUT) {
      Serial.println("Hết thời gian AP, khởi động lại...");
      ESP.restart();
    }
    delay(100);
    return;
  }
  
  // Kiểm tra kết nối WiFi
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println("Mất kết nối WiFi!");
      wifiConnected = false;
    }
    if (!connectWiFi()) {
      delay(5000);
      return;
    }
  }
  
  // Xử lý nút bấm
  handleButton();
  
  // Xử lý input từ Serial
  handleSerialInput();
  
  // Kiểm tra giao dịch mới
  if (!isSpeaking && (millis() - lastApiCheck) > API_CHECK_INTERVAL) {
    if (checkNewTransaction()) {
      playNotification(lastAmountIn);
    }
    lastApiCheck = millis();
  }
  
  // Xử lý phát âm thanh
  audio.loop();
  
  // Kiểm tra trạng thái âm thanh
  if (!audio.isRunning() && isSpeaking) {
    isSpeaking = false;
    digitalWrite(LED_PIN, ledEnabled ? HIGH : LOW);
    Serial.println("Hoàn thành phát âm thanh");
  }
  
  // Tự động khởi động lại nếu cần
  if (autoRestart && millis() > 86400000) { // 24 giờ
    Serial.println("Tự động khởi động lại sau 24 giờ...");
    ESP.restart();
  }
  
  // Delay nhỏ để tránh loop quá nhanh
  delay(10);
}