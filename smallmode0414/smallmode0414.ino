#include <TM1640.h>
#include <TM16xxMatrix.h>

// 引脚定义
#define SDA_PIN PF0
#define SCL_PIN PF1
#define KEY1_PIN PA0
#define KEY2_PIN PA1
#define KEY3_PIN PF4//是boot键
//#define UART_TX_PIN PA2
//#define UART_RX_PIN PA3//没用上

// 显示矩阵尺寸
#define MATRIX_NUMCOLUMNS 16
#define MATRIX_NUMROWS 8
//声明tm1640
TM1640 module(SDA_PIN,SCL_PIN);    // For air001, DIN=PF0,CLK=PF1

TM16xxMatrix matrix(&module, MATRIX_NUMCOLUMNS, MATRIX_NUMROWS);    // TM16xx object, columns, rows

const int pinRandom = PA4;

const int wait = 20; // In milliseconds,50fps
//播放状态枚举
typedef enum {

    MODE_SERIAL_DATA,
    MODE_TEMP,
    MODE_SNAKE,
    MODE_FLAME,
    MODE_CODE_RAIN,
    MODE_RAIN,
    MODE_RANDOM,
    MODE_BOUNCE_PIXEL,
    MODE_FLY,
    MODE_OFF,
    MODE_COUNT
} DisplayMode;
//温度变量
byte tempmap[16];
//小蛇变量
const int snakelength = 8;
int snakex[snakelength], snakey[snakelength];
int snakeptr, snakenextPtr;

//雨滴变量
// 定义方向枚举
enum RAIN_DIR {//方向是错的，后面的要改
    DOWN,   // 0: 向下
    UP,     // 1: 向上  
    LEFT,   // 2: 向左
    RIGHT   // 3: 向右
};
#define RAIN_DROP_COUNT1 16   //雨滴数量

// 雨滴结构体
struct Raindrop1 {
    int x;
    int y;
    bool active;
};

struct Raindrop1 raindrops1[RAIN_DROP_COUNT1];
int raindropCount = 1;  // 当前活跃雨滴数量
RAIN_DIR currentDir = LEFT;  // 当前方向

// 代码雨相关变量
// 代码雨结构体
#define CODE_RAIN_DROP_COUNT1 8//代码雨数量
struct CodeRaindrop1 {
    int x;
    int y;
    bool active;
};

struct CodeRaindrop1 coderaindrops1[CODE_RAIN_DROP_COUNT1];
int coderaindropCount = 1;  // 当前活跃雨滴数量


//按键变量
// 按键状态枚举
typedef enum {
    NO_PRESS = 0,
    KEY1_SHORT_PRESS,
    KEY2_SHORT_PRESS,
    KEY3_SHORT_PRESS,

    KEY1_LONG_PRESS,
    KEY2_LONG_PRESS,
    BOTH_LONG_PRESS,
    KEY3_LONG_PRESS
} KeyAction;
// 按键状态结构体
struct KeyStatus {
    bool key1Pressed;
    bool key2Pressed;
    bool key3Pressed;
    unsigned long key1PressTime;
    unsigned long key2PressTime;
    unsigned long key3PressTime;
    
    bool key1WasPressed;
    bool key2WasPressed;
    bool key3WasPressed;
    bool key1LongPressHandled;
    bool key2LongPressHandled;
    bool key3LongPressHandled;
};

//弹球变量
struct MovingPixel {
  int x;
  int y;
  int dx;  // x方向速度
  int dy;  // y方向速度
  unsigned long lastUpdate;
};
MovingPixel movingPixel = {4, 8, 1, 1, 0};  // 初始位置(0,0)，速度(1,1)

// 串口播放变量
#define CMD_FRAME_MODE 0xF1
#define CMD_PIXEL_MODE 0xF2
#define CMD_SET_MODE 0xF3

byte protocolState = 0;         // 0=空闲, 1=接收帧数据, 2=接收像素数据 3=设置亮度
unsigned long lastProtocolTime = 0;
byte frameBuffer[16];           // 存储16字节帧数据
int frameIndex = 0;             // 当前接收的字节索引

// 全局变量
KeyStatus keyStatus = {false, false, 0, 0, false, false, false, false};
KeyAction currentAction = NO_PRESS;
unsigned long lastCheckTime = 0;
const unsigned long CHECK_INTERVAL = 10; // 每10ms检查一次
const unsigned long LONG_PRESS_TIME = 1000; // 1秒长按
uint8_t brightness = 1; // 0-7
DisplayMode currentMode = MODE_TEMP;//MODE_BOUNCE_PIXEL;//当前模式，可改默认模式
bool ledOn = true;//是否开启led

