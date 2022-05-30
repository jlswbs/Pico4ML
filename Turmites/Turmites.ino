// Turmites cellular automata //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

#define ITER    100

  uint8_t col[2*SCR];
  uint16_t image;
  int x, y, posx, posy;
  int state;
  int dir;
  uint8_t world[WIDTH][HEIGHT];
  int current_col;
  int next_col[4][4];
  int next_state[4][4];
  int directions[4][4];

void rndrule(){

  state = rand()%4;
  dir = 0;
  posx = WIDTH/2;
  posy = HEIGHT/2;
  
  for(int j=0; j<4; j++){   
    for(int i=0; i<4; i++){         
      next_col[i][j] = rand()%4;
      next_state[i][j] = rand()%4;
      directions[i][j] = rand()%8;
    }   
  } 

  world[posx][posy] = rand()%4;

}

void move_turmite(){
  
  int cols = world[posx][posy];
  
  x = posx;
  y = posy;
  current_col = next_col[cols][state];
  world[posx][posy] = next_col[cols][state];
  state = next_state[cols][state];    

  dir = (dir + directions[cols][state]) % 8;

  switch(dir){
    case 0: posy--; break;
    case 1: posy--; posx++; break;
    case 2: posx++; break;
    case 3: posx++; posy++; break;
    case 4: posy++; break;
    case 5: posy++; posx--; break;
    case 6: posx--; break;
    case 7: posx--; posy--; break;
  }

  if(posy < 0) posy = HEIGHT-1;
  if(posy >= HEIGHT) posy = 0;
  if(posx < 0) posx = WIDTH-1;
  if(posx >= WIDTH) posx=0;
  
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

  for(int i = 0; i < ITER; i++) {

    move_turmite();

    switch(current_col){
      case 0: image = ST7735_RED; break;
      case 1: image = ST7735_GREEN; break;
      case 2: image = ST7735_BLUE; break;
      case 3: image = ST7735_WHITE; break;
    }
    
    col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
    col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;

  }
  
  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
