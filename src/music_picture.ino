#include <Arduino_GFX_Library.h>
#include "BmpClass.h"
#include "Arduino.h"
#include "Audio.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include <TAMC_GT911.h>

#define BMP_FILENAME "/logo.bmp"

#define TFT_BL 2

#define SD_SCK 12
#define SD_MISO 13
#define SD_MOSI 11
#define SD_CS 10

#define I2S_DOUT 17
#define I2S_BCLK 0
#define I2S_LRC 18

#define TOUCH_SCL 20
#define TOUCH_SDA 19
#define TOUCH_INT -1
#define TOUCH_RST 38
#define TOUCH_ROTATION ROTATION_NORMAL
#define TOUCH_MAP_X1 0
#define TOUCH_MAP_X2 800
#define TOUCH_MAP_Y1 480
#define TOUCH_MAP_Y2 0

//---- TFT Set ------------------------------------------

// 7"
 Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
     GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* SDA */,
     41 /* DE */, 40 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
     14 /* R0 */, 21 /* R1 */, 47 /* R2 */, 48 /* R3 */, 45 /* R4 */,
     9 /* G0 */, 46 /* G1 */, 3 /* G2 */, 8 /* G3 */, 16 /* G4 */, 1 /* G5 */,
     15 /* B0 */, 7 /* B1 */, 6 /* B2 */, 5 /* B3 */, 4 /* B4 */
 );
Arduino_RPi_DPI_RGBPanel *gfx = new Arduino_RPi_DPI_RGBPanel(
    bus,
    800 /* width */, 0 /* hsync_polarity */, 210 /* hsync_front_porch */, 30 /* hsync_pulse_width */, 16 /* hsync_back_porch */,
    480 /* height */, 0 /* vsync_polarity */, 22 /* vsync_front_porch */, 13 /* vsync_pulse_width */, 10 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 8000000 /* prefer_speed */, true /* auto_flush */);

// 5"
// Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
//     GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* SDA */,
//     40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
//     45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
//     5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
//     8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */
// );
// Arduino_RPi_DPI_RGBPanel *gfx = new Arduino_RPi_DPI_RGBPanel(
//     bus,
//     800 /* width */, 0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
//     480 /* height */, 0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */,
//     1 /* pclk_active_neg */, 32000000 /* prefer_speed */, true /* auto_flush */);

static BmpClass bmpClass;

// pixel drawing callback
static void bmpDrawCallback(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h)
{
    gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
}

String img_list[20];
int img_num = 0;
int img_index = 1;

int touch_last_x = 0;
int touch_last_y = 0;

TAMC_GT911 ts = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, max(TOUCH_MAP_X1, TOUCH_MAP_X2), max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));

uint img_time = 0;

//---- Audio Set ------------------------------------------

struct Music_info
{
    String name;
    int length;
    int runtime;
    int volume;
    int status;
    int mute_volume;
} music_info = {"", 0, 0, 0, 0, 0};

Audio audio;

String music_list[20];
int music_num = 0;
int music_index = 0;

//---- Main --------------------------------------------------

void setup()
{
    Serial.begin(115200);

    pin_init();
    sd_init();
    tft_init();
    touch_init();

    while (1)
    {
        if (touch_touched())
        {
            if (touch_last_x > 300 && touch_last_x < 540 && touch_last_y > 160 && touch_last_y < 320)
                break;
            else
                gfx->fillCircle(touch_last_x, touch_last_y, 10, RED);
        }
        delay(10);
    }

    logo_show();

    audio_init();

    open_new_song(music_list[music_index]);

    // img_time = millis();
}

void loop()
{
    // if (millis() - img_time > 5000)
    if (0)
    {
        Serial.println("pic");
        // audio.stopSong();

        File bmpFile = SD.open(img_list[img_index], "r");
        bmpClass.draw(&bmpFile, bmpDrawCallback, false, 0, 0, gfx->width(), gfx->height());
        bmpFile.close();

        img_index++;
        if (img_index >= img_num)
        {
            img_index = 0;
        }
        img_time = millis();

        music_index++;
        if (music_index >= music_num)
        {
            music_index = 0;
        }
        // open_new_song(music_list[music_index]);
    }
    audio.loop();
}

//---- Device init --------------------------------------------------

void pin_init()
{
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
}

void sd_init()
{
    // SD(SPI)
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
    SPI.setFrequency(1000000);
    if (!SD.begin(SD_CS, SPI))
    {
        Serial.println("Card Mount Failed");
        while (1)
            ;
    }
    else
    {
        Serial.println("SD OK");
    }
}

void tft_init()
{
    // Init Display
    gfx->begin();
    // gfx->fillScreen(BLACK);

    gfx->fillScreen(0xEF9E);
    gfx->fillRect(300, 160, 240, 160, BLACK);

    gfx->setTextSize(4);
    gfx->setTextColor(WHITE);
    gfx->setCursor(320, 180);
    gfx->println("WHAT IS THIS");
    gfx->setCursor(320, 220);
    gfx->println("TO");
    gfx->setCursor(320, 260);
    gfx->println("CONTINUE");
}

