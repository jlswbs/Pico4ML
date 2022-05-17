// Diffusion limited aggregation //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

  uint8_t col[2*SCR];
  bool world[SCR];
  bool value;
  uint16_t image;
  int x, y;


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

  x = rand()%WIDTH;
  y = rand()%HEIGHT;
  value = true;
  world[x+y*WIDTH] = value;
  
}

void loop(){

  for(int i=0;i<50;i++){

    x = rand()%WIDTH;
    y = rand()%HEIGHT;
    
    while(x>1 && x<WIDTH-1 && y>1 && y<HEIGHT-1){
    
      int move = rand()%4;
      
      if (move == 0) x++;
      else if (move == 1) x--;
      else if (move == 2) y++;
      else if (move == 3) y--;

      if(world[y*WIDTH+(x-1)] == value || world[y*WIDTH+(x+1)] == value || world[(y+1)*WIDTH+x] == value || world[(y-1)*WIDTH+x] == value) {
    
        world[y*WIDTH+x] = value;
        break;

      }   
    }   
  }
  
  for(int y = 0; y < HEIGHT; y++){
       
    for(int x = 0; x < WIDTH; x++){
 
      if(world[y*WIDTH+x] == true) image = ST7735_WHITE;
      else image = ST7735_BLACK;
      
      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;

    }
  }
  
  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
