// Turing patterns //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   40
#define HEIGHT  80
#define FULLW   80
#define FULLH   160
#define SCR     (WIDTH*HEIGHT)
#define SCR2    (FULLW*FULLH)

//#define TWO_PI 6.28318530717958647693f

  float randomf(float minf, float maxf) {return minf + (rand()%(1UL << 31))*(maxf - minf) / (1UL << 31);} 
  
  uint8_t col[2*SCR2];
  
  #define SCL 4
  int lim = 128;
  int dirs = 9;
  int patt = 0;
  
  float pat[SCR];
  float pnew[SCR];  
  float pmedian[SCR][SCL];
  float prange[SCR][SCL];
  float pvar[SCR][SCL];
    
void rndrule() {

  lim = 96 + rand()%96;
  dirs = 6 + rand()%7;
  patt = rand()%3;
  
  for(int j=0; j<SCL; j++){
  
    for(int i=0; i<SCR; i++) {
    
      pmedian[i][j] = 0;
      prange[i][j] = 0;
      pvar[i][j] = 0;
  
    }
  }
  
  for(int i=0; i<SCR; i++) pat[i] = randomf(0, 255.0f);

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

  float R = randomf(0, TWO_PI);
  memcpy(pnew, pat, 4 * SCR);
  
  for(int j=0; j<SCL; j++){
  
    for(int i=0; i<SCR; i++) {
    
      pmedian[i][j] = 0;
      prange[i][j] = 0;
      pvar[i][j] = 0;
  
    }
  }
      
  for(int i=0; i<SCL; i++) {

    float d = (2<<i);

    for(int j=0; j<dirs; j++) {
          
      float dir = j * TWO_PI / dirs + R;
      int dx = (d * cos(dir));
      int dy = (d * sin(dir));
           
      for(int l=0; l<SCR; l++) {
        int x1 = l%WIDTH + dx, y1 = l/WIDTH + dy;
        if(x1<0) x1 = WIDTH-1-(-x1-1); if(x1>=WIDTH) x1 = x1%WIDTH;
        if(y1<0) y1 = HEIGHT-1-(-y1-1); if(y1>=HEIGHT) y1 = y1%HEIGHT;
        pmedian[l][i] += pat[x1+y1*WIDTH] / dirs;
      }
    }

    for(int j=0; j<dirs; j++) {
      float dir = j * TWO_PI / dirs + R;
      int dx = (d * cos(dir));
      int dy = (d * sin(dir));
    
      for(int l=0; l<SCR; l++) {
        int x1 = l%WIDTH + dx, y1 = l/WIDTH + dy;
        if(x1<0) x1 = WIDTH-1-(-x1-1); if(x1>=WIDTH) x1 = x1%WIDTH;
        if(y1<0) y1 = HEIGHT-1-(-y1-1); if(y1>=HEIGHT) y1 = y1%HEIGHT;
        pvar[l][i] += fabs(pat[x1+y1*WIDTH] - pmedian[l][i]) / dirs;
        prange[l][i] += pat[x1+y1*WIDTH] > (lim + i*10) ? +1.0f : -1.0f;
      }
    }
  }

  for(int l=0; l<SCR; l++) {
    int imin = 0, imax = SCL;
    float vmin = MAXFLOAT;
    float vmax = -MAXFLOAT;

    for(int i=0; i<SCL; i+=1) {
      if(pvar[l][i] <= vmin) { vmin = pvar[l][i]; imin = i; }
      if(pvar[l][i] >= vmax) { vmax = pvar[l][i]; imax = i; }
    }

    switch(patt){
      case 0: for(int i=0; i<=imin; i++)    pnew[l] += prange[l][i]; break;
      case 1: for(int i=imin; i<=imax; i++) pnew[l] += prange[l][i]; break;
      case 2: for(int i=imin; i<=imax; i++) pnew[l] += prange[l][i] + pvar[l][i] / 2.0f; break;
    }
  }

  float vmin = MAXFLOAT;
  float vmax = -MAXFLOAT;

  for(int i=0; i<SCR; i++) {
    vmin = min(vmin, pnew[i]);
    vmax = max(vmax, pnew[i]);     
  }

  float dv = vmax - vmin;
      
  for(int y=0; y<HEIGHT; y++) {
    for(int x=0; x<WIDTH; x++){
    
      pat[x+y*WIDTH] = (pnew[x+y*WIDTH] - vmin) * 255.0f / dv;
      uint8_t coll = pat[x+y*WIDTH];
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
      col[((4*x)+(4*y)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
      col[((4*x)+(4*y)*FULLW)+1] = (uint8_t)(image) & 0xFF;  
         
    }
      
  }
  
  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

}
