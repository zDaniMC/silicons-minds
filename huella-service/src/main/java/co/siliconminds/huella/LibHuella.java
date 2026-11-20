package co.siliconminds.huella;

import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Pointer;

/**
 * Interfaz JNA de libhuella.so. Refleja native/huella.h, que es el contrato congelado.
 * No hay código puente: se declara la firma y la biblioteca queda disponible como un objeto más.
 *
 * Dónde se busca libhuella.so (decisión D-06 de la bitácora): JNA mira, en este orden,
 * jna.library.path, java.library.path y el cargador del sistema (LD_LIBRARY_PATH).
 * En el contenedor queda en /opt/huella/lib y el ENTRYPOINT fija -Djna.library.path.
 * Si no está, Native.load lanza UnsatisfiedLinkError al primer uso, con la lista de
 * rutas probadas: el servicio falla al arrancar, no a mitad de una petición.
 */
public interface LibHuella extends Library {

    LibHuella INSTANCE = Native.load("huella", LibHuella.class);

    /** Devuelve un estado nuevo, o null si no hay memoria. Hay que liberarlo. */
    Pointer huella_nueva();

    void huella_actualizar(Pointer estado, byte[] bloque, int longitud);

    void huella_final(Pointer estado, byte[] salida32);

    /** Libera el estado. Pointer null es válido y no hace nada. */
    void huella_liberar(Pointer estado);

    /** Escribe (nBits + 7) / 8 bytes en salida. */
    void huella_reducida(byte[] entrada32, int nBits, byte[] salida);
}
