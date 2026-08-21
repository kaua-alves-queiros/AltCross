/* Enumeração real dos monitores físicos conectados, via AppKit (NSScreen).
 * Único arquivo Objective-C do Core — necessário porque NSScreen não tem
 * equivalente em C puro. Normaliza a origem pra canto superior esquerdo com
 * Y crescendo pra baixo (AppKit usa Y crescendo pra cima por padrão), pra
 * bater com a convenção do Windows e não obrigar o lado Dart a tratar isso
 * por plataforma. */
#include "altcross/displays.h"

#import <AppKit/AppKit.h>
#include <float.h>

int altcross_displays_enumerate(altcross_display_t *out, int max_count) {
    @autoreleasepool {
        NSArray<NSScreen *> *screens = [NSScreen screens];
        NSScreen *main = [NSScreen mainScreen];

        double max_top = -DBL_MAX;
        for (NSScreen *screen in screens) {
            double top = screen.frame.origin.y + screen.frame.size.height;
            if (top > max_top) {
                max_top = top;
            }
        }

        NSUInteger i = 0;
        for (NSScreen *screen in screens) {
            if ((int)i >= max_count) {
                break;
            }
            NSRect frame = screen.frame;
            out[i].x = frame.origin.x;
            out[i].y = max_top - (frame.origin.y + frame.size.height);
            out[i].width = frame.size.width;
            out[i].height = frame.size.height;
            out[i].is_primary = (screen == main) ? 1 : 0;
            i++;
        }

        return (int)screens.count;
    }
}
