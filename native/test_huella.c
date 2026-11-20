/*
 * Pruebas de la biblioteca, desde C: sin Java, sin Spring, sin contenedor.
 *
 *   ./test_huella                       casos autocontenidos (RN-01..RN-06 y contrato)
 *   ./test_huella hash <archivo> <N>    huella del archivo leído en bloques de N bytes
 *   ./test_huella bench <archivo> <N>   MB/s de una pasada con bloques de N bytes
 *   ./test_huella ciclos <n>            n ciclos nueva/actualizar/final/liberar (para valgrind)
 */
#define _POSIX_C_SOURCE 200809L
#include "huella.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int fallos = 0, casos = 0;

static void verificar(int ok, const char *nombre) {
    casos++;
    if (!ok) fallos++;
    printf("  %s  %s\n", ok ? "OK   " : "FALLA", nombre);
}

static void a_hex(const unsigned char *h, int n, char *out) {
    for (int i = 0; i < n; i++) sprintf(out + 2 * i, "%02x", h[i]);
}

static void huella_de(const unsigned char *datos, size_t n, size_t bloque, unsigned char out[32]) {
    EstadoHuella *e = huella_nueva();
    for (size_t i = 0; i < n; i += bloque) {
        size_t k = (n - i < bloque) ? n - i : bloque;
        huella_actualizar(e, datos + i, (int)k);
    }
    huella_final(e, out);
    huella_liberar(e);
}

static int bits_distintos(const unsigned char a[32], const unsigned char b[32]) {
    int d = 0;
    for (int i = 0; i < 32; i++) d += __builtin_popcount((unsigned)(a[i] ^ b[i]));
    return d;
}

/* generador determinista (xorshift64): las pruebas dan siempre el mismo resultado */
static uint64_t semilla = 88172645463325252ull;
static uint64_t siguiente(void) {
    semilla ^= semilla << 13; semilla ^= semilla >> 7; semilla ^= semilla << 17;
    return semilla;
}

static double ahora(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static long rss_kb(void) {
    FILE *f = fopen("/proc/self/statm", "r");
    long pag = 0, rss = 0;
    if (!f) return -1;
    if (fscanf(f, "%ld %ld", &pag, &rss) != 2) rss = -1;
    fclose(f);
    return rss < 0 ? -1 : rss * (sysconf(_SC_PAGESIZE) / 1024);
}

/* ---------- modos auxiliares ---------- */

static int modo_hash(const char *ruta, size_t bloque, int bench) {
    FILE *f = fopen(ruta, "rb");
    if (!f || bloque == 0) { fprintf(stderr, "no se pudo abrir %s\n", ruta); return 1; }
    unsigned char *buf = malloc(bloque);
    if (!buf) { fclose(f); return 1; }
    EstadoHuella *e = huella_nueva();
    double t0 = ahora();
    size_t n, total = 0;
    while ((n = fread(buf, 1, bloque, f)) > 0) { huella_actualizar(e, buf, (int)n); total += n; }
    unsigned char h[32];
    huella_final(e, h);
    double dt = ahora() - t0;
    huella_liberar(e);
    free(buf);
    fclose(f);
    if (bench) { printf("%.1f\n", ((double)total / 1048576.0) / dt); return 0; }
    char hex[65];
    a_hex(h, 32, hex);
    printf("%s\n", hex);
    return 0;
}

static int modo_ciclos(long n) {
    unsigned char h[32], dato[100];
    memset(dato, 'x', sizeof dato);
    for (long i = 0; i < n; i++) {
        EstadoHuella *e = huella_nueva();
        huella_actualizar(e, dato, (int)sizeof dato);
        huella_final(e, h);
        huella_liberar(e);
    }
    return 0;
}

/* ---------- casos ---------- */

static void rn01_vectores(void) {
    puts("RN-01  Huella de un bloque; vectores publicados del estándar");
    unsigned char h[32];
    char hex[65];
    huella_de((const unsigned char *)"", 0, 64, h);
    a_hex(h, 32, hex);
    verificar(!strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), "cadena vacía");
    huella_de((const unsigned char *)"abc", 3, 64, h);
    a_hex(h, 32, hex);
    verificar(!strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "\"abc\"");
    const char *largo = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    huella_de((const unsigned char *)largo, strlen(largo), 64, h);
    a_hex(h, 32, hex);
    verificar(!strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"),
              "vector de dos bloques del estándar (448 bits)");
}