void touch_init()
{
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    ts.begin();
    ts.setRotation(TOUCH_ROTATION);
}

void audio_init()
{
    // Read SD
    music_num = get_music_list(SD, "/", 0, music_list);
    Serial.print("Music file count:");
    Serial.println(music_num);
    Serial.println("All music:");
    for (int i = 0; i < music_num; i++)
    {
        Serial.println(music_list[i]);
    }

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(21); // 0...21
}

//----- Display & Touch------------------------------------------
void logo_show()
{
    img_num = get_img_list(SD, "/", 0, img_list);
    Serial.print("Img file count:");
    Serial.println(img_num);
    Serial.println("All img:");
    for (int i = 0; i < img_num; i++)
    {
        Serial.println(img_list[i]);
    }

    File bmpFile = SD.open(BMP_FILENAME, "r");

    // read BMP file header
    bmpClass.draw(&bmpFile, bmpDrawCallback, false /* useBigEndian */,
                  0 /* x */, 0 /* y */, gfx->width() /* widthLimit */, gfx->height() /* heightLimit */);

    bmpFile.close();
}

bool touch_touched()
{
    ts.read();
    if (ts.isTouched)
    {
        touch_last_x = map(ts.points[0].x, TOUCH_MAP_X1, TOUCH_MAP_X2, 800 - 1, 0);
        touch_last_y = map(ts.points[0].y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, 480 - 1);

        Serial.print("ox = ");
        Serial.print(ts.points[0].x);
        Serial.print(", oy = ");
        Serial.print(ts.points[0].y);
        Serial.print(",x = ");
        Serial.print(touch_last_x);
        Serial.print(", y = ");
        Serial.print(touch_last_y);
        Serial.println();

        return true;
    }
    else
    {
        return false;
    }
}

//----- SD read------------------------------------------

int get_img_list(fs::FS &fs, const char *dirname, uint8_t levels, String list[30])
{
    Serial.printf("Listing directory: %s\n", dirname);
    int i = 0;

    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("Failed to open directory");
        return i;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return i;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
        }
        else
        {
            String temp = file.name();
            if (temp.endsWith(".bmp"))
            {
                list[i] = (String) "/" + temp;
                i++;
            }
        }
        file = root.openNextFile();
    }
    return i;
}

int get_music_list(fs::FS &fs, const char *dirname, uint8_t levels, String wavlist[30])
{
    Serial.printf("Listing directory: %s\n", dirname);
    int i = 0;

    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("Failed to open directory");
        return i;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return i;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
        }
        else
        {
            String temp = file.name();
            if (temp.endsWith(".wav"))
            {
                wavlist[i] = temp;
                i++;
            }
            else if (temp.endsWith(".mp3"))
            {
                wavlist[i] = temp;
                i++;
            }
        }
        file = root.openNextFile();
    }
    return i;
}

//----- Audio Function --------------------------------------------------

void open_new_song(String filename)
{
    //去掉文件名的根目录"/"和文件后缀".mp3",".wav"
    music_info.name = filename.substring(1, filename.indexOf("."));
    audio.connecttoFS(SD, filename.c_str());
    music_info.runtime = audio.getAudioCurrentTime();
    music_info.length = audio.getAudioFileDuration();
    music_info.volume = audio.getVolume();
    music_info.status = 1;
    Serial.println("**********start a new sound************");
}

void audio_info(const char *info)
{
    Serial.print("info        ");
    Serial.println(info);
}
void audio_id3data(const char *info)
{ // id3 metadata
    Serial.print("id3data     ");
    Serial.println(info);
}
void audio_eof_mp3(const char *info)
{ // end of file
    Serial.print("eof_mp3     ");
    Serial.println(info);

    File bmpFile = SD.open(img_list[img_index], "r");
    bmpClass.draw(&bmpFile, bmpDrawCallback, false, 0, 0, gfx->width(), gfx->height());
    bmpFile.close();

    img_index++;
    if (img_index >= img_num)
    {
        img_index = 0;
    }

    music_index++;
    if (music_index >= music_num)
    {
        music_index = 0;
    }
    open_new_song(music_list[music_index]);
}
void audio_showstation(const char *info)
{
    Serial.print("station     ");
    Serial.println(info);
}
void audio_showstreamtitle(const char *info)
{
    Serial.print("streamtitle ");
    Serial.println(info);
}
void audio_bitrate(const char *info)
{
    Serial.print("bitrate     ");
    Serial.println(info);
}
void audio_commercial(const char *info)
{ // duration in sec
    Serial.print("commercial  ");
    Serial.println(info);
}
void audio_icyurl(const char *info)
{ // homepage
    Serial.print("icyurl      ");
    Serial.println(info);
}
void audio_lasthost(const char *info)
{ // stream URL played
    Serial.print("lasthost    ");
    Serial.println(info);
}
