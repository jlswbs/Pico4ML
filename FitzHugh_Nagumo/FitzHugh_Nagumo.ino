// FitzHugh-Nagumo reaction diffusion //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   40
#define HEIGHT  80
#define FULLW   80
#define FULLH   160
#define SCR     (WIDTH*HEIGHT)
#define SCR2    (FULLW*FULLH)

float randomf(float minf, float maxf) {return minf + (rand()%(1UL << 31))*(maxf - minf) / (1UL << 31);} 
  
  uint8_t col[2*SCR2];
  
  float reactionRate = 0.2f;
  float diffusionRateV = 0.01f;
  float diffusionRateU = 0.04f;
  float diffusionRateUX = 0.04f;
  float diffusionRateUY = 0.03f;
  float deltaT = 4.0f;
  
  float gridU[WIDTH][HEIGHT];
  float gridV[WIDTH][HEIGHT];
  float gridNext[WIDTH][HEIGHT];
  float diffRateUYarr[WIDTH][HEIGHT];
  float diffRateUXarr[WIDTH][HEIGHT];
  float Farr[WIDTH][HEIGHT];
  float karr[WIDTH][HEIGHT];
  float temp[WIDTH][HEIGHT];
    
void rndrule(){

  diffusionRateV = randomf(0.01f, 0.05f);
  diffusionRateU = randomf(0.01f, 0.05f);
  diffusionRateUX = randomf(0.01f, 0.09f);
  diffusionRateUY = randomf(0.01f, 0.09f);

  for(int i=0;i<WIDTH;++i) {
    
    for(int j=0;j<HEIGHT;++j) {
      
      gridU[i][j] = 0.5f + randomf(-0.01f, 0.01f);
      gridV[i][j] = 0.25f + randomf(-0.01f, 0.01f);
        
    }
  }

  setupF();
  setupK();
  setupDiffRates();

}

void diffusionU(){
  
  for(int i=0;i<WIDTH;++i){
    for(int j=0;j<HEIGHT;++j){

      gridNext[i][j] = gridU[i][j]+deltaT*
        (diffRateUXarr[i][j]*(gridU[(i-1+WIDTH)%WIDTH][j]
        +gridU[(i+1)%WIDTH][j]-2*gridU[i][j])+
        diffRateUYarr[i][j]*(gridU[i][(j-1+HEIGHT)%HEIGHT]
        +gridU[i][(j+1)%HEIGHT]
        -2*gridU[i][j]));
    }
  }
  
  for(int i=0;i<WIDTH;++i){
    for(int j=0;j<HEIGHT;++j){
      temp[i][j] = gridU[i][j];
      gridU[i][j] = gridNext[i][j];
      gridNext[i][j] = temp[i][j];
    }
  }
  
}


void diffusionV(){
  
  for(int i=0;i<WIDTH;++i){
    for(int j=0;j<HEIGHT;++j){

      gridNext[i][j] = gridV[i][j]+diffusionRateV*deltaT*
        (gridV[(i-1+WIDTH)%WIDTH][j]
        +gridV[(i+1)%WIDTH][j]
        +gridV[i][(j-1+HEIGHT)%HEIGHT]
        +gridV[i][(j+1)%HEIGHT]
        -4*gridV[i][j]
        );
    }
  }
  
  for(int i=0;i<WIDTH;++i){
    for(int j=0;j<HEIGHT;++j){
      
      temp[i][j] = gridV[i][j];
      gridV[i][j] = gridNext[i][j];
      gridNext[i][j] = temp[i][j];
      
    }
  }
}


void reaction(){

  for(int i=0;i<WIDTH;++i){
  
    for(int j=0;j<HEIGHT;++j){

      gridU[i][j] = gridU[i][j] + deltaT*(reactionRate*(gridU[i][j]-gridU[i][j]*gridU[i][j]*gridU[i][j]-gridV[i][j]+karr[i][j]));
      gridV[i][j] = gridV[i][j] + deltaT*(reactionRate*Farr[i][j]*(gridU[i][j]-gridV[i][j]));
    
    }
  }
}


void setupF(){
  
  for(int i=0;i<WIDTH;++i){
  
    for(int j=0;j<HEIGHT;++j) Farr[i][j] = 0.01f + i * 0.09f / WIDTH;
  
  }
}


void setupK(){

  for(int i=0;i<WIDTH;++i){
  
    for(int j=0;j<HEIGHT;++j) karr[i][j] = 0.06f + j * 0.4f / HEIGHT;
    
  } 
}


void setupDiffRates(){
  
  for(int i=0;i<WIDTH;++i){
  
    for(int j=0;j<HEIGHT;++j){
  
      diffRateUYarr[i][j] = 0.03f + j * 0.04f / HEIGHT;
      diffRateUXarr[i][j] = 0.03f + j * 0.02f / HEIGHT;
    
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

void loop() {

  diffusionU();
  diffusionV();
  reaction();
  
  for(int x=0;x<WIDTH;++x){
    for(int y=0;y<HEIGHT;++y){
    
      uint8_t coll = 255.0f * gridU[x][y];
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
      col[((4*x)+(4*y)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
      col[((4*x)+(4*y)*FULLW)+1] = (uint8_t)(image) & 0xFF;
        
    }
  }

  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

}