//蜡烛变量
// ==================== 1. 常量表（存入 Flash，不占用 RAM） ====================
// 柏林噪声排列表（256 字节）
uint8_t buffer[16];
uint8_t seq = 0;
#define TURBULENCE 32//抖动幅度
static const uint8_t PROGMEM perm[256] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
    140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
    247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
    57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
    74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
    65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,
    200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
    52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
    207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
    119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,
    129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
    218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
    81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,
    184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,
    222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

// 平滑插值查找表 t*t*(6t-15)+10，范围 [0,255]
static const uint8_t PROGMEM smooth_lut[256] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x04,
    0x04,0x05,0x05,0x05,0x06,0x06,0x07,0x07,0x08,0x08,0x09,0x09,0x0A,0x0B,0x0B,0x0C,
    0x0D,0x0D,0x0E,0x0F,0x10,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,
    0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x23,0x24,0x25,0x26,0x28,0x29,0x2A,0x2C,0x2D,
    0x2E,0x30,0x31,0x33,0x34,0x36,0x37,0x39,0x3A,0x3C,0x3D,0x3F,0x40,0x42,0x44,0x45,
    0x47,0x48,0x4A,0x4C,0x4E,0x4F,0x51,0x53,0x54,0x56,0x58,0x5A,0x5C,0x5D,0x5F,0x61,
    0x63,0x65,0x66,0x68,0x6A,0x6C,0x6E,0x70,0x71,0x73,0x75,0x77,0x79,0x7B,0x7D,0x7F,
    0x80,0x82,0x84,0x86,0x88,0x8A,0x8C,0x8E,0x8F,0x91,0x93,0x95,0x97,0x99,0x9A,0x9C,
    0x9E,0xA0,0xA2,0xA3,0xA5,0xA7,0xA9,0xAB,0xAC,0xAE,0xB0,0xB1,0xB3,0xB5,0xB7,0xB8,
    0xBA,0xBB,0xBD,0xBF,0xC0,0xC2,0xC3,0xC5,0xC6,0xC8,0xC9,0xCB,0xCC,0xCE,0xCF,0xD1,
    0xD2,0xD3,0xD5,0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDE,0xDF,0xE0,0xE1,0xE2,0xE3,0xE4,
    0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xEF,0xF0,0xF1,0xF2,0xF2,
    0xF3,0xF4,0xF4,0xF5,0xF6,0xF6,0xF7,0xF7,0xF8,0xF8,0xF9,0xF9,0xFA,0xFA,0xFA,0xFB,
    0xFB,0xFC,0xFC,0xFC,0xFC,0xFD,0xFD,0xFD,0xFD,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

// 16x8 蜡烛静态形状（16 列，每列 8 位，MSB=顶部）
// 形状：顶部椭圆火苗 + 底部垂直烛身

static const uint8_t PROGMEM candle_shape[16] = {
    
    
    B00000000,  // 0:  ........  （空）
    B00000000,  // 1:  ..X.XX..
    B00101000,  // 2:  .XXXXX..  火苗左侧
    B00101100,  // 3:  .XXXXXX.
    B01111100,  // 4:  .XXXXXX.  火苗最宽处
    B01111110,  // 5:  .XXXXXX.
    B01111110,  // 6:  .XXXXXX.
    B01111110,  // 7:  .XXXXXX.  火苗右侧
    B01111110,  // 8:  .XXXXXX.  烛身上部
    B00111110,  // 9:  ..XXXX..
    B00111100,  // 10: ..XXXX..
    B00011100,  // 11: ..XXXX..
    B00011000,  // 12: ...XXX..
    B00001000,  // 13: ...XX...
    B00000000,  // 14: ....X...
    B00000000   // 15: ........  烛底
};


static const unsigned char PROGMEM num_shape[10][8] = {
  {
    B00000000,
    B00110000,
    B01010110,
    B01011010,
    B01011010,
    B01101010,
    B00001100,
    B00000000
  },//00
  {
    B00000000,
    B00100000,
    B01100100,
    B00101100,
    B00100100,
    B01110100,
    B00001110,
    B00000000
    },//11
  {
    B00000000,
    B01110000,
    B00011110,
    B01110010,
    B01001110,
    B01111000,
    B00001110,
    B00000000
  },//22
  {
    B00000000,
    B01100000,
    B00011100,
    B01110010,
    B00011110,
    B01110010,
    B00001110,
    B00000000
  },//33
  {
    B00000000,
    B01010000,
    B01011010,
    B01111010,
    B00011110,
    B00010010,
    B00000010,
    B00000000
  },//4
  {
    B00000000,
    B01110000,
    B01001110,
    B01111000,
    B00011110,
    B01110010,
    B00001110,
    B00000000
  },//55
  {
    B00000000,
    B00110000,
    B01000110,
    B01111000,
    B01011110,
    B01111010,
    B00001110,
    B00000000
  },//66
  {
    B00000000,
    B01110000,
    B00011110,
    B00100010,
    B00100100,
    B00100100,
    B00000100,
    B00000000,
  },//77
  {
    B00000000,
    B01110000,
    B01011110,
    B00101010,
    B01010100,
    B01111010,
    B00001110,
    B00000000
  },//88
  {
    B00000000,
    B01110000,
    B01011110,
    B01111010,
    B00011110,
    B01100010,
    B00001100,
    B00000000
  }//99
};
void setup() {
  // put your setup code here, to run once:
  pinMode(KEY1_PIN, INPUT_PULLUP);
  pinMode(KEY2_PIN, INPUT_PULLUP);
  matrix.setAll(false);
  module.setupDisplay(true, brightness); 
  // 初始化串口
  Serial.begin(115200);

  // 初始化雨滴数组
  for (int i = 0; i < RAIN_DROP_COUNT1; i++) {
    raindrops1[i].active = false;
  }
  // 设置初始雨滴
  raindrops1[0].x = random(16);
  raindrops1[0].y = 0;
  raindrops1[0].active = true;
}

