// Brian's Brain cellular automata //

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

  uint8_t world[SCR];
  uint8_t new_world[SCR];

int randint(int weight){
  
    int choice = rand()%10;
    if (choice > weight) return 1;
    else return 0;
}
    
void rndrule(){

  for(int y = 0; y < HEIGHT; y++){
    for(int x = 0; x < WIDTH; x++){
           
      int r = randint(DENSITY);
      if (r == 1) world[x+y*WIDTH] = FIRING;
      else world[x+y*WIDTH] = READY;

    }
  }

}

int count_neighbours(uint8_t world[], int x_pos, int y_pos){
  
    int cell;
    int count = 0;

    for(int y = -1; y < 2; y++){
        for(int x = -1; x < 2; x++){
            int cx = x_pos + x;
            int cy = y_pos + y;
            if((0 <= cx && cx < WIDTH) && (0 <= cy && cy < HEIGHT)){
                cell = world[(x_pos + x)+(y_pos + y)*WIDTH];
                if (cell == FIRING) count ++;
            }
        }
    }
  return count;
}


void apply_rules(uint8_t world[]){

  memcpy(new_world, world, sizeof(new_world));

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++){
      
      int cell = new_world[x+y*WIDTH];          
      if(cell == READY){
        int neighbours = count_neighbours(new_world, x, y);
        if (neighbours == 2) world[x+y*WIDTH] = FIRING;
      }
      else if(cell == FIRING) world[x+y*WIDTH] = REFRACTORY;
      else world[x+y*WIDTH] = READY;
      
    }
  }
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

  apply_rules(world);

  for(int y = 0; y < HEIGHT; y++){
    for(int x = 0; x < WIDTH; x++){

      uint16_t image;
        
      if(world[x+y*WIDTH] == FIRING) image = ST7735_WHITE;
      else if(world[x+y*WIDTH] == REFRACTORY) image = ST7735_BLUE;
      else image = ST7735_BLACK;
        
      col[((4*x)+(4*y)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
      col[((4*x)+(4*y)*FULLW)+1] = (uint8_t)(image) & 0xFF;

    }
  }
  
  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

}
