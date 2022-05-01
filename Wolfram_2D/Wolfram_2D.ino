// Wolfram 2D cellular automata // 

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

  uint8_t col[2*SCR];

  bool state[SCR];
  bool newst[SCR];
  bool rules[10] = {0,0,1,1,1,1,0,0,0,0};
  uint16_t color[10];


void rndrule(){

  for(int i = 0; i < 10; i++) rules[i] = rand()%2;
  for(int i = 0; i < 10; i++) color[i] = rand()%ST7735_WHITE;

  state[(WIDTH/2)+(HEIGHT/2)*WIDTH] = 1;
  state[(WIDTH/2)+((HEIGHT/2)-1)*WIDTH] = 1;
  state[((WIDTH/2)-1)+((HEIGHT/2)-1)*WIDTH] = 1;
  state[((WIDTH/2)-1)+(HEIGHT/2)*WIDTH] = 1;

}

uint8_t neighbors(uint8_t x, uint8_t y){
  
  uint8_t result = 0;

  if(y > 0 && state[x+(y-1)*WIDTH] == 1) result = result + 1;
  if(x > 0 && state[(x-1)+y*WIDTH] == 1) result = result + 1;
  if(x < WIDTH-1 && state[(x+1)+y*WIDTH] == 1) result = result + 1;
  if(y < HEIGHT-1 && state[x+(y+1)*WIDTH] == 1) result = result + 1;
  
  return result;
 
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

  for(int y = 0; y < HEIGHT; y++){
    
    for(int x = 0; x < WIDTH; x++){
           
      uint8_t totalNeighbors = neighbors(x,y);
      uint16_t image;

      if(state[x+y*WIDTH] == 0 && totalNeighbors == 0)      {newst[x+y*WIDTH] = rules[0]; image = color[0];}
      else if(state[x+y*WIDTH] == 1 && totalNeighbors == 0) {newst[x+y*WIDTH] = rules[1]; image = color[1];}
      else if(state[x+y*WIDTH] == 0 && totalNeighbors == 1) {newst[x+y*WIDTH] = rules[2]; image = color[2];}
      else if(state[x+y*WIDTH] == 1 && totalNeighbors == 1) {newst[x+y*WIDTH] = rules[3]; image = color[3];}
      else if(state[x+y*WIDTH] == 0 && totalNeighbors == 2) {newst[x+y*WIDTH] = rules[4]; image = color[4];}
      else if(state[x+y*WIDTH] == 1 && totalNeighbors == 2) {newst[x+y*WIDTH] = rules[5]; image = color[5];}
      else if(state[x+y*WIDTH] == 0 && totalNeighbors == 3) {newst[x+y*WIDTH] = rules[6]; image = color[6];}
      else if(state[x+y*WIDTH] == 1 && totalNeighbors == 3) {newst[x+y*WIDTH] = rules[7]; image = color[7];}
      else if(state[x+y*WIDTH] == 0 && totalNeighbors == 4) {newst[x+y*WIDTH] = rules[8]; image = color[8];}
      else if(state[x+y*WIDTH] == 1 && totalNeighbors == 4) {newst[x+y*WIDTH] = rules[9]; image = color[9];}

      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;
      
    }
    
  }
 
  memcpy(state, newst, sizeof(state));

  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
