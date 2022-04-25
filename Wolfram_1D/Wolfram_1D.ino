// Wolfram 1D cellular automata // 

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

  uint8_t col[2*SCR];

  bool state[HEIGHT]; 
  bool newstate[HEIGHT];
  bool rules[8] = {0, 1, 1, 1, 1, 0, 0, 0};


void rndrule(){

  for (int i=0;i<8;i++) rules[i] = rand()%2;
  for (int i=0;i<HEIGHT;i++) state[i] = rand()%2;

}

static inline void seed_random_from_rosc(){
  
  uint32_t random = 0;
  uint32_t random_bit;
  volatile uint32_t *rnd_reg = (uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);

  for (int k = 0; k < 32; k++) {
    while (1) {
      random_bit = (*rnd_reg) & 1;
      if (random_bit != ((*rnd_reg) & 1)) break;
    }

    random = (random << 1) | random_bit;
  }

  srand(random);
}

void setup() {
  
  seed_random_from_rosc();

  ST7735_Init();
  ST7735_FillScreen(ST7735_BLACK);
 
}

void loop(){
  
  rndrule();

  for(int x=0;x<WIDTH; x++) {

    memset (newstate, 0, sizeof(newstate));
    
    for(int y=0;y<HEIGHT; y++) {
      
      int k = 4*state[(y-1+HEIGHT)%HEIGHT] + 2*state[y] + state[(y+1)%HEIGHT];
      newstate[y] = rules[k];
 
    }

    memcpy (state, newstate, sizeof(state));

    for(int y=0;y<HEIGHT; y++) {

      uint16_t image;
      if (state[y] == true) image = ST7735_WHITE;       
      else image = ST7735_BLACK;
      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;
      
    }

  }

  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

  delay(500);

}
