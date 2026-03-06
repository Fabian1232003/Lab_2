#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdint.h>
#include "esp_timer.h"


// filas rojo (r0..r7)
#define r0 19
#define r1 16
#define r2 16
#define r3 16
#define r4 16
#define r5 16
#define r6 16
#define r7 5

// filas verde (v0..v7)
#define v0  16
#define v1  15
#define v2  18                  
#define v3  4   
#define v4  33
#define v5  32                    
#define v6  14                                                            
#define v7  16                                             

// columnas (c0..c7) -> solo pines de salida, sin repetir con r/v
#define c0 23      // unchanged
#define c1 22      // unchanged
#define c2 2       // unchanged
#define c3 0       // unchanged
#define c4 25      // was 35 (input-only), moved to 25
#define c5 17     // was 25, now 26 to be different from c4
#define c6 26      // unchanged
#define c7 27      // NEW: was 27 before if you want, now 33 so all 8 are unique

// otras señales (lu, ld, ru, rd, rs) -> muevo a pines libres y seguros
#define lu  21   // si no quieres reutilizarlo, elige cualquier otro libre
#define ld  34
#define ru  12
#define rd  13




static bool lup_prev= false;
static bool ldown_prev= false;
static bool rup_prev= false;
static bool rdown_prev= false;  

static uint8_t reset_ticks = 0; 

static int8_t pf=0; 
static int8_t pc=0;
static const gpio_num_t r[8] = { r0, r1, r2, r3, r4, r5, r6, r7 };
static const gpio_num_t v[8] = { v0, v1, v2, v3, v4, v5, v6, v7 };
static const gpio_num_t fil[8] = { c0, c1, c2, c3, c4, c5, c6, c7 };


static uint8_t contador_mux=0;
static int8_t col_izq = 0;
static int8_t col_der = 0;
static uint8_t dir=0;

static const uint8_t col_izqV[8] = {0, 1, 2, 3, 4, 5, 6, 7};
static const uint8_t col_derV[8] = {0, 1, 2, 3, 4, 5, 6, 7};
static bool lado=false;


volatile bool half_second_flag = false;

static void half_second_cb(void *arg)
{
    half_second_flag = true;
}

void init_half_second_timer(void)
{
    const esp_timer_create_args_t timer_args = {
        .callback = &half_second_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "half_sec"
    };

    static esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 500000)); // 500000 us = 500 ms
}

int8_t circular(int8_t value){
    int8_t da=0;
    da = ((value % 8) + 8) % 8;
    return da; 
}

void reinicio(void){
    for (int i = 0; i < 8; i++) {
        gpio_set_level(r[i], 1);
        gpio_set_level(fil[i], 0);
        gpio_set_level(v[i], 1);
    }
}

uint8_t contador3(void) {
    static uint8_t count = 0; 
      
    count = (count + 1) ;
    if (count > 7) count = 0; 
     
    return count;
}



void set_col(bool up, bool down, bool side){
    if (up==1 && side==1 && col_izq>0 && down==0) {
        col_izq= col_izq-1;
    }
    if (up==0 && side==1 && col_izq<7 && down==1) {
        col_izq= col_izq+1;
    }

    if (up==1 && side==0 && col_der>0 && down==0) {
        col_der= col_der-1;
    }
    if (up==0 && side==0 && col_der<7 && down==1) {
        col_der= col_der+1;
    }
    

}

void dir_pelota( bool side){
    if (dir==0 && side ){
        pc++;
    }   
    else if (dir==0 && !side){
        pc--; 
    }
    else if (dir==1 && side){
        pc++; pf--;
    }
       
    else if (dir==1 && !side){
        pc--; pf--;
    }
       
    else if (dir==2 && side){
        pc++; pf++;
    }
       
    else if (dir==2 && !side){
        pc--; pf++;
    
    }
}      
  

    

uint8_t mov_pelota(void){
    
    
    if ((pc==0) && (pf== col_izqV[circular(col_izq)])) {
        dir=0;
        lado=true;
        
    }
    else if ((pc==0) && (pf== col_izqV[circular(col_izq+1)])) {
        dir=1;
        lado=true;
        
    }

    else if ((pc==0) && (pf== col_izqV[circular(col_izq-1)])) {
        dir=2;
        lado=true;
        
    }

    else if ((pc==7) && ( pf== col_derV[circular(col_der)])) {
        dir=0;
        lado=false;
    }
    else if ((pc==7) && ( pf== col_derV[circular(col_der+1)])) {
        dir=1;
        lado=false;
    }

    else if ((pc==7) && ( pf== col_derV[circular(col_der-1)])) {
        dir=2;
        lado=false;
    }

    else if (!(pc==0 || pc==7) && (pf==0 || pf==7) && dir!=0) {
        if (dir==1) dir=2;
        else dir=1;
        
    }
    else if ((pc==0 || pc==7)) {
        dir=3;
        
    }

    return dir;
}

