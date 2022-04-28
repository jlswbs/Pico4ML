// Turing pattern by cellular automata //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

float randomf(float minf, float maxf) {return minf + (rand()%(1UL << 31))*(maxf - minf) / (1UL << 31);}  

  uint8_t col[2*SCR];

  uint16_t image;
  bool cells[WIDTH][HEIGHT]; 
  int innerNeighbor = 2;
  int outerNeighbor = 4;
  float w;


void rndrule(){

  w = randomf(0.24f, 0.77f);

  for (int y = 0; y < HEIGHT; y++){
    
    for (int x = 0; x < WIDTH; x++){
      
      if(rand()%10 < 1) cells[x][y] = 1;
      else cells[x][y] = 0;

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

  for (int y = 0; y < HEIGHT; y++){
    
    for (int x = 0; x < WIDTH; x++){
    
      if(cells[x][y] == 1) image = ST7735_WHITE;
      else image = ST7735_BLACK;
      
      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;
      
    }
    
  }

   bool nextCells[WIDTH][HEIGHT];
   
   for(int y = 0; y < HEIGHT; y++){
  
      for(int x = 0; x < WIDTH; x++){
   
        int innerSum = 0;
        int outerSum = 0;
        
        for(int dx = - outerNeighbor; dx <= outerNeighbor; dx++){
          
          for(int dy = - outerNeighbor; dy <= outerNeighbor; dy++){
          
            int distance = abs(dx) + abs(dy);
            if(distance > outerNeighbor || distance == 0)continue;
          
            int nx = x + dx;
            int ny = y + dy;
           
            if(nx < 0 || WIDTH <= nx || ny < 0 || HEIGHT <= ny) continue;  
            if(distance <= innerNeighbor) innerSum += cells[nx][ny];
            if(distance <= outerNeighbor) outerSum += cells[nx][ny];

        }
              
      }
      
      if(innerSum > w * outerSum) nextCells[x][y] = 1;
      else if(innerSum < w * outerSum) nextCells[x][y] = 0;      
      else nextCells[x][y] = cells[x][y];      

    }
    
  }

  for(int y = 0; y < HEIGHT; y++){
  
    for(int x = 0; x < WIDTH; x++) cells[x][y] = nextCells[x][y];

  }

  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
