// Ripples (like sand) cellular automata //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

  uint8_t col[2*SCR];
  uint8_t state[WIDTH][HEIGHT];
  uint16_t image;


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

  for(int y = 0; y < HEIGHT; y++){
  
    for(int x = 0; x < WIDTH; x++) state[x][y] = rand()%256;
    
  }
 
}

void loop(){

  for(int interlace = 0; interlace < 2; interlace++){

    for(int j = interlace; j < HEIGHT; j += 2){

      float b = rand()%256;
       
      for(int i = 0; i < WIDTH; i++){

        b -= 2.0f;
        float bj = state[i][j];
        float ba = state[i][(j+HEIGHT-1)%HEIGHT];
        float bb = state[i][(j+HEIGHT+1)%HEIGHT];
        float bja = state[(i+WIDTH+1)%WIDTH][j];
        
        if(b < bj){
          
          b = bj + 2.0f;
          if(bja >= bj || (ba >= bj && bb >= bj)) bj += 2.0f;
          else bj -= 2.0f;

        } else bj = (ba + bb + bja) / 3.0f;
         
        uint8_t coll = state[i][j] = bj;
        image = ST7735_COLOR565(coll, coll, coll);
        col[((2*i)+(2*j)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
        col[((2*i)+(2*j)*WIDTH)+1] = (uint8_t)(image) & 0xFF;
        
      }
    }
  }

  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
