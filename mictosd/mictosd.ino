#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <driver/i2s.h>

const int chipSelect = 5;
File audioFile;
// kết nối các chân
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32
 
// Use I2S Processor 0
#define I2S_PORT I2S_NUM_0
 
// Define input buffer length
#define bufferLen 64
int16_t sBuffer[bufferLen];
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

void i2s_install() {
  // Set up I2S Processor configuration
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = bufferLen,
    .use_apll = false
  };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}
void i2s_setpin() {
  // Set I2S pin configuration
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };
 
  i2s_set_pin(I2S_PORT, &pin_config);
}
void setup() {
  Serial.begin(115200);
  Serial.println(" ");
 
  delay(1000);
 
  // Set up I2S
  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);
  Serial.print("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized.");
  
  listDir(SD, "/", 0);
    createDir(SD, "/mydir");
    listDir(SD, "/", 0);
    removeDir(SD, "/mydir");
    listDir(SD, "/", 2);
    
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
    //writeFile(SD, "/recording.wav", mean);
    appendFile(SD, "/recording.wav", mean);
   
    }
  }
   readFile(SD, "/recording.wav");
  //Start recording audio
  // // audioFile = SD.open("recording.wav", FILE_WRITE);
  // // if (audioFile) {
  // //   Serial.println("Record started: ");
  //   size_t bytes_read;
  //   uint8_t i2s_data[64];

  //   // Read audio data from I2S interface
  //   i2s_read(I2S_NUM_0, i2s_data, sizeof(i2s_data), &bytes_read, portMAX_DELAY);
  //   // writeFile(SD,"/recording.wav","listen");
  //   // readFile(SD,"/recording.wav");
  //   // Write audio data to the SD card
  //   audioFile.write(i2s_data, bytes_read);
  // }else {
  //   Serial.println("Error opening recording file.");
  // } 

  // Stop recording if 's' or 'S' is received from Serial input
  // if (Serial.available()) {
  //   char c = Serial.read();
  //   if (c == 's' || c == 'S') {
  //     if (audioFile) {
  //       audioFile.close();
  //       Serial.println("Recording stopped.");
  //     }
  //   }
  // }
}

void loop() {
}