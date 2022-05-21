// Multi Langton's Ant cellular automata //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

#define NUMANTS 4
#define ITER    25

  uint8_t col[2*SCR];
  uint16_t world[SCR];
  uint16_t coll[NUMANTS];
  int x[NUMANTS];
  int y[NUMANTS];
  int antsdir[NUMANTS];
  

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

  for(int i = 0; i < NUMANTS; i++){
  
    x[i] = rand()%WIDTH;
    y[i] = rand()%HEIGHT;
    antsdir[i] = rand()%4;
    coll[i] = rand();
    
  }
  
}

void loop(){

  for(int k = 0; k < ITER; k++){
  
    for(int i = 0; i < NUMANTS; i++){
    
      if (world[x[i]+WIDTH*y[i]] > ST7735_BLACK){ antsdir[i] = antsdir[i] - 1; world[x[i]+WIDTH*y[i]] = ST7735_BLACK; }
      else { antsdir[i] = antsdir[i] + 1; world[x[i]+WIDTH*y[i]] = coll[i]; }

      if (antsdir[i] > 3) antsdir[i] = 0;   
      if (antsdir[i] < 0) antsdir[i] = 3;   
    
      if (antsdir[i] == 0) x[i] = x[i] - 1;
      if (antsdir[i] == 1) y[i] = y[i] + 1;
      if (antsdir[i] == 2) x[i] = x[i] + 1;
      if (antsdir[i] == 3) y[i] = y[i] - 1;
    
      if (x[i] > WIDTH-1) x[i] = 0;
      if (x[i] < 0) x[i] = WIDTH-1;
      if (y[i] > HEIGHT-1) y[i] = 0;
      if (y[i] < 0) y[i] = HEIGHT-1;
    
    }
  }

  for(int y = 0; y < HEIGHT; y++){

    for(int x = 0; x < WIDTH; x++) {
    
      uint16_t image = world[x+WIDTH*y];
      
      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;
  
    }
      
  }
  
  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
