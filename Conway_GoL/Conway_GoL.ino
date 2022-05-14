// Conway's Game of Life //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   40
#define HEIGHT  80
#define FULLW   80
#define FULLH   160
#define SCR     (WIDTH*HEIGHT)
#define SCR2    (FULLW*FULLH)

  uint8_t col[2*SCR2];
  
#define DENSITY     7
#define READY       0
#define REFRACTORY  1
#define FIRING      2

  bool grid[SCR];
  bool newgrid[SCR];
    
void rndrule(){

  for (int y = 0; y < HEIGHT; y++){
    for (int x = 0; x < WIDTH; x++){     
      if (x == 0 || x == WIDTH - 1 || y == 0 || y == HEIGHT - 1) grid[x+y*WIDTH] = 0;
      else {
        if (rand()%3 == 1) grid[x+y*WIDTH] = 1;
        else grid[x+y*WIDTH] = 0;
      }
    }
  }

}

void computeCA() {
  
  for (int y = 0; y < HEIGHT; y++){
    
    for (int x = 0; x < WIDTH; x++){
      
      int neighbors = getNumberOfNeighbors(x, y);
      
      if (grid[x+y*WIDTH] == 1 && (neighbors == 2 || neighbors == 3 )) newgrid[x+y*WIDTH] = 1;
      else if (grid[x+y*WIDTH] == 1)  newgrid[x+y*WIDTH] = 0;
      
      if (grid[x+y*WIDTH] == 0 && (neighbors == 3)) newgrid[x+y*WIDTH] = 1;
      else if (grid[x+y*WIDTH] == 0) newgrid[x+y*WIDTH] = 0;
    
    }
  }
}

uint8_t getNumberOfNeighbors(int x, int y){
  
  return grid[(x-1)+y*WIDTH] + grid[(x-1)+(y-1)*WIDTH] + grid[x+(y-1)*WIDTH] + grid[(x+1)+(y-1)*WIDTH] + grid[(x+1)+y*WIDTH] + grid[(x+1)+(y+1)*WIDTH] + grid[x+(y+1)*WIDTH] + grid[(x-1)+(y+1)*WIDTH];

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

  rndrule();
  
}

void loop(){

  computeCA();

  for(int y = 0; y < HEIGHT; y++){

    for(int x = 0; x < WIDTH; x++){

      uint16_t image;
      
      if((grid[x+y*WIDTH]) != (newgrid[x+y*WIDTH])){
        
        if (newgrid[x+y*WIDTH] == 1) image = ST7735_WHITE;
        else image = ST7735_BLACK;

        col[((4*x)+(4*y)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
        col[((4*x)+(4*y)*FULLW)+1] = (uint8_t)(image) & 0xFF;

      }

    }
  }

  memcpy(grid, newgrid, SCR);
  
  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

}
