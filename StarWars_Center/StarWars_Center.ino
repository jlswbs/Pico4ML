// Star-Wars cellular automata - centered //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

  uint8_t col[2*SCR];
  
#define ALIVE     3
#define DEATH_1   2
#define DEATH_2   1
#define DEAD      0

  uint8_t current[SCR];
  uint8_t next[SCR];
  uint8_t alive_counts[SCR];
  uint8_t swap[SCR];
  uint8_t gridpix;
    
void rndrule(){

  memset(col, 0, sizeof(col));

  gridpix = rand() % (WIDTH/2);

  for (int y = (HEIGHT/2)-gridpix; y < (HEIGHT/2)+gridpix; y=y+2) {
  
    for (int x = (WIDTH/2)-gridpix; x < (WIDTH/2)+gridpix; x=x+2) current[x+y*WIDTH] = ALIVE;
 
  } 

}

void step(){

  for(int y = 0; y < HEIGHT; y++){
  
    for(int x = 0; x < WIDTH; x++){
    
      int count = 0;
      int next_val;   
      int mx = WIDTH-1;
      int my = HEIGHT-1;    
      int self = current[x+y*WIDTH];
    
      for (int nx = x-1; nx <= x+1; nx++) { 
        for (int ny = y-1; ny <= y+1; ny++) { 
          if (nx == x && ny == y) continue;    
          if (current[wrap(nx, mx) + wrap(ny, my) * WIDTH] == ALIVE) count++;    
        }   
      }  

      int neighbors = count;
      
      if (self == ALIVE) next_val = ((neighbors == 3) || (neighbors == 4) || (neighbors == 5)) ? ALIVE : DEATH_1;
      else if (self == DEAD) next_val = (neighbors == 2) ? ALIVE : DEAD;
      else next_val = self-1;
   
      next[x+y*WIDTH] = next_val;
  
      if (next_val == ALIVE) alive_counts[x+y*WIDTH] = min(alive_counts[x+y*WIDTH]+1, 100);
      else if (next_val == DEAD) alive_counts[x+y*WIDTH] = 0;
    }
  }
    
  memcpy(swap, current, sizeof(swap));
  memcpy(current, next, sizeof(current));
  memcpy(next, swap, sizeof(next));

}
  
int wrap(int v, int m){

  if (v < 0) return v + m;
  else if (v >= m) return v - m;
  else return v;

}

void draw_type(int min_alive, int max_alive, uint16_t color){

  uint16_t coll;

  for(int y = 0; y < HEIGHT; y++){
    
    for(int x = 0; x < WIDTH; x++){
      
      int self = current[x+y*WIDTH];
      if (self == DEAD) continue;
      int alive = alive_counts[x+y*WIDTH];
      if (alive < min_alive || alive > max_alive) continue;
      if (self == ALIVE) coll = color;
      else if (self == DEATH_1) coll = color<<2;
      else if (self == DEATH_2) coll = ST7735_BLACK;
      
      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(coll >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(coll) & 0xFF;
   
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

  step();
  
  draw_type(50, 100, ST7735_RED);
  draw_type(2, 49, ST7735_BLUE);
  draw_type(0, 1, ST7735_WHITE);
  
  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
