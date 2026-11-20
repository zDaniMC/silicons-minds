/*
 * SHA-256 (FIPS 180-4): ciclo de compresión en aritmética modular sobre enteros sin signo
 * de 32 bits, con rotaciones y desplazamientos. El estado vive aquí entre llamadas:
 * 32 bytes de estado + un bloque parcial de 64 bytes + el contador de longitud.
 * La memoria no depende del tamaño del mensaje.
 */
#include "huella.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct EstadoHuella {
    uint32_t h[8];      /* estado interno: 8 palabras = 32 bytes */
    uint8_t  buf[64];   /* bloque parcial pendiente */
    uint64_t total;     /* bytes procesados hasta ahora */
    unsigned n;         /* bytes pendientes en buf (0..63) */
};

static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

static const uint32_t INICIAL[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

static inline uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }
static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z)  { return (x & y) ^ (~x & z); }
static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t sigma_mayor0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
static inline uint32_t sigma_mayor1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
static inline uint32_t sigma_menor0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
static inline uint32_t sigma_menor1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

/* Ciclo de compresión: 64 rondas sobre un bloque de 64 bytes. Suma módulo 2^32 (el
   desbordamiento de uint32_t es exactamente la aritmética modular que pide el estándar). */
static void comprimir(uint32_t h[8], const uint8_t bloque[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)bloque[4 * i] << 24) | ((uint32_t)bloque[4 * i + 1] << 16) |
               ((uint32_t)bloque[4 * i + 2] << 8) | (uint32_t)bloque[4 * i + 3];
    for (int i = 16; i < 64; i++)
        w[i] = sigma_menor1(w[i - 2]) + w[i - 7] + sigma_menor0(w[i - 15]) + w[i - 16];

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = hh + sigma_mayor1(e) + ch(e, f, g) + K[i] + w[i];
        uint32_t t2 = sigma_mayor0(a) + maj(a, b, c);
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

EstadoHuella *huella_nueva(void) {
    EstadoHuella *e = malloc(sizeof *e);
    if (!e) return NULL;
    memcpy(e->h, INICIAL, sizeof INICIAL);
    e->total = 0;
    e->n = 0;
    return e;
}

void huella_actualizar(EstadoHuella *e, const unsigned char *bloque, int longitud) {
    if (!e || longitud <= 0 || !bloque) return;
    size_t restante = (size_t)longitud;
    e->total += restante;

    /* completar el bloque parcial pendiente */
    if (e->n > 0) {
        size_t falta = 64 - e->n;
        size_t k = restante < falta ? restante : falta;
        memcpy(e->buf + e->n, bloque, k);
        e->n += (unsigned)k;
        bloque += k;
        restante -= k;
        if (e->n < 64) return;
        comprimir(e->h, e->buf);
        e->n = 0;
    }
    /* bloques completos directamente desde la entrada, sin copiar */
    while (restante >= 64) {
        comprimir(e->h, bloque);
        bloque += 64;
        restante -= 64;
    }
    /* lo que sobra queda pendiente para la siguiente llamada */
    if (restante > 0) {
        memcpy(e->buf, bloque, restante);
        e->n = (unsigned)restante;
    }
}

void huella_final(EstadoHuella *e, unsigned char salida32[32]) {
    if (!e || !salida32) return;

    /* Se trabaja sobre copias: el estado del llamador queda intacto. */
    uint32_t h[8];
    uint8_t buf[128];
    memcpy(h, e->h, sizeof h);
    memcpy(buf, e->buf, e->n);

    /* Relleno del estándar: un bit 1, ceros, y la longitud del mensaje en bits (64 bits,
       big-endian) al final. Si no caben los 8 bytes de longitud, hace falta un bloque más. */
    size_t n = e->n;
    uint64_t bits = e->total * 8u;
    buf[n++] = 0x80;
    size_t largo = (n <= 56) ? 64 : 128;
    memset(buf + n, 0, largo - n);
    for (int i = 0; i < 8; i++)
        buf[largo - 1 - i] = (uint8_t)(bits >> (8 * i));

    comprimir(h, buf);
    if (largo == 128) comprimir(h, buf + 64);

    for (int i = 0; i < 8; i++) {
        salida32[4 * i]     = (unsigned char)(h[i] >> 24);
        salida32[4 * i + 1] = (unsigned char)(h[i] >> 16);
        salida32[4 * i + 2] = (unsigned char)(h[i] >> 8);
        salida32[4 * i + 3] = (unsigned char)(h[i]);
    }
}

void huella_liberar(EstadoHuella *e) {
    free(e);
}

void huella_reducida(const unsigned char entrada32[32], int n_bits, unsigned char *salida) {
    if (!entrada32 || !salida || n_bits < 1 || n_bits > 256) return;
    int bytes = (n_bits + 7) / 8;
    memcpy(salida, entrada32, (size_t)bytes);
    int sobrantes = bytes * 8 - n_bits;
    if (sobrantes > 0)
        salida[bytes - 1] = (unsigned char)(salida[bytes - 1] & (0xFFu << sobrantes));
}