//画图片，逐行绘制
void drawBitmap(const uint8_t *bitmap) {
    
    
    for (int x = 0; x < 16; x++) {
      uint8_t byte = bitmap[x];
      //交换高低位，实现从左往右，00000001实际显示是最左边的点亮了。
      //根据pcb设计不同而不同
      byte = (byte & 0xF0) >> 4 | (byte & 0x0F) << 4;  // 交换高4位和低4位
      byte = (byte & 0xCC) >> 2 | (byte & 0x33) << 2;  // 交换每组2位
      byte = (byte & 0xAA) >> 1 | (byte & 0x55) << 1;  // 交换相邻位
      matrix.setColumn(x,byte);//从下往上，依次画
      //matrix.setColumn(16-x,byte);//从上往下，依次画
    }
}
//画数字，
void drawNummap(int left, int right, unsigned char* result) {
  for(int row = 0; row < 8; row++) {
    // 从PROGMEM读取两个字模的当前行
    unsigned char left_byte = pgm_read_byte(&num_shape[left][row]);
    unsigned char right_byte = pgm_read_byte(&num_shape[right][row]);
    // 拼接逻辑：
    // 左数字保持在高4位（bit7-4），右数字移到高4位后右移至低4位（bit3-0）
    result[row] = (left_byte & 0xF0) | ((right_byte & 0x0F));
  }
}
//播放温度
void displayTemp()
{
  float temp=analogReadTempSensor();
  int temp_int = (int)(temp + 0.5);     // 四舍五入取整，如 25.6→26, 5.8→6
  // 或 int temp_int = (int)temp;      // 直接截断小数，如 25.6→25

  int digit_left = temp_int / 10;       // 十位数字 (2)
  int digit_right = temp_int % 10;      // 个位数字 (5)
  unsigned char buffer[8];//
  drawNummap(digit_left, digit_right, buffer);  // 左右显示如"25"
  //drawNummap(1, 4, buffer);  // 左右显示如"25"

  //Serial.print(digit_left);
  //Serial.print(digit_right);
  //Serial.print(temp);
  tempmap[0]=B00000000;
  tempmap[1]=B00111000;
  tempmap[2]=B01000100;
  tempmap[3]=B01000000;
  tempmap[4]=B01000000;
  tempmap[5]=B01000100;
  tempmap[6]=B00111001;
  tempmap[7]=B00000000;
  for(int i=0;i<8;i++){ tempmap[i+8]=buffer[7-i];}
  drawBitmap(tempmap);
  delay(1000);
}

//播放小蛇
void displaysnake()
{
  //matrix.setAll(false);
  // Shift pointer to the next segment
  snakeptr = snakenextPtr;
  snakenextPtr = snakenext(snakeptr);

  matrix.setPixel(snakex[snakeptr], snakey[snakeptr], true); // Draw the head of the snake

  delay(wait*2);

  if ( ! occupied(snakenextPtr) ) {
    matrix.setPixel(snakex[snakenextPtr], snakey[snakenextPtr], false); // Remove the tail of the snake
  }

  for ( int attempt = 0; attempt < 10; attempt++ ) {

    // Jump at random one step up, down, left, or right
    switch ( random(4) ) {
    case 0: snakex[snakenextPtr] = constrain(snakex[snakeptr] + 1, 0, MATRIX_NUMCOLUMNS - 1); snakey[snakenextPtr] = snakey[snakeptr]; break;
    case 1: snakex[snakenextPtr] = constrain(snakex[snakeptr] - 1, 0, MATRIX_NUMCOLUMNS - 1); snakey[snakenextPtr] = snakey[snakeptr]; break;
    case 2: snakey[snakenextPtr] = constrain(snakey[snakeptr] + 1, 0, MATRIX_NUMROWS - 1);    snakex[snakenextPtr] = snakex[snakeptr]; break;
    case 3: snakey[snakenextPtr] = constrain(snakey[snakeptr] - 1, 0, MATRIX_NUMROWS - 1);    snakex[snakenextPtr] = snakex[snakeptr]; break;
    }

    if ( ! occupied(snakenextPtr) ) {
      break; // The spot is empty, break out the for loop
    }
  }
}

