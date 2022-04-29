// Turing pattern by cellular automata //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

float randomf(float minf, float maxf) {return minf + (rand()%(1UL << 31))*(maxf - minf) / (1UL << 31);}  

  uint8_t col[2*SCR];

  uint16_t image;
  bool state[WIDTH][HEIGHT];
  bool next[WIDTH][HEIGHT];  
  int Ra = 1;
  int Ri = 5;
  float Wa = 1.0f;
  float Wi = 0.1f;


void rndrule(){

  Wa = randomf(0.799f, 1.499f);

  for (int y = 0; y < HEIGHT; y++){
    
    for (int x = 0; x < WIDTH; x++) state[x][y] = rand()%2;

  }

}

int edgex(int a){
  
  if(a < 0) return WIDTH + a;
  else if(a >= WIDTH)return a - WIDTH;
  else return a;

}

int edgey(int a){
  
  if(a < 0) return HEIGHT + a;
  else if(a >= HEIGHT)return a - HEIGHT;
  else return a;

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
   
      int count_activator = 0;
        
      for(int vi = -Ra; vi < Ra+1; vi++){
        
        for(int vj = -Ra; vj < Ra+1; vj++) count_activator += state[edgex(x + vi)][edgey(y + vj)];
      
      }
      
      int count_inhibitor = 0;
      
      for(int vi = -Ri; vi < Ri+1; vi++){
        
        for(int vj = -Ri; vj < Ri+1; vj++) count_inhibitor += state[edgex(x + vi)][edgey(y + vj)];

      }
      
      float a = Wa * count_activator - Wi * count_inhibitor;
      
      if(a > 0)next[x][y] = 1;
      else next[x][y] = 0;

    }
    
  }

  for(int y = 0; y < HEIGHT; y++){
  
    for(int x = 0; x < WIDTH; x++) state[x][y] = next[x][y];

  }

  for (int y = 0; y < HEIGHT; y++){
    
    for (int x = 0; x < WIDTH; x++){
    
      if(state[x][y] == 1) image = ST7735_WHITE;
      else image = ST7735_BLACK;
      
      col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;
      
    }
    
  }

  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
