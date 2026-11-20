package co.siliconminds.huella;

/**
 * C no lanza excepciones: aquí se traducen sus condiciones de error (ver tabla en BITACORA.md, D-04).
 */
public class HuellaNativaException extends RuntimeException {

    public enum Causa {
        /** huella_nueva() devolvió NULL: la biblioteca no pudo reservar memoria. */
        SIN_MEMORIA,
        /** Argumento fuera del contrato (por ejemplo, nBits fuera de 1..256). C no lo habría reportado. */
        ARGUMENTO_INVALIDO
    }

    private final Causa causa;

    public HuellaNativaException(Causa causa, String mensaje) {
        super(mensaje);
        this.causa = causa;
    }

    public Causa causa() {
        return causa;
    }
}
