package co.siliconminds.huella;

import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;

import java.io.IOException;

/**
 * Expone la biblioteca. El cuerpo de la petición es el archivo tal cual (application/octet-stream)
 * y se lee como flujo: nunca se carga entero en memoria.
 *
 * Autenticación y autorización: no se tocan aquí. El servicio queda protegido por la configuración
 * de seguridad común del sistema (Keycloak / OAuth 2.0, responsabilidad del frente de Web 2).
 */
@RestController
@RequestMapping("/api/huellas")
public class HuellaController {

    public record HuellaDto(String algoritmo, String huella, Integer bits, String huellaReducida) {}

    private final HuellaService servicio;

    public HuellaController(HuellaService servicio) {
        this.servicio = servicio;
    }

    /** POST /api/huellas[?bits=16|24] con el archivo como cuerpo. */
    @PostMapping(consumes = "application/octet-stream")
    public HuellaDto calcular(HttpServletRequest peticion,
                              @RequestParam(value = "bits", required = false) Integer bits) throws IOException {
        byte[] completa = servicio.huella(peticion.getInputStream());
        String reducida = null;
        if (bits != null) {
            try {
                reducida = HuellaService.hex(servicio.huellaReducida(completa, bits));
            } catch (HuellaNativaException e) {
                throw new ResponseStatusException(HttpStatus.BAD_REQUEST, e.getMessage());
            }
        }
        return new HuellaDto("SHA-256", HuellaService.hex(completa), bits, reducida);
    }

    /**
     * Prueba de humo: calcula la huella de "abc" con la biblioteca nativa y la compara con el valor
     * publicado del estándar. Responde 200 solo si libhuella.so cargó y calcula bien.
     * Es lo que usa el manifiesto como comprobación de salud (RQ-03: "primera huella correcta").
     */
    @GetMapping("/salud")
    public String salud() {
        String esperada = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        String obtenida = HuellaService.hex(servicio.huella("abc".getBytes(java.nio.charset.StandardCharsets.US_ASCII)));
        if (!esperada.equals(obtenida)) {
            throw new ResponseStatusException(HttpStatus.INTERNAL_SERVER_ERROR, "la huella de \"abc\" no coincide");
        }
        return "ok";
    }

    @ExceptionHandler(HuellaNativaException.class)
    @ResponseStatus(HttpStatus.INSUFFICIENT_STORAGE)
    public String sinMemoria(HuellaNativaException e) {
        return e.getMessage();
    }
}