//小蛇函数
boolean occupied(int snakeptrA) {
  for ( int snakeptrB = 0 ; snakeptrB < snakelength; snakeptrB++ ) {
    if ( snakeptrA != snakeptrB ) {
      if ( equal(snakeptrA, snakeptrB) ) {
        return true;
      }
    }
  }

  return false;
}

int snakenext(int snakeptr) {
  return (snakeptr + 1) % snakelength;
}

boolean equal(int snakeptrA, int snakeptrB) {
  return snakex[snakeptrA] == snakex[snakeptrB] && snakey[snakeptrA] == snakey[snakeptrB];
}

//播放蝴蝶乱飞
void displayfly() {
  static int x = MATRIX_NUMCOLUMNS/ 2;
  static int y = MATRIX_NUMROWS   / 2;
  int xNext, yNext;
  
  matrix.setPixel(x, y, true);
  //matrix.write(); // Send bitmap to display

  delay(wait);

  matrix.setPixel(x, y, false); // Erase the old position of our dot

  do {
    switch ( random(4) ) {
      case 0: xNext = constrain(x + 1, 0, MATRIX_NUMCOLUMNS - 1); yNext = y; break;
      case 1: xNext = constrain(x - 1, 0, MATRIX_NUMCOLUMNS - 1); yNext = y; break;
      case 2: yNext = constrain(y + 1, 0, MATRIX_NUMROWS - 1); xNext = x; break;
      case 3: yNext = constrain(y - 1, 0, MATRIX_NUMROWS - 1); xNext = x; break;
    }
  }
  while ( x == xNext && y == yNext ); // Repeat until we find a new coordinate

  x = xNext;
  y = yNext;
}


//雨滴下落
void displayRain() {
    // 初始化雨滴数组
    if (raindropCount == 1 && coderaindrops1[0].active == false) {
        raindrops1[0].x = random(16);  // 随机起始列
        raindrops1[0].y = 0;           // 从顶部开始
        raindrops1[0].active = true;
    }

    // 清除屏幕
    // 更新每个雨滴的位置
    for (int i = 0; i < raindropCount; i++) {
        matrix.setPixel(raindrops1[i].x, raindrops1[i].y, false);
        if (raindrops1[i].active) {
            // 根据方向更新位置

                
            switch (currentDir) {
                case DOWN:
                    raindrops1[i].y++;
                    break;
                case UP:
                    raindrops1[i].y--;
                    break;
                case LEFT:
                    raindrops1[i].x--;
                    break;
                case RIGHT:
                    raindrops1[i].x++;
                    break;
            }

            // 检测是否超出范围
            if (raindrops1[i].x < 0 || raindrops1[i].x >= 16 || 
                raindrops1[i].y < 0 || raindrops1[i].y >= 8) {
                
                // 雨滴超出范围，重置位置
                resetRaindrop(i);
            } else {
                // 在新位置点亮雨滴
                matrix.setPixel(raindrops1[i].x, raindrops1[i].y, true);
                
                
            }
        }
    }
    delay(100);//帧率30
    // 随机增加新的雨滴（增加密度）
    if (raindropCount < RAIN_DROP_COUNT1 && random(20) == 0) {  // 5%概率增加新雨滴
        for (int i = 0; i < RAIN_DROP_COUNT1; i++) {
            if (!raindrops1[i].active) {
                resetRaindrop(i);
                raindropCount++;
                break;
            }
        }
    }
}

void resetRaindrop(int index) {
    // 根据当前方向在屏幕边缘生成新雨滴
    switch (currentDir) {
        case DOWN:
            raindrops1[index].x = random(16);  // 随机列
            raindrops1[index].y = 0;           // 顶部行
            break;
        case UP:
            raindrops1[index].x = random(16);  // 随机列
            raindrops1[index].y = 7;           // 底部行
            break;
        case LEFT:
            raindrops1[index].x = 15;          // 右边列
            raindrops1[index].y = random(8);   // 随机行
            break;
        case RIGHT:
            raindrops1[index].x = 0;           // 左边列
            raindrops1[index].y = random(8);   // 随机行
            break;
    }
    raindrops1[index].active = true;
}

