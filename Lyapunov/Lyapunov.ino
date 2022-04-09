// Lyapunov fractal //

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
  
#define ITER 4

  uint8_t color;
  float total, x, r;
  float a1, b1;
  float dx, dy;
  float xd, yd;
  float attractor;
  float MinX;
  float MaxX;
  float temp;
    
void rndrule(){

  attractor = 0.0f;
  MinX = -MAXFLOAT;
  MaxX = MAXFLOAT;
  xd = randomf(1.0f, 1.9f);
  yd = randomf(1.0f, 1.9f);
  dx = randomf(0.5f, 1.0f)/(WIDTH/3);
  dy = randomf(0.5f, 1.0f)/(HEIGHT/3);

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

  bool pari = true;
  b1 = yd;
  
  for (int a = 0; a < HEIGHT; a++){
    
    b1 = (b1 + dy);
    a1 = xd;
    
    for (int b = 0; b < WIDTH; b++){
      
      a1 = (a1 + dx);
      total = 0;
      x = attractor;

      for (int n = 1; n <= ITER; n++){
        
        if (pari) {
          r = a1;
          pari = false;
        } else{
          r = b1;
          pari = true;
        }
        
        if ((x > MinX) && (x < MaxX)) x =  r*x*(1-x);
        else if (x < MinX) x = fabs(x);
        else x = MaxX * MaxX;

        temp = fabs(r-2*r*x);
        if (temp != 0) total = total + logf(temp);
        
      }

      float lyap = total / ITER;
      if (lyap >= 0) color = 0;
      else color = 128.0f - (floor(fabs(lyap*128.0f)));

      if (color < 128){
        
        uint8_t coll = color<<2;
        uint16_t image = ST7735_COLOR565(coll, coll, coll);
        col[((4*b)+(4*a)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
        col[((4*b)+(4*a)*FULLW)+1] = (uint8_t)(image) & 0xFF;
      
      }
    }
  }
  
  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

  attractor = attractor + 0.001f;
  if(attractor >= 0.5f) attractor = 0.0f;

}
