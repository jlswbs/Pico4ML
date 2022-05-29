// Super Langton's Ant cellular automata //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

#define LENGHT  32
#define ITER    250

  uint8_t col[2*SCR];
  uint16_t state[WIDTH][HEIGHT];
  uint16_t antX = WIDTH/2;
  uint16_t antY = HEIGHT/2;
  uint16_t direction;
  uint16_t stateCount;
  bool type[LENGHT];
  uint16_t stateCols[LENGHT];


void rndrule(){

  antX = WIDTH/2;
  antY = HEIGHT/2;

  stateCount = 2 + rand()%(LENGHT-2);
  direction = rand()%4;

  for(int i = 0; i < stateCount; i++) type[i] = rand()%2;
  for(int i = 0; i < stateCount; i++) stateCols[i] = rand();

}

void turn(int angle){
  
  if(angle == 0){
    if(direction == 0){
      direction = 3;
    } else {
      direction--;
    }
  } else {
    if(direction == 3){
      direction = 0;
    } else {
      direction++;
    }
  }
}

void move(){
  
  if(antY == 0 && direction == 0){
    antY = HEIGHT-1;
  } else {
    if(direction == 0 ){
      antY--;
    }
  }
  if(antX == WIDTH-1 && direction == 1){
    antX = 0;
  } else {
    if(direction == 1){
      antX++;
    }
  }
  if(antY == HEIGHT-1 && direction == 2){
   antY = 0; 
  } else {
    if(direction == 2){
      antY++;
    }
  }
  if(antX == 0 && direction == 3){
    antX = WIDTH-1;
  } else {
    if(direction == 3){
      antX--;
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

  for(int i = 0; i < ITER; i++) {

    move();
    
    turn(type[(state[antX][antY]%stateCount)]);
    state[antX][antY]++;
    
    uint16_t image = stateCols[(state[antX][antY]%stateCount)];
    col[((2*antX)+(2*antY)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
    col[((2*antX)+(2*antY)*WIDTH)+1] = (uint8_t)(image) & 0xFF;

  }
  
  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
