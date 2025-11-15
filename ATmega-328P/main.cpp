#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

/* ==== CONFIGURACIÓN DE HARDWARE ==== */
#define BTN_START PC0
#define BTN_FIRE  PC1

/* ==== MATRIZ FILAS ==== */
static const uint8_t PORT_R[8] = {1,2,4,8,16,32,64,128};

/* ==== MACROS DE COLUMNA ==== */
#define COLBIT(c) (1U<<(7-(c)))

/* ==== GLYPHS ==== */
static const uint8_t GLYPH_M[8] = {
  0b00000000,
  0b11111111,
  0b01000000,
  0b00100000,
  0b00100000,
  0b01000000,
  0b11111111,
  0b00000000
};
static const uint8_t GLYPH_A[8] = {
  0b00000000,
  0b11111111,
  0b10000000,
  0b10000000,
  0b10000000,
  0b10000000,
  0b00000000,
  0b00000000
};

/* ==== TEXTO DESPLAZANTE “PRESS START” ==== */
static const uint8_t MENSAJE_C[] = {
    0x00,0x7E,0x7E,0x12,0x12,0x1E,0x1E,0x00, // P
    0x00,0x7E,0x7E,0x1A,0x3A,0x6A,0x4E,0x00, // R
    0x00,0x3E,0x3E,0x2A,0x2A,0x2A,0x2A,0x00, // E
    0x00,0x4E,0x4E,0x5A,0x5A,0x72,0x72,0x00, // S
    0x00,0x4E,0x4E,0x5A,0x5A,0x72,0x72,0x00, // S
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // espacio
    0x00,0x4E,0x4E,0x5A,0x5A,0x72,0x72,0x00, // S
    0x00,0x02,0x02,0x7E,0x7E,0x02,0x02,0x00, // T
    0x00,0x7C,0x7E,0x16,0x16,0x7E,0x7C,0x00, // A
    0x00,0x7E,0x7E,0x1A,0x3A,0x6A,0x4E,0x00, // R
    0x00,0x02,0x02,0x7E,0x7E,0x02,0x02,0x00, // T
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // espacio
};

/* ==== FUNCIONES DE BAJO NIVEL ==== */
static inline void io_init(void){
    DDRB = 0xFF; // columnas
    DDRD = 0xFF; // filas
    DDRC = (1 << PC2) | (1 << PC3); // botones
    PORTC = (1<<BTN_START)|(1<<BTN_FIRE); // pull-ups
}

static void scan_frame(const uint8_t cols[8], uint16_t ms){
    while(ms--){
        for(uint8_t r=0;r<8;r++){
            PORTD = PORT_R[r];
            PORTB = ~cols[r];
            _delay_us(300);
        }
    }
}

/* ==== MUESTRA UNA IMAGEN 8x8 ==== */
static void show_glyph(const uint8_t glyph[8], uint16_t ms){
    uint8_t cols[8];
    for(uint8_t r=0;r<8;r++) cols[r]=glyph[r];
    scan_frame(cols, ms);
}

/* ==== CONSTRUCCIÓN DEL FRAME DE JUEGO ==== */
static void build_game_frame(uint8_t cols[8],
                             uint8_t bar_top, uint8_t bar_len,
                             bool bullet_active, uint8_t bullet_row, uint8_t bullet_col)
{
    for(uint8_t r=0;r<8;r++) cols[r]=0x00;
    for(uint8_t i=0;i<bar_len;i++){
        int8_t rr = (int8_t)bar_top + (int8_t)i;
        if(rr>=0 && rr<8) cols[rr] |= COLBIT(0);
    }
    if(bullet_active && bullet_row<8 && bullet_col<8){
        cols[bullet_row] |= COLBIT(bullet_col);
    }
}

/* ==== UNA RONDA DEL JUEGO ==== */
static bool play_round(void){
    uint8_t bar_len = 3;
    int8_t bar_top = 0;
    int8_t bar_dir = +1;
    bool bullet = false;
    uint8_t b_row = 3;
    int8_t b_col = 7;

    while(1){
        if((!(PINC & (1<<BTN_FIRE))) && !bullet){
            bullet = true;
            b_row = 3;
            b_col = 7;
        }

        bar_top += bar_dir;
        if(bar_top <= 0){ bar_top = 0; bar_dir = +1; }
        if(bar_top >= (8 - bar_len)){ bar_top = 8 - bar_len; bar_dir = -1; }

        if(bullet){
            b_col--;
            if(b_col < 0){ return false; }
        }

        if(bullet && b_col == 0){
            if(b_row >= bar_top && b_row < (bar_top + bar_len)){
                return true;
            }else{
                return false;
            }
        }

        uint8_t cols[8];
        build_game_frame(cols, bar_top, bar_len, bullet, b_row, (uint8_t)b_col);
        scan_frame(cols, 12);
    }
}

/* ==== ANIMACIÓN: PRESS START ==== */
static void show_start_initial(void){
    for(uint8_t i=0;i<84;i++){
        for(uint8_t k=0;k<10;k++){
            for(uint8_t j=0;j<8;j++){
                if(!(PINC & (1<<BTN_START))) return; // si presiona start, salir
                PORTD = PORT_R[j];
                PORTB = ~MENSAJE_C[i+j];
                _delay_ms(0.8);
            }
        }
    }
}



/* ==== MAIN ==== */
int main(void){
    io_init();
    uint8_t estado = 0;

    while(1){
        switch(estado){
            case 0: // PANTALLA START
                PORTC &= ~(1 << PC2);
                PORTC &= ~(1 << PC3);
                show_start_initial();
                if(!(PINC & (1<<BTN_START))){
                    _delay_ms(100);
                    estado = 1;
                }
                break;

            case 1: { // JUEGO EN PROGRESO
                PORTC |= (1 << PC2);
                PORTC &= ~(1 << PC3);
                bool hit = play_round();
                estado = hit ? 2 : 3;
                break;
            }

            case 2: // GANÓ
                PORTC |= (1 << PC3);
                PORTC &= ~(1 << PC2);
                show_glyph(GLYPH_M, 1500);
                estado = 0;
                break;

            case 3: // PERDIÓ
                PORTC |= (1 << PC2);
                PORTC |= (1 << PC3);
                show_glyph(GLYPH_A, 1500);
                estado = 0;
                break;
        }
    }
}