static void rn02_bloques(void) {
    puts("RN-02  Lectura por bloques equivalente a una sola pasada (100003 bytes, tamaño primo)");
    enum { N = 100003 };
    unsigned char *datos = malloc(N);
    for (int i = 0; i < N; i++) datos[i] = (unsigned char)siguiente();
    unsigned char a[32], b[32], c[32], d[32];
    huella_de(datos, N, N, a);          /* una sola pasada */
    huella_de(datos, N, 65536, b);      /* bloques de 64 KB */
    huella_de(datos, N, 1024, c);       /* bloques de 1 KB */
    huella_de(datos, N, 7, d);          /* bloques que nunca cuadran con 64 */
    verificar(!memcmp(a, b, 32), "64 KB == una pasada");
    verificar(!memcmp(a, c, 32), "1024 bytes == una pasada");
    verificar(!memcmp(a, d, 32), "7 bytes == una pasada");
    free(datos);
}

static void rn04_avalancha(void) {
    puts("RN-04  Efecto avalancha: 20 pares que difieren en un solo bit");
    double suma = 0, minimo = 100, maximo = 0;
    for (int par = 0; par < 20; par++) {
        unsigned char m1[64], m2[64], h1[32], h2[32];
        for (int i = 0; i < 64; i++) m1[i] = (unsigned char)siguiente();
        memcpy(m2, m1, sizeof m1);
        int bit = (int)(siguiente() % (64 * 8));
        m2[bit / 8] ^= (unsigned char)(1u << (bit % 8));
        huella_de(m1, 64, 64, h1);
        huella_de(m2, 64, 64, h2);
        double pct = 100.0 * bits_distintos(h1, h2) / 256.0;
        suma += pct;
        if (pct < minimo) minimo = pct;
        if (pct > maximo) maximo = pct;
    }
    double media = suma / 20.0;
    printf("         media %.1f %% (mínimo %.1f %%, máximo %.1f %%)\n", media, minimo, maximo);
    verificar(media >= 45.0 && media <= 55.0, "porcentaje medio de bits distintos entre 45 % y 55 %");
    verificar(minimo > 35.0, "ningún par cerca de 0 (implementación sana)");
}

static void rn05_reducida(void) {
    puts("RN-05  Huella reducida");
    unsigned char h[32], r16[2], r24[3], r12[2];
    huella_de((const unsigned char *)"abc", 3, 64, h);
    huella_reducida(h, 16, r16);
    huella_reducida(h, 24, r24);
    huella_reducida(h, 12, r12);
    verificar(!memcmp(r16, h, 2), "16 bits: coinciden con los primeros bits de la completa");
    verificar(!memcmp(r24, h, 3), "24 bits: coinciden con los primeros bits de la completa");
    verificar(r12[0] == h[0] && r12[1] == (h[1] & 0xF0), "12 bits: el último byte lleva ceros en los bits sobrantes");

    /* fuerza bruta: hasta repetir una salida de 16 bits (paradoja del cumpleaños, ~300 intentos) */
    static int64_t visto[65536];
    for (int i = 0; i < 65536; i++) visto[i] = -1;
    double t0 = ahora();
    int64_t a = -1, b = -1;
    for (int64_t i = 0; i < 1000000; i++) {
        unsigned char entrada[8], hh[32], r[2];
        memcpy(entrada, &i, 8);
        huella_de(entrada, 8, 8, hh);
        huella_reducida(hh, 16, r);
        int clave = (r[0] << 8) | r[1];
        if (visto[clave] >= 0) { a = visto[clave]; b = i; break; }
        visto[clave] = i;
    }
    double dt = ahora() - t0;
    int ok = (a >= 0 && a != b);
    if (ok) {
        unsigned char ea[8], eb[8], ha[32], hb[32], ra[2], rb[2];
        memcpy(ea, &a, 8); memcpy(eb, &b, 8);
        huella_de(ea, 8, 8, ha); huella_de(eb, 8, 8, hb);
        huella_reducida(ha, 16, ra); huella_reducida(hb, 16, rb);
        ok = memcmp(ea, eb, 8) != 0 && !memcmp(ra, rb, 2) && memcmp(ha, hb, 32) != 0;
        printf("         colisión de 16 bits tras %lld intentos en %.4f s (entradas %lld y %lld)\n",
               (long long)b + 1, dt, (long long)a, (long long)b);
    }
    verificar(ok && dt < 5.0, "con 16 bits se hallan dos entradas distintas con la misma salida en segundos");
}

