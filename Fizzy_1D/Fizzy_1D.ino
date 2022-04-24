// Fizzy 1D cellular automata //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

  uint8_t col[2*SCR];

  int Calm = 233;
  int CellIndex = 0;
  float CellVal[HEIGHT];

void rndrule(){

  Calm = 16 + rand()%233;
  for (int i = 0; i < HEIGHT; i++) CellVal[i] = rand()%128;

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

  for (int x = 0; x < WIDTH; x++) {

    for (int y = 0; y < HEIGHT; y++) {
    
      CellIndex = (CellIndex+1)%HEIGHT;

      uint8_t coll = 4.7f * CellVal[CellIndex];
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;

      int below      = (CellIndex+1)%HEIGHT;
      int above      = (CellIndex+HEIGHT-1)%HEIGHT;
      int left       = (CellIndex+HEIGHT-1)%HEIGHT;
      int right      = (CellIndex+1)%HEIGHT;
      int aboveright = ((CellIndex-1) + 1 + HEIGHT)%HEIGHT;
      int aboveleft  = ((CellIndex-1) - 1 + HEIGHT)%HEIGHT;
      int belowright = ((CellIndex+1) + 1 + HEIGHT)%HEIGHT;
      int belowleft  = ((CellIndex+1) - 1 + HEIGHT)%HEIGHT;

      float NeighbourMix = powf((CellVal[left]*CellVal[right]*CellVal[above]*CellVal[below]*CellVal[belowleft]*CellVal[belowright]*CellVal[aboveleft]*CellVal[aboveright]),0.125f);
      CellVal[CellIndex] = fmod(sqrtf(CellVal[CellIndex]*NeighbourMix)+0.5f, Calm);
      
    }

  }

  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