//播放代码雨
//雨滴下落
void displayCodeRain() {
    // 初始化雨滴数组
    if (coderaindropCount == 1 && coderaindrops1[0].active == false) {
        coderaindrops1[0].x = random(16);  // 随机起始列
        coderaindrops1[0].y = 0;           // 从顶部开始
        coderaindrops1[0].active = true;
    }

    // 清除屏幕
    // 更新每个雨滴的位置
    for (int i = 0; i < coderaindropCount; i++) {
        matrix.setPixel(coderaindrops1[i].x, coderaindrops1[i].y, false);
        if (coderaindrops1[i].active) {
            // 根据方向更新位置

                
            switch (currentDir) {
                case DOWN:
                    coderaindrops1[i].y++;
                    break;
                case UP:
                    coderaindrops1[i].y--;
                    break;
                case LEFT:
                    coderaindrops1[i].x--;
                    break;
                case RIGHT:
                    coderaindrops1[i].x++;
                    break;
            }

            // 检测是否超出范围
            if (coderaindrops1[i].x < 0 || coderaindrops1[i].x >= 16 || 
                coderaindrops1[i].y < 0 || coderaindrops1[i].y >= 8) {
                
                // 雨滴超出范围，重置位置
                resetCodeRaindrop(i);
            } else {
                // 在新位置点亮雨滴
                matrix.setPixel(coderaindrops1[i].x, coderaindrops1[i].y, true);
            }
        }
    }
    delay(100);//帧率30
    // 随机增加新的雨滴（增加密度）
    if (coderaindropCount < CODE_RAIN_DROP_COUNT1 && random(20) == 0) {  // 5%概率增加新雨滴
        for (int i = 0; i < CODE_RAIN_DROP_COUNT1; i++) {
            if (!coderaindrops1[i].active) {
                resetCodeRaindrop(i);
                coderaindropCount++;
                break;
            }
        }
    }
}

void resetCodeRaindrop(int index) {
    // 根据当前方向在屏幕边缘生成新雨滴
    switch (currentDir) {
        case DOWN:
            coderaindrops1[index].x = random(16);  // 随机列
            coderaindrops1[index].y = 0;           // 顶部行
            break;
        case UP:
            coderaindrops1[index].x = random(16);  // 随机列
            coderaindrops1[index].y = 7;           // 底部行
            break;
        case LEFT:
            coderaindrops1[index].x = 15;          // 右边列
            coderaindrops1[index].y = random(8);   // 随机行
            break;
        case RIGHT:
            coderaindrops1[index].x = 0;           // 左边列
            coderaindrops1[index].y = random(8);   // 随机行
            break;
    }
    coderaindrops1[index].active = true;
}



//闪电效果
void displayRandom() {
    // 遍历每一列，从上到下移动雨滴
    for (int col = 0; col < 16; col++) {
        // 查找该列中所有亮着的像素点
        for (int row = 0; row < 8; row++) {
            if (matrix.getPixel(col, row)) {  // 如果当前像素点亮
                // 灭掉原来的像素点
                matrix.setPixel(col, row, false);
                
                // 计算新位置（向下移动一行）
                int newRow = row + 1;
                
                // 如果新位置在屏幕范围内，则点亮新位置
                if (newRow < 8) {
                    matrix.setPixel(col, newRow, true);
                }
                // 否则雨滴消失（超出屏幕底部）
            }
        }
    }
    // 随机在顶部添加雨滴
    if (random(10) == 0) {  // 10%概率添加新雨滴
        int col = random(16);  // 随机列
        matrix.setPixel(col, 0, true);  // 在顶部添加雨滴
    }

}


//播放弹球
void displayBouncePixel(){


  // 在display函数中更新像素位置
  unsigned long currentTime = millis();
  if (currentTime - movingPixel.lastUpdate > 100) {  // 每200ms移动一次
    // 计算新位置
    int newX = movingPixel.x + movingPixel.dx;
    int newY = movingPixel.y + movingPixel.dy;

    // 检查x边界碰撞
    if (newX >= 15 || newX < 0) {
      //movingPixel.dx = -random(-2,2);  // 反转x方向
      movingPixel.dx = -movingPixel.dx;  // 反转x方向
      
      newX = movingPixel.x;              // 保持当前位置
    }
    
    // 检查y边界碰撞
    if (newY >= 8 || newY < 0) {
      //movingPixel.dy = -random(-2,2);  // 反转y方向
      movingPixel.dy = -movingPixel.dy;  // 反转y方向
      newY = movingPixel.y;              // 保持当前位置
    }
    
    //更新位置前去掉上一个
    matrix.setPixel(movingPixel.x, movingPixel.y, false);

    // 更新位置

    movingPixel.x = newX;
    movingPixel.y = newY;
    Serial.println(newX,newY);
    movingPixel.lastUpdate = currentTime;
    matrix.setPixel(movingPixel.x, movingPixel.y, true);  
  }

  // 清屏并绘制像素
}

//播放串口数据
void displaySerialData() {
  while (Serial.available()) {
  byte incomingByte = Serial.read();
  unsigned long currentTime = millis();
  lastProtocolTime = currentTime;
  
  switch (protocolState) {
    case 0: // 空闲状态，等待命令
      if (incomingByte == CMD_FRAME_MODE) {
          protocolState = 1; // 进入帧传输模式
          frameIndex = 0;    // 重置接收索引
      } else if (incomingByte == CMD_PIXEL_MODE) {
          protocolState = 2; // 进入单点传输模式
      }
      else if (incomingByte == CMD_SET_MODE) {
          protocolState = 3; // 进入设置模式)
          }
      break;
  	  
      
    case 1: // 接收帧数据 (16字节)
      if (frameIndex < 16) {
        frameBuffer[frameIndex] = incomingByte;
        frameIndex++;
        }
      if (frameIndex >= 16) { // 接收完毕
        // 使用自定义函数绘制帧数据
        drawBitmap(frameBuffer);
        protocolState = 0; // 返回空闲状态
      }
      break;
        
    case 2: // 接收像素数据 (1字节)
      {
      // 解析像素数据: [bit0-2:行][bit3-6:列][bit7:开关]
      int row = incomingByte & 0x07;        // 低3位: 0-7行
      int col = (incomingByte >> 3) & 0x0F; // 中4位: 0-15列
      bool onOff = (incomingByte >> 7) & 1; // 最高位: 开/关
      
      // 设置单个像素
      if (row < 8 && col < 16) {
        matrix.setPixel(col, row, onOff);
        //matrix.writeDisplay(); // 刷新显示
      }
      protocolState = 0; // 返回空闲状态
      break;
      }
    
    case 3://设置亮度
      if (incomingByte & 0x08)module.setupDisplay(true,incomingByte & 0x08);//低8位为亮度，为0则不反应
      
      protocolState = 0; // 返回空闲状态
      break;
    }
    // 超时保护 (1秒)
    if (millis() - lastProtocolTime > 1000) 
    {
        if (protocolState != 0) {
          // 超时且未完成传输，根据模式执行不同操作
          if (protocolState == 1) {
            matrix.setAll(false); // 帧模式超时则清屏
            delay(500);
            matrix.setAll(true);
            delay(500);
            matrix.setAll(false);
            delay(500);
            matrix.setAll(true);
            delay(500);
            matrix.setAll(false);
            //matrix.writeDisplay();
          }   
          protocolState = 0; // 重置状态
          frameIndex = 0;
        }
    }
  }
}

//播放蜡烛

void displayFlame(){
    candleRender(buffer, seq++, TURBULENCE);  // 生成当前帧到 buffer
    drawBitmap(buffer);  // 直接传入 16 字节数组
    delay(10);  // 20 FPS，seq 会自动循环 0-255
}
// ==================== 2. 核心算法函数 ====================
static inline int8_t lerp(uint8_t t, int8_t a, int8_t b) {
    return ((int16_t)a * (256 - t) + (int16_t)b * t) >> 8;
}

static inline int8_t grad(uint8_t hash, int8_t x, int8_t y) {
    return ((hash & 1) ? -x : x) + ((hash & 2) ? -y : y);
}

// 8-bit 柏林噪声，输出 [0,255]
static uint8_t perlin_noise(uint8_t x, uint8_t y) {
    uint8_t X = x >> 4;  // 16x16 网格
    uint8_t Y = y >> 4;
    int8_t fx = (x & 0x0F) << 2;  // 0-60
    int8_t fy = (y & 0x0F) << 2;
    
    uint8_t u = pgm_read_byte(&smooth_lut[x]);
    uint8_t v = pgm_read_byte(&smooth_lut[y]);
    uint8_t A = pgm_read_byte(&perm[X]) + Y;
    uint8_t B = pgm_read_byte(&perm[X + 1]) + Y;
    
    int8_t r = lerp(v, 
        lerp(u, grad(pgm_read_byte(&perm[A]), fx, fy), 
                grad(pgm_read_byte(&perm[B]), fx - 64, fy)),
        lerp(u, grad(pgm_read_byte(&perm[A + 1]), fx, fy - 64), 
                grad(pgm_read_byte(&perm[B + 1]), fx - 64, fy - 64))
    );
    return (uint8_t)r + 128;
}

// 采样静态形状（16x8 分辨率，输入 u,v 范围 [0,255]）
static inline uint8_t candle_sample(uint8_t u, uint8_t v) {
    uint8_t x = u >> 4;   // /16，映射到 0-15
    uint8_t y = v >> 5;   // /32，映射到 0-7（因为高度只有 8）
    
    if (x > 15) return 0;
    
    uint8_t col = pgm_read_byte(&candle_shape[x]);
    // 注意：假设 bit0 是底部（第 7 行），bit7 是顶部（第 0 行）
    // 如果硬件相反，改为 (col >> (7-y)) & 1
    return (col >> y) & 0x01;
}

// ==================== 3. 主接口函数 ====================
/**
 * 生成蜡烛火焰的一帧位图
 * @param bitmap 输出缓冲区，16 字节数组，每字节代表一列（bit0=底行，bit7=顶行）
 * @param sequence 时间序列，每次调用建议 +1（自动循环 0-255）
 * @param turbulence 扰动强度，0=静止，32=正常微风，64=强风
 */