static void rn06_memoria(void) {
    puts("RN-06  Sin fugas: 10 000 ciclos crear/actualizar/finalizar/liberar");
    modo_ciclos(200);                       /* calentamiento del asignador */
    long antes = rss_kb();
    modo_ciclos(10000);
    long despues = rss_kb();
    printf("         memoria residente: %ld KB antes, %ld KB después\n", antes, despues);
    verificar(antes >= 0 && despues - antes <= 256, "la memoria residente no aumenta (una fuga de 10 000 estados serían ~1,2 MB; tolerancia 256 KB)");
}

static void contrato_errores(void) {
    puts("Contrato  Convención de errores (sin excepciones, funciones void)");
    unsigned char h[32], marca[32], r[4];
    memset(marca, 0xAA, 32);

    huella_actualizar(NULL, (const unsigned char *)"x", 1);
    huella_final(NULL, h);
    huella_liberar(NULL);
    verificar(1, "con estado NULL no hacen nada ni se caen");

    EstadoHuella *e = huella_nueva();
    huella_actualizar(e, NULL, 10);
    huella_actualizar(e, (const unsigned char *)"abc", -5);
    huella_actualizar(e, (const unsigned char *)"abc", 0);
    memcpy(h, marca, 32);
    huella_final(e, NULL);
    huella_final(e, h);
    char hex[65];
    a_hex(h, 32, hex);
    verificar(!strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
              "longitud negativa, cero o bloque NULL no alteran el estado");

    unsigned char h2[32];
    huella_actualizar(e, (const unsigned char *)"abc", 3);
    huella_final(e, h);
    huella_final(e, h2);
    huella_liberar(e);
    verificar(!memcmp(h, h2, 32), "finalizar dos veces da lo mismo (el estado no se destruye)");

    memset(r, 0xAA, 4);
    huella_reducida(h, 0, r);
    huella_reducida(h, 257, r);
    huella_reducida(NULL, 16, r);
    verificar(r[0] == 0xAA && r[1] == 0xAA, "huella_reducida con n_bits inválido no escribe");
}

int main(int argc, char **argv) {
    if (argc == 4 && !strcmp(argv[1], "hash"))  return modo_hash(argv[2], (size_t)atol(argv[3]), 0);
    if (argc == 4 && !strcmp(argv[1], "bench")) return modo_hash(argv[2], (size_t)atol(argv[3]), 1);
    if (argc == 3 && !strcmp(argv[1], "ciclos")) return modo_ciclos(atol(argv[2]));
    if (argc != 1) { fprintf(stderr, "uso: %s [hash|bench <archivo> <bloque> | ciclos <n>]\n", argv[0]); return 2; }

    rn01_vectores();
    rn02_bloques();
    rn04_avalancha();
    rn05_reducida();
    rn06_memoria();
    contrato_errores();
    printf("%d casos, %d fallas\n", casos, fallos);
    return fallos ? 1 : 0;
}
