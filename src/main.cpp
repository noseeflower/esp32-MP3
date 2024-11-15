#include "Arduino.h"
#include "Audio.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include "U8g2lib.h"

void Write_List(const char* path);
void Read_Name(int target_line);
void U8g2_Show();
String Read_Path(int target_line);


// Digital I/O used
#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

#define add_vol       2
#define cut_vol       15
#define next_music    16
#define pre_music     4
#define start_music   17

Audio audio;
String line_one, line_two, line_three;
int line_index = 1, max_index, music_start = 0, menu_change = 0;
String music_path;
int Vol = 1;
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

void Change_1(){        //中断音量+
    Vol+=1;
    if(Vol>21){Vol=21;}
    audio.setVolume(Vol);  //设置音量
}

void Change_2(){        //中断音量-
    Vol-=1;
    if(Vol<0){Vol=0;}
    audio.setVolume(Vol);  //设置音量
}

void Change_3(){
    line_index+=1;
    Serial.println(line_index);
    menu_change = 1;
}

void Change_4(){
    line_index-=1;
    Serial.println(line_index);
    menu_change = 1;
}

void Change_5(){
    music_start = 1;
}

void setup() {

    pinMode(SD_CS, OUTPUT);      digitalWrite(SD_CS, HIGH);
    pinMode(add_vol, INPUT_PULLDOWN);       //配置音量+引脚
    pinMode(cut_vol, INPUT_PULLDOWN);       //配置音量-引脚
    pinMode(next_music, INPUT_PULLDOWN);    //配置下一首引脚
    pinMode(pre_music, INPUT_PULLDOWN);     //配置上一首引脚
    pinMode(start_music, INPUT_PULLDOWN);   //配置播放引脚
    attachInterrupt(digitalPinToInterrupt(add_vol), Change_1, FALLING);     //中断音量+
    attachInterrupt(digitalPinToInterrupt(cut_vol), Change_2, FALLING);     //中断音量-
    attachInterrupt(digitalPinToInterrupt(next_music), Change_3, FALLING);
    attachInterrupt(digitalPinToInterrupt(pre_music), Change_4, FALLING);
    attachInterrupt(digitalPinToInterrupt(start_music), Change_5, FALLING);

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setFrequency(1000000);
    Serial.begin(115200);
    SD.begin(SD_CS);

    //获取Music目录中的文件，初始化file_list.txt和file_path.txt文件
    Write_List("/Music");

    //获取目标行数的文件路径，获取ssd1306上显示的文本
    music_path = Read_Path(line_index);
    Read_Name(line_index);

    //ssd1306显示初始化配置
    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_wqy13_t_gb2312b);
    U8g2_Show();

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(1); // 0...21，设置初始音量
    audio.connecttoSD(music_path.c_str());      //播放文件
}

void loop()
{
    audio.loop();
    vTaskDelay(1);
    if(Serial.available()){ // put streamURL in serial monitor
        audio.stopSong();
        String r=Serial.readString(); r.trim();
        if(r.length()>5) audio.connecttohost(r.c_str());
        log_i("free heap=%i", ESP.getFreeHeap());
    }

    //播放选中的音乐
    if(music_start == 1){
        music_start = 0;
        audio.stopSong();
        music_path = Read_Path(line_index);
        audio.connecttoSD(music_path.c_str());
    }

    //刷新显示屏
    if(menu_change == 1){
        menu_change = 0;
        Read_Name(line_index);
        U8g2_Show();
    }

}

// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
}
void audio_showstation(const char *info){
    Serial.print("station     ");Serial.println(info);
}
void audio_showstreamtitle(const char *info){
    Serial.print("streamtitle ");Serial.println(info);
}
void audio_bitrate(const char *info){
    Serial.print("bitrate     ");Serial.println(info);
}
void audio_commercial(const char *info){  //duration in sec
    Serial.print("commercial  ");Serial.println(info);
}
void audio_icyurl(const char *info){  //homepage
    Serial.print("icyurl      ");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    Serial.print("lasthost    ");Serial.println(info);
}

//将Music文件夹中的文件名，写入到根目录下的file_list.txt文件中
//将文件路径，写入到file_path.txt文件中
void Write_List(const char* path){
    File root = SD.open(path);
    File file = root.openNextFile();
    File file_list = SD.open("/file_list.txt", FILE_WRITE);
    File file_path = SD.open("/file_path.txt", FILE_WRITE);
    while(file){
        file_list.println(file.name());     //在file_list.txt中写入文件名
        file_path.print("/Music/");         //分两步在file_path.txt中写入文件路径
        file_path.println(file.name());
        file = root.openNextFile();         //打开下一个文件
        max_index+=1;
    }
    file_list.close();
    file_path.close();
    root.close();
}

//读取file_list.txt文件中的内容
void Read_Name(int target_line){
    int remain = max_index-target_line;
    File file_list =SD.open("/file_list.txt");
    for(int i = 1; i < target_line; i++){       //for循环跳过前面的内容
        file_list.readStringUntil('\n');
    }
    if(remain>=0){
        line_one = file_list.readStringUntil('\n');             //ssd1306上的第一行
    }
    if(remain>=1){
        line_two = file_list.readStringUntil('\n');             //ssd1306上的第二行
    }
    if(remain>=2){
        line_three = file_list.readStringUntil('\n');           //ssd1306上的第三行
    }

    file_list.close();

}

//读取目标行数的文件路径
String Read_Path(int target_line){
    File file_path =SD.open("/file_path.txt");
    for(int i = 1; i < target_line; i++) {       //for循环跳过前面的内容
        file_path.readStringUntil('\n');
    }
    String line_txt = file_path.readStringUntil('\n');
    file_path.close();

    return line_txt;
}

//ssd1306显示函数
void U8g2_Show(){
    u8g2.clearBuffer();
    u8g2.setCursor(0,13);
    u8g2.print(line_one);
    u8g2.setCursor(0,28);
    u8g2.print(line_two);
    u8g2.setCursor(0,43);
    u8g2.print(line_three);
    u8g2.sendBuffer();
}