void candleRender(uint8_t *bitmap, uint8_t sequence, uint8_t turbulence) {
    // 交换：f 控制垂直（原水平），g 控制水平（原垂直）
    uint8_t f = perlin_noise(sequence >> 3, sequence);      // 慢变 → 用于上下摇摆
    uint8_t g = perlin_noise(128, sequence << 1);           // 快变 → 用于左右扭动
    
    // g 处理：水平方向绝对值（原垂直噪声的处理方式）
    uint8_t horiz = (g > 128) ? ((g - 128) << 1) : ((128 - g) << 1);
    if (horiz < 20) horiz = 0;  // 中心死区，避免烛身扭动
    
    // 交换后的偏移计算
    // dy：上下摇摆（原 dx 的计算方式，用 f 噪声）
    int8_t dy = ((int16_t)f - 128) * turbulence / 128;
    
    // dx：左右扭动（原 dy 的计算方式，用 g 噪声的变体）
    int8_t dx = -((int16_t)horiz * turbulence / 256);
    
    for (uint8_t col = 0; col < 16; col++) {
        uint8_t byte = 0;
        for (uint8_t row = 0; row < 8; row++) {
            uint8_t u = (col << 4) + 8;
            uint8_t v = (row << 5) + 16;
            
            // 交换应用方式：
            // dy 随高度变化（顶部摇摆大）→ 上下摆动
            int16_t vv = (int16_t)v + (dy * (8 + row) >> 3);
            
            // dx 直接应用（全高度一致）→ 左右扭动
            int16_t uu = (int16_t)u + dx;
            
            if (uu < 0) uu = 0; else if (uu > 255) uu = 255;
            if (vv < 0) vv = 0; else if (vv > 255) vv = 255;
            
            if (candle_sample((uint8_t)uu, (uint8_t)vv)) {
                byte |= (1 << row);
            }
        }
        bitmap[col] = byte;
    }
}
//按键检测

void checkKeys() {
    bool key1State = digitalRead(KEY1_PIN) == LOW;
    bool key2State = digitalRead(KEY2_PIN) == LOW;
    bool key3State = digitalRead(KEY3_PIN) == LOW;
    
    unsigned long currentTime = millis();
    
    // 更新按键状态
    if (key1State) {
        if (!keyStatus.key1Pressed) {
            // KEY1刚按下
            keyStatus.key1Pressed = true;
            keyStatus.key1PressTime = currentTime;
            keyStatus.key1WasPressed = true;
            keyStatus.key1LongPressHandled = false; // 重置长按处理标志
        }
    } else {
        if (keyStatus.key1Pressed) {
            // KEY1刚释放
            if (!keyStatus.key1LongPressHandled && (currentTime - keyStatus.key1PressTime < LONG_PRESS_TIME)) {
                // 短按（只有在没有触发长按的情况下）
                currentAction = KEY1_SHORT_PRESS;
            }
            keyStatus.key1Pressed = false;
            keyStatus.key1WasPressed = false;
        }
    }
    
    // 检查KEY2
    if (key2State) {
        if (!keyStatus.key2Pressed) {
            // KEY2刚按下
            keyStatus.key2Pressed = true;
            keyStatus.key2PressTime = currentTime;
            keyStatus.key2WasPressed = true;
            keyStatus.key2LongPressHandled = false; // 重置长按处理标志
        }
    } else {
        if (keyStatus.key2Pressed) {
            // KEY2刚释放
            if (!keyStatus.key2LongPressHandled && (currentTime - keyStatus.key2PressTime < LONG_PRESS_TIME)) {
                // 短按（只有在没有触发长按的情况下）
                currentAction = KEY2_SHORT_PRESS;
            }
            keyStatus.key2Pressed = false;
            keyStatus.key2WasPressed = false;
        }
    }
    // 检查KEY3
    if (key3State) {
        if (!keyStatus.key3Pressed) {
            // KEY3刚按下
            keyStatus.key3Pressed = true;
            keyStatus.key3PressTime = currentTime;
            keyStatus.key3WasPressed = true;
            keyStatus.key3LongPressHandled = false; // 重置长按处理标志
        }
    } else {
        if (keyStatus.key3Pressed) {
            // KEY3刚释放
            if (!keyStatus.key3LongPressHandled && (currentTime - keyStatus.key3PressTime < LONG_PRESS_TIME)) {
                // 短按（只有在没有触发长按的情况下）
                currentAction = KEY3_SHORT_PRESS;
            }
            keyStatus.key3Pressed = false;
            keyStatus.key3WasPressed = false;
        }
    }
    
    // 检查长按（关键修改：允许同时检测两个按键的长按）
    if (keyStatus.key1Pressed && !keyStatus.key1LongPressHandled && 
        (currentTime - keyStatus.key1PressTime >= LONG_PRESS_TIME)) {
        // 检查是否两个按键都在长按
        if (keyStatus.key2Pressed && !keyStatus.key2LongPressHandled) {
            currentAction = BOTH_LONG_PRESS;
            keyStatus.key1LongPressHandled = true;
            keyStatus.key2LongPressHandled = true;
        } else if (!keyStatus.key2Pressed) {
            // 只有KEY1长按
            currentAction = KEY1_LONG_PRESS;
            keyStatus.key1LongPressHandled = true;
        }
    }
    
    if (keyStatus.key2Pressed && !keyStatus.key2LongPressHandled && 
        (currentTime - keyStatus.key2PressTime >= LONG_PRESS_TIME)) {
        // 检查是否两个按键都在长按
        if (keyStatus.key1Pressed && !keyStatus.key1LongPressHandled) {
            // 这种情况应该已经在上面的KEY1检测中处理过了
            // 但为了完整性，也在这里检查
            if (!keyStatus.key1LongPressHandled) {
                currentAction = BOTH_LONG_PRESS;
                keyStatus.key1LongPressHandled = true;
                keyStatus.key2LongPressHandled = true;
            }
        } else if (!keyStatus.key1Pressed) {
            // 只有KEY2长按
            currentAction = KEY2_LONG_PRESS;
            keyStatus.key2LongPressHandled = true;
        }
    }
}

