#include <driver/i2s.h>// cung cấp hàm và chức năng để cấu hình i2s
#include <WiFi.h>// cấu hình và kết nối wifi
#include <ArduinoWebsockets.h>//như wifi nhưng websocket truyền dữ liệu liên tục
#include <SD.h>//làm việc với SD( khởi tạo,đọc ghi,...)
#include <SPI.h>//để giao tiếp giữa các vi xử lý, vi điều khiển và các thiết bị ngoại vi như cảm biến, bộ nhớ, màn hình
#include <Wire.h>// giao thức i2c với các thiết bị ngoại vi
#define I2S_SD 33//chân gpio 33
#define I2S_WS 25
#define I2S_SCK 32
#define I2S_PORT I2S_NUM_0// I2S0
#define bufferCnt 8
#define bufferLen 64
int16_t sBuffer[bufferLen];
const int chipSelect = 5;// chân CS của SD là gpio 5
File audioFile;
const char* ssid = "Glory Man utd";
const char* password = "inuyasha";

const char* websocket_server_host = "192.168.43.157";
const uint16_t websocket_server_port = 8888;  // <WEBSOCKET_SERVER_PORT>

using namespace websockets;
WebsocketsClient client;
bool isWebSocketConnected;//check đã kết nối websocket chưa
hw_timer_t * timer = NULL;// biến timer thực hiện ngắt

//hàm thực hiện khi ngắt xảy ra
void IRAM_ATTR onTimer(){
  mictoSD();
}

//hàm sự kiện với websocket
void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    Serial.println("Connnection Opened");
    isWebSocketConnected = true;
  } else if (event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("Connnection Closed");
    isWebSocketConnected = false;
  } else if (event == WebsocketsEvent::GotPing) {
    Serial.println("Got a Ping!");
  } else if (event == WebsocketsEvent::GotPong) {
    Serial.println("Got a Pong!");
  }
}

void i2s_install() {
  // cài đặt i2s
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,//tần số lấy mẫu
    //.sample_rate = 16000,
    .bits_per_sample = i2s_bits_per_sample_t(16),//số bit dùng để lượng tử hóa
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = bufferCnt,
    .dma_buf_len = bufferLen,
    .use_apll = false
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  // cấu hình cho i2s
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pin_config);
}
//hàm liệt kê các tệp có trong sd
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}
//hàm tạo tệp
void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}
//hàm đọc file, các biến và hàm đã được hỗ trợ bởi SD.h
void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        uint8_t buffer[sizeof(float)];
    if (file.read(buffer, sizeof(buffer)) == sizeof(buffer)) {
      float value;
      memcpy(&value, buffer, sizeof(value));
      Serial.println(value);
    } else {
      Serial.println("cannot read");
    }
    }
    file.close();
}
//hàm viết lại file hiện có(xóa trắng viết lại hết)
void writeFile(fs::FS &fs, const char * path, float  value){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.write((uint8_t *)&value,sizeof(value))){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}
//hàm viết thêm vào file hiện có
void appendFile(fs::FS &fs, const char * path,float  value){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.write((uint8_t *)&value,sizeof(value))){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}
//hàm chạy chính
void setup() {
  Serial.begin(115200);//baudrate UART0 dùng để giao tiếp máy tính và vi xử lý, chỉnh sang số khác thì không xem dc kết quả dưới dạng bình thường hoặc ko thấy
  Serial.println(" ");
  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);//khởi tạo i2s
  delay(1000);// cho trễ 1s để thực hiện chu kì mới
  connectWiFi();//có hàm ngắt 10s trong hàm nữa thì muốn bắt đầu lưu thì đếm 11s khi connect dc wifi thì mới thực hiện các hàm sau, 
  connectWSServer();//kết nối websocket
  delay(500);
  xTaskCreatePinnedToCore(micTask, "micTask", 10000, NULL, 1, NULL, 1);//thực hiện gửi lên server
  
}

void loop() {
}
// trong hàm kết nối wifi này có sử dụng timer 
void connectWiFi() {
  WiFi.begin(ssid, password);
  timer = timerBegin(0, 80, true);

  /* Attach onTimer function to our timer */
  timerAttachInterrupt(timer, &onTimer, true);

  /* Set alarm to call onTimer function every second 1 tick is 1us
  => 1 second is 1000000us */
  /* Repeat the alarm (third parameter) */
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  //sau 5s nếu tình trạng vẫn không kết nối được wifi thì thực hiện hàm ngắt
  timerAlarmWrite(timer, 10000000, true);

  /* Start an alarm */
  timerAlarmEnable(timer);
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

void connectWSServer() {
  client.onEvent(onEventsCallback);
  while (!client.connect(websocket_server_host, websocket_server_port, "/")) {
    delay(500); 
    Serial.print(".");
  }
  Serial.println("Websocket Connected!");
}


void micTask(void* parameter) {
size_t bytesIn = 0;
  esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);
  
  while (1) {
    esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);
    if (result == ESP_OK && isWebSocketConnected) {
    
      client.sendBinary((const char*)sBuffer, bytesIn);
    }
  }
}
void mictoSD() {
  Serial.print("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized.");
  
  // listDir(SD, "/", 0);
  //   createDir(SD, "/mydir");
  //   listDir(SD, "/", 0);
  //   removeDir(SD, "/mydir");
  //   listDir(SD, "/", 2);
    
  // Get I2S data and place in data buffer
  size_t bytesIn = 0;
  esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);
 
  if (result == ESP_OK)
  {
    // Read I2S data buffer
    int16_t samples_read = bytesIn / 8;
    if (samples_read > 0) {
      float mean = 0;
      for (int16_t i = 0; i < samples_read; ++i) {
        mean += (sBuffer[i]);
      }
 
      // Average the data reading
      mean /= samples_read;
 
      // Print to serial plotter
      Serial.println(mean);
    writeFile(SD, "/recording.wav", mean);
    //appendFile(SD, "/recording.wav", mean);
   
    }
  }
   readFile(SD, "/recording.wav");
}