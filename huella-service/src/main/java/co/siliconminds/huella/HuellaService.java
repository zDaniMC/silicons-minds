package co.siliconminds.huella;

import com.sun.jna.Pointer;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.io.InputStream;
import java.util.HexFormat;

/**
 * Traduce entre el modelo del dominio (flujos de bytes, texto hexadecimal) y lo que C entiende
 * (un estado opaco, bloques de bytes). La frontera Java↔C se cruza una vez por bloque de 64 KB,
 * nunca una vez por byte, y el estado nativo se libera siempre en un finally.
 */
@Service
public class HuellaService {

    /** Tamaño del bloque que cruza la frontera. Parte del contrato (RN-02). */
    public static final int TAM_BLOQUE = 65536;

    private final LibHuella lib;

    @Autowired
    public HuellaService() {
        this(LibHuella.INSTANCE);
    }

    /** Para pruebas: permite inyectar una biblioteca simulada y comprobar el finally sin tener el .so. */
    HuellaService(LibHuella lib) {
        this.lib = lib;
    }

    /** SHA-256 completa (32 bytes) de todo el flujo, leído por bloques de 64 KB. No cierra el flujo. */
    public byte[] huella(InputStream entrada) throws IOException {
        Pointer estado = lib.huella_nueva();
        if (estado == null) {
            throw new HuellaNativaException(HuellaNativaException.Causa.SIN_MEMORIA,
                    "la biblioteca nativa no pudo reservar un estado");
        }
        try { // la biblioteca reserva, así que Java debe liberar: sin esto, se fuga
            byte[] buffer = new byte[TAM_BLOQUE];
            int leidos;
            while ((leidos = entrada.read(buffer)) > 0) { // read puede devolver menos de 64 KB: se pasa `leidos`
                lib.huella_actualizar(estado, buffer, leidos);
            }
            byte[] salida = new byte[32];
            lib.huella_final(estado, salida);
            return salida;
        } finally {
            lib.huella_liberar(estado);
        }
    }

    public byte[] huella(byte[] datos) {
        try {
            return huella(new java.io.ByteArrayInputStream(datos));
        } catch (IOException e) {
            throw new IllegalStateException(e); // un ByteArrayInputStream no falla
        }
    }

    /**
     * Huella truncada a nBits para la demostración de colisión. C no reporta error con nBits
     * inválido (solo no escribe), así que aquí se valida antes de llamar.
     */
    public byte[] huellaReducida(byte[] completa, int nBits) {
        if (completa == null || completa.length != 32) {
            throw new HuellaNativaException(HuellaNativaException.Causa.ARGUMENTO_INVALIDO,
                    "la huella completa debe tener 32 bytes");
        }
        if (nBits < 1 || nBits > 256) {
            throw new HuellaNativaException(HuellaNativaException.Causa.ARGUMENTO_INVALIDO,
                    "nBits debe estar entre 1 y 256 (se usan 16 o 24 en la demostración)");
        }
        byte[] salida = new byte[(nBits + 7) / 8];
        lib.huella_reducida(completa, nBits, salida);
        return salida;
    }

    /** Cuántos de los 256 bits difieren entre dos huellas (para el experimento de avalancha). */
    public static int bitsDistintos(byte[] a, byte[] b) {
        int distintos = 0;
        for (int i = 0; i < 32; i++) {
            distintos += Integer.bitCount((a[i] ^ b[i]) & 0xFF);
        }
        return distintos;
    }

    public static String hex(byte[] huella) {
        return HexFormat.of().formatHex(huella);
    }
}
