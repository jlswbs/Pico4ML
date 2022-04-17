// Munching squares //

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

      uint8_t nx = (x + t) % WIDTH;
      uint8_t ny = (y + t) % WIDTH;
      
      uint8_t coll = (nx ^ ny);
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;

    }
  }

  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

  t++;

}
