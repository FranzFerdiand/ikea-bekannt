#ifndef RADIATOR_CONFIG_H
#define RADIATOR_CONFIG_H

#include <stdint.h>

/**
 * --- HEIZKÖRPER-KOLLISIONSSCHUTZ FÜR IKEA BEKANT ---
 *
 * Berechnung der Encoder-Ticks:
 *   Unterster Werksanschlag (~64 cm): ~150 Ticks (0x0096)
 *   Standard-Sitzhöhe       (~70 cm): 1590 Ticks (0x0636)
 *   Standard-Stehhöhe      (~110 cm): 5632 Ticks (0x1600)
 *   Faktor:                 1 cm    ~ 100 Ticks
 *
 * Formel:
 *   Ziel-Ticks = 1590 + (Wunsch-Minimalhöhe_in_cm - 70) * 100
 *
 * Beispiel:
 *   73 cm Minimalhöhe (inkl. 2 cm Sicherheitsabstand zum Heizkörper):
 *   1590 + (73 - 70) * 100 = 1890 Ticks = 0x0762
 */

// Absolutes Notfall-Minimum im entsperrten Zustand (0x0190 = 400 Ticks ≈ 65 cm)
#define RADIATOR_UNLOCKED_POS   ((int16_t)0x0190)

// Standard-Startwert, falls das EEPROM komplett leer ist (0x0762 = 1890 Ticks ≈ 73 cm)
#define RADIATOR_DEFAULT_POS    ((int16_t)0x0762)

// Bremsweg-Reserve (0x0050 = 80 Ticks ≈ 8 mm Bremsauslauf)
#define RADIATOR_BRAKE_MARGIN   ((int16_t)0x0050)

// Toleranzfenster am Anschlag zum Entsperren per 3-Sek-Speichergeste (0x0064 = 100 Ticks ≈ 1 cm)
#define RADIATOR_UNLOCK_WINDOW  ((int16_t)0x0064)

// Liefert die aktuell gültige dynamische Heizkörper-Untergrenze zurück
int16_t bui_get_radiator_min(void);

#endif /* RADIATOR_CONFIG_H */