//按键响应
void changeAction(KeyAction action) {
    switch(action) {
        case KEY1_SHORT_PRESS:
            Serial.println("KEY1 短按");
            if (brightness > 0) brightness--;
            module.setupDisplay(true, brightness); // set intensity lower
            break;
        case KEY2_SHORT_PRESS:
            Serial.println("KEY2 短按");
            if (brightness < 7) brightness++;
            module.setupDisplay(true, brightness); // set intensity lower
            break;
        case KEY3_SHORT_PRESS:
            Serial.println("KEY3 短按");
            switch(currentMode) {
              //case MODE_FLAME:
              //    displayFlame();
              //    break;
              //case MODE_TEMP:
              //    displayTemperature();
              //    break;
              case MODE_SERIAL_DATA:
                
                break;
              case MODE_SNAKE:
                matrix.setAll(true);
                break;
              case MODE_CODE_RAIN:
                
                break;
              
              case MODE_RAIN:
                if (currentDir==RIGHT)currentDir=DOWN;
                else{currentDir = static_cast<RAIN_DIR>(currentDir + 1);}
                break;
              case MODE_RANDOM:
              
              break;  
              case MODE_BOUNCE_PIXEL:
                
                break;
              case MODE_FLY:
                
                break;
              case MODE_OFF:
                matrix.setAll(false);
                break;
              default:
                // 默认显示
                break;
              }
            break;
        case KEY1_LONG_PRESS:
            Serial.println("KEY1 长按");
            if (currentMode==MODE_FLY) currentMode=(DisplayMode)(0);
            else {
              currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
              }
            matrix.setAll(false);
            break;
        case KEY2_LONG_PRESS:
            Serial.println("KEY2 长按");
            if (currentMode==0) currentMode=MODE_FLY;
            else{
            currentMode = (DisplayMode)((currentMode - 1) % MODE_COUNT); // + MODE_COUNT) % MODE_COUNT);
            }
            break;
        case BOTH_LONG_PRESS:
            Serial.println("KEY1 和 KEY2 同时长按");
            ledOn=!ledOn;
            break;
        default:
            break;
    }
}
//选择播放内容
void display()
{
  if (!ledOn) {
    matrix.setAll(false);
    return;
  }
 switch(currentMode) {
        case MODE_FLAME:
            displayFlame();
            break;
        case MODE_TEMP:
            displayTemp();
            break;
        case MODE_SERIAL_DATA:
          displaySerialData();
          break;
        case MODE_SNAKE:
            displaysnake();
            break;
        case MODE_CODE_RAIN:
          displayCodeRain();
          break;
        
        case MODE_RAIN:
          displayRain();
          break;
        
        case MODE_RANDOM:
          displayRandom();
          break;  
        case MODE_BOUNCE_PIXEL:
            displayBouncePixel();
            break;
        case MODE_FLY:
            displayfly();
            break;
        case MODE_OFF:
            matrix.setAll(false);
            break;
        default:
            displayFlame(); // 默认显示
            break;
  }
}
void loop() {
  unsigned long currentTime = millis();
  
  // 每隔一定时间检查按键状态
  if (currentTime - lastCheckTime >= CHECK_INTERVAL) {
      checkKeys();
      lastCheckTime = currentTime;
  }
  
  // 如果有按键动作，通过串口输出
  if (currentAction != NO_PRESS) {
      changeAction(currentAction);
      currentAction = NO_PRESS; // 重置动作状态
  }
  display();
}