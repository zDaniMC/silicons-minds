/* huella.h — el estado vive en C entre llamadas
 *
 * CONTRATO CONGELADO con el frente de Web 2 (ver ../BITACORA.md, decisiones D-01 a D-04).
 * Las cinco firmas son las del documento del equipo y no cambian.
 *
 * 1. QUIÉN LIBERA LA MEMORIA
 *    huella_nueva() reserva con malloc. Quien la llama debe llamar a huella_liberar()
 *    exactamente una vez (en Java: dentro de un finally). huella_liberar(NULL) no hace nada.
 *    Ninguna otra función reserva memoria. El arreglo `salida` siempre lo preasigna quien llama.
 *
 * 2. CÓMO SE REPORTA UN ERROR
 *    C no lanza excepciones y estas firmas devuelven void, así que:
 *      huella_nueva()      devuelve NULL si no hay memoria.
 *      huella_actualizar() con e == NULL, bloque == NULL (y longitud > 0) o longitud < 0
 *                          no hace nada. longitud == 0 es válido y no hace nada.
 *      huella_final()      con e == NULL o salida32 == NULL no hace nada y no escribe.
 *      huella_reducida()   con n_bits fuera de 1..256, o punteros NULL, no escribe nada.
 *    Por eso el lado de Java valida antes de llamar (estado != null, 1 <= n_bits <= 256)
 *    y traduce un estado NULL a una excepción. Ver tabla en BITACORA.md.
 *
 * 3. CASOS DE REFERENCIA
 *    native/run_tests.sh: vectores del estándar ("" y "abc"), sha256sum y MessageDigest.
 */
#ifndef HUELLA_H
#define HUELLA_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EstadoHuella EstadoHuella;   /* opaco para quien llama */

/* Crea un estado nuevo (huella de un mensaje vacío todavía). NULL si no hay memoria. */
EstadoHuella *huella_nueva(void);

/* Procesa `longitud` bytes más. Se puede llamar muchas veces con bloques de cualquier
   tamaño (no hace falta que sean múltiplos de 64): el resultado es el mismo que
   procesar todo el mensaje de una vez. */
void huella_actualizar(EstadoHuella *e, const unsigned char *bloque, int longitud);

/* Aplica el relleno del estándar y escribe los 32 bytes de la huella.
   No destruye el estado: se puede seguir actualizando o volver a finalizar. */
void huella_final(EstadoHuella *e, unsigned char salida32[32]);

/* Libera el estado. */
void huella_liberar(EstadoHuella *e);

/* Huella reducida: los mismos bits truncados a n_bits (16 o 24 para la demostración de
   colisión; se acepta cualquier valor entre 1 y 256). Escribe (n_bits + 7) / 8 bytes en
   `salida`; si n_bits no es múltiplo de 8, los bits sobrantes del último byte quedan en 0. */
void huella_reducida(const unsigned char entrada32[32], int n_bits,
                     unsigned char *salida);

#ifdef __cplusplus
}
#endif

#endif /* HUELLA_H */