void mux_salida(uint8_t col_izq, uint8_t col_der, uint8_t contador_mux){
    switch (contador_mux){
        case 0:break;
        case 1: reinicio( ); gpio_set_level(fil[col_izqV[circular(col_izq-1)]],1); gpio_set_level(r[0],0);break;
        case 2: reinicio( ); gpio_set_level(fil[col_izqV[circular(col_izq)]],1); gpio_set_level(r[0],0);break;
        case 3: reinicio( ); gpio_set_level(fil[col_izqV[circular(col_izq+1)]],1); gpio_set_level(r[0],0);break;
        case 4: reinicio( ); gpio_set_level(fil[col_derV[circular(col_der-1)]],1); gpio_set_level(r[7],0);break;
        case 5: reinicio( ); gpio_set_level(fil[col_derV[circular(col_der)]],1); gpio_set_level(r[7],0);break;
        case 6: reinicio( ); gpio_set_level(fil[col_derV[circular(col_der+1)]],1); gpio_set_level(r[7],0);break; 
        case 7: reinicio( ); gpio_set_level(fil[pf],1); gpio_set_level(v[pc],0);break;
        default:break;
    }


}         
 



void app_main(void) {
    gpio_config_t io_sal = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << r0) | (1ULL << r1) | (1ULL << r2) | (1ULL << r3) |
                        (1ULL << r4) | (1ULL << r5) | (1ULL << r6) | (1ULL << r7) |
                        (1ULL << v0) | (1ULL << v1) | (1ULL << v2) | (1ULL << v3) |
                        (1ULL << v4) | (1ULL << v5) | (1ULL << v6) | (1ULL << v7) |
                        (1ULL << c0) | (1ULL << c1) | (1ULL << c2) | (1ULL << c3) |
                        (1ULL << c4) | (1ULL << c5) | (1ULL << c6) | (1ULL << c7),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE
    };
    gpio_config(&io_sal);

   
    

    gpio_config_t io_ent={
        .pin_bit_mask= (1ULL << lu) | (1ULL << ld) |(1ULL << ru) | (1ULL << rd) ,
        .mode = GPIO_MODE_INPUT, 
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en =GPIO_PULLDOWN_DISABLE,              // sin pull-down
        .pull_up_en = GPIO_PULLUP_ENABLE   

    };
    gpio_config(&io_ent);
    init_half_second_timer();

 
    while(1){

        contador_mux=contador3();
        vTaskDelay(pdMS_TO_TICKS(1.333));   
        bool lup = !gpio_get_level(lu);
        bool ldown = !gpio_get_level(ld);
        bool rup = !gpio_get_level(ru);
        bool rdown = !gpio_get_level(rd);
        if (pf>7) pf=7;
        
        if (pc>7) pc=7;
        

        bool all_pressed = lup && ldown && rup && rdown;

        if (half_second_flag) {
            half_second_flag = false;

            if (all_pressed) {
             if (reset_ticks < 6) reset_ticks++;
            } else {
                reset_ticks = 0;
            }

            if (reset_ticks >= 6) {   // 6 * 0.5 s = 3 s
                col_izq = 3;
                col_der = 3;
                pc = 4;
                pf = 4;
                dir = 0;
                reset_ticks = 0;
            }
            if (dir != 3) {
                
                dir_pelota(lado);
                dir = mov_pelota();
            }

        }
         
       
        if (dir == 3){
         continue ;
        }
       else {
            mux_salida(col_izq, col_der, contador_mux);
        
            if (lup==true && lup_prev==false) set_col(1,0,1);
            else if (ldown==true && ldown_prev==false) set_col(0,1,1);
            else if (rup==true && rup_prev==false) set_col(1,0,0);
            else if (rdown==true && rdown_prev==false) set_col(0,1,0);
            // Tick de 0.5 s: reset + movimiento de pelota
       }

      
       lup_prev = lup;
       ldown_prev = ldown;
       rup_prev = rup;
       rdown_prev = rdown;
    }
   
} 