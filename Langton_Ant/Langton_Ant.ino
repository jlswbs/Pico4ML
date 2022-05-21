// Langton's Ant cellular automata //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

#define ITER    35

  uint8_t col[2*SCR];
  uint16_t image;
  bool state[WIDTH][HEIGHT];
  int antLoc[2];
  int antDirection;


void turnLeft(){

  if (antDirection > 1) antDirection--;
  else antDirection = 4;

}

void turnRight(){

  if (antDirection < 4) antDirection++;
  else antDirection = 1;

}

void moveForward(){

  if (antDirection == 1) antLoc[0]--;
  if (antDirection == 2) antLoc[1]++;
  if (antDirection == 3) antLoc[0]++;
  if (antDirection == 4) antLoc[1]--;

  if (antLoc[0] < 0) antLoc[0] = WIDTH-1;
  if (antLoc[0] > WIDTH-1) antLoc[0] = 0;
  if (antLoc[1] < 0) antLoc[1] = HEIGHT-1;
  if (antLoc[1] > HEIGHT-1) antLoc[1] = 0;

}

void updateScene(){

  moveForward();

  if (state[antLoc[0]][antLoc[1]] == 0){
    state[antLoc[0]][antLoc[1]] = 1;
    turnRight();
  } else {
    state[antLoc[0]][antLoc[1]] = 0;
    turnLeft();
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

  antDirection = 1 + rand()%4;
  antLoc[0] = rand()%WIDTH;
  antLoc[1] = rand()%HEIGHT;
  
}

void loop(){

  for(int y = 0; y < HEIGHT; y++){

    for(int x = 0; x < WIDTH; x++) {
    
      if(state[x][y] == 1) image = ST7735_WHITE;
      else image = ST7735_BLACK;
      
      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;
  
    }
      
  }

  for(int i = 0; i < ITER; i++) updateScene();
  
  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
