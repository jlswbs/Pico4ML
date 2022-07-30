// Physarum growth //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     WIDTH*HEIGHT

  uint8_t col[2*SCR];

#define ITER  2000
#define NUM   8

  uint16_t grid[WIDTH][HEIGHT]; 
  uint16_t coll[NUM];
  uint16_t image;
  int t, q;

void rndseed(){

  for (int i = 0; i < NUM; i++) coll[i] = rand();

  for (int y = 0; y < HEIGHT; y++){
    for (int x = 0; x < WIDTH; x++){
      
      if(x == 0 || x == 1 || x == WIDTH-2 || x == WIDTH-1 || y == 0 || y == 1 || y == HEIGHT-2 || y == HEIGHT-1) grid[x][y] = 1;
      else grid[x][y] = 0;

    }
  }
  
  for (int i = 1; i < NUM; i++){
    
    int x = 2 * (5 + rand()%(WIDTH/2)-5);
    int y = 2 * (5 + rand()%(HEIGHT/2)-5);
    if(grid[x][y] == 0) grid[x][y] = 1000+(i*100);

  }

}

void nextstep(){

  for (int i = 0; i < ITER; i++){
  
    int x = 2 * (1 + rand()%(WIDTH/2)-1);
    int y = 2 * (1 + rand()%(HEIGHT/2)-1);
    
    if(grid[x][y] >= 100 && grid[x][y] < 1000){
      
      q = grid[x][y]/100;
      int p = grid[x][y] - (q*100);
      
      if(p < 30){
        
        t = 1 + rand()%5;
        if(t == 1 && grid[x+2][y] == 0){ grid[x+2][y] = q*100; grid[x+1][y] = q*100; } 
        if(t == 2 && grid[x][y+2] == 0){ grid[x][y+2] = q*100; grid[x][y+1] = q*100; } 
        if(t == 3 && grid[x-2][y] == 0){ grid[x-2][y] = q*100; grid[x-1][y] = q*100; } 
        if(t == 4 && grid[x][y-2] == 0){ grid[x][y-2] = q*100; grid[x][y-1] = q*100; } 
        grid[x][y] = grid[x][y] + 1;
        
      } else {
        
        t = 0;
        if(grid[x+1][y] > 1) t = t + 1;
        if(grid[x][y+1] > 1) t = t + 1;
        if(grid[x-1][y] > 1) t = t + 1;
        if(grid[x][y-1] > 1) t = t + 1;
        if(t <= 1){
          grid[x][y] = 9100;
          grid[x+1][y] = 0;
          grid[x][y+1] = 0;
          grid[x-1][y] = 0;
          grid[x][y-1] = 0; 
        }
      }      
    }
    
    if(grid[x][y] >= 1000 && grid[x][y] < 2000){
      
      q = (grid[x][y]/100)-10;
      if(grid[x+2][y] == 0){ grid[x+2][y] = q*100; grid[x+1][y] = q*100; }
      if(grid[x][y+2] == 0){ grid[x][y+2] = q*100; grid[x][y+1] = q*100; }
      if(grid[x-2][y] == 0){ grid[x-2][y] = q*100; grid[x-1][y] = q*100; }
      if(grid[x][y-2] == 0){ grid[x][y-2] = q*100; grid[x][y-1] = q*100; }
    
    }
    
    if(grid[x][y] >= 9000){
      
      grid[x][y] = grid[x][y] - 1;
      if(grid[x][y] < 9000) grid[x][y] = 0;
    
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

  rndseed();
 
}

void loop(){

  nextstep();

  for (int y = 0; y < HEIGHT; y++){   
    for (int x = 0; x < WIDTH; x++){
    
      if(grid[x][y] >= 100 && grid[x][y] < 1000){
        q = (grid[x][y]/100)%NUM;
        image = coll[q];    
      } else image = ST7735_BLACK;
      
      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;
      
    }
  }

  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
