// XOR fractal //

#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

  uint8_t col[2*SCR];

  long t;

void setup() {

  ST7735_Init();
  ST7735_FillScreen(ST7735_BLACK);
 
}

void loop(){

  for (int y = 0; y < HEIGHT; y++){
    for (int x = 0; x < WIDTH; x++){

      uint8_t nx = (x + t);
      uint8_t ny = (y + t);    
      uint8_t coll1 = (nx ^ ny) * t;
      uint8_t coll2 = (nx & ny) * t;
      uint8_t coll3 = (nx | ny) * t;
      uint16_t image = ST7735_COLOR565(coll1, coll2, coll3);
      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;

    }
  }

  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

  t++;

  delay(33);

}
