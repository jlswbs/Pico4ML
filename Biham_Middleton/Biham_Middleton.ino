// Biham-Middleton-Levine traffic model //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

  uint8_t col[2*SCR];
  uint16_t image;

  bool one = true;
  bool two = false;
  bool twoorone = one;
  int percent = 35;
  uint8_t statesone[] = {0,0,0,0,1,1,2,2,2,1,1,1,0,1,1,2,2,2,0,0,0,0,1,1,2,2,2,};
  uint8_t statestwo[] = {0,0,0,1,1,1,0,2,2,0,0,0,1,1,1,0,2,2,2,2,2,1,1,1,0,2,2,};
  uint16_t worldx[WIDTH][HEIGHT];
  uint16_t worldy[WIDTH][HEIGHT];


void rndrule(){

  int n, xn, yn;

  percent = 25 + rand()%50;
  
  for(int j=0;j<HEIGHT;j++){
    
    for(int i=0;i<WIDTH;i++) worldx[i][j] = 0;

  }
  
  for(int k=0;k<percent*SCR/200;k++){
    
    do{ n = rand()%SCR; }   
    while(worldx[n/HEIGHT][n%HEIGHT] != 0);
    
    worldx[n/HEIGHT][n%HEIGHT] = 1;
    
    do{ n = rand()%SCR;}
    while(worldx[n/HEIGHT][n%HEIGHT] !=0 );
    
    worldx[n/HEIGHT][n%HEIGHT] = 2;
    
  }

}

void trafficroll(bool oneortwo){

  int a,b,c;
  
  if(oneortwo==one){
    
    for(int j=0;j<HEIGHT;j++){
      
      for(int i=0;i<WIDTH;i++){
        
        a = worldx[(i-1+WIDTH)%WIDTH][j];
        b = worldx[i][j];
        c = worldx[(i+1)%WIDTH][j];
        worldy[i][j] = statesone[c+3*b+9*a];
      
      }
    
    }
  
  } else {
    
    for(int j=0;j<HEIGHT;j++){
      
      for(int i=0;i<WIDTH;i++){
        
        a = worldy[i][(j-1+HEIGHT)%HEIGHT];
        b = worldy[i][j];
        c = worldy[i][(j+1)%HEIGHT];
        worldx[i][j] = statestwo[c+3*b+9*a];
      
      }
    }
  
  }

  return;
  
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

  trafficroll(twoorone?one:two);
  
  for(int j=0;j<HEIGHT;j++){
    
    for(int i=0;i<WIDTH;i++){
      
      if(twoorone) image = worldy[i][j] == 0 ? ST7735_WHITE:(worldy[i][j] == 1 ? ST7735_BLUE:ST7735_RED);
      else image = worldx[i][j] == 0 ? ST7735_WHITE:(worldy[i][j] == 1 ? ST7735_BLUE:ST7735_RED);
      
      col[((2*i)+(2*j)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*i)+(2*j)*WIDTH)+1] = (uint8_t)(image) & 0xFF;

    }

  }
  
  twoorone=!twoorone;
  
  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
