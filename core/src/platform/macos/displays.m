/* Enumeração real dos monitores físicos conectados, via AppKit (NSScreen).
 * Único arquivo Objective-C do Core — necessário porque NSScreen não tem
 * equivalente em C puro. Normaliza a origem pra canto superior esquerdo com
 * Y crescendo pra baixo (AppKit usa Y crescendo pra cima por padrão), pra
 * bater com a convenção do Windows e não obrigar o lado Dart a tratar isso
 * por plataforma. */
#include "altcross/displays.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#include <float.h>
#include <math.h>

static double screens_max_top(NSArray<NSScreen *> *screens) {
    double max_top = -DBL_MAX;
    for (NSScreen *screen in screens) {
        double top = screen.frame.origin.y + screen.frame.size.height;
        if (top > max_top) {
            max_top = top;
        }
    }
    return max_top;
}

static unsigned int screen_display_id(NSScreen *screen) {
    NSNumber *number =
        [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
    return number.unsignedIntValue;
}

int altcross_displays_enumerate(altcross_display_t *out, int max_count) {
    @autoreleasepool {
        NSArray<NSScreen *> *screens = [NSScreen screens];
        NSScreen *main = [NSScreen mainScreen];
        double max_top = screens_max_top(screens);

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

int altcross_displays_enumerate_ex(altcross_display_ex_t *out, int max_count) {
    @autoreleasepool {
        NSArray<NSScreen *> *screens = [NSScreen screens];
        NSScreen *main = [NSScreen mainScreen];
        double max_top = screens_max_top(screens);

        NSUInteger i = 0;
        for (NSScreen *screen in screens) {
            if ((int)i >= max_count) {
                break;
            }
            NSRect frame = screen.frame;
            out[i].display_id = screen_display_id(screen);
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

int altcross_displays_set_origin(unsigned int display_id, int x, int y) {
    @autoreleasepool {
        NSArray<NSScreen *> *screens = [NSScreen screens];
        NSScreen *target = nil;
        for (NSScreen *screen in screens) {
            if (screen_display_id(screen) == display_id) {
                target = screen;
                break;
            }
        }
        if (!target) {
            return 1;
        }

        /* (x, y) chega na nossa convenção (Y pra baixo, ancorada no topo de
         * TODAS as telas — ver enumerate). O CGDirectDisplay usa Y pra
         * baixo também, mas ancorado no topo da tela PRINCIPAL
         * especificamente — as duas âncoras só coincidem se a principal
         * também for a mais alta de todas. Convertemos explicitamente em
         * vez de assumir isso. */
        double max_top = screens_max_top(screens);
        NSScreen *main = [NSScreen mainScreen];
        double main_top = main.frame.origin.y + main.frame.size.height;

        double target_height = target.frame.size.height;
        double appkit_origin_y = max_top - target_height - (double)y;
        double cg_y = main_top - (appkit_origin_y + target_height);

        CGDisplayConfigRef config;
        if (CGBeginDisplayConfiguration(&config) != kCGErrorSuccess) {
            return 1;
        }
        CGError err = CGConfigureDisplayOrigin(
            config, (CGDirectDisplayID)display_id, (int32_t)lround((double)x),
            (int32_t)lround(cg_y));
        if (err != kCGErrorSuccess) {
            CGCancelDisplayConfiguration(config);
            return 1;
        }
        err = CGCompleteDisplayConfiguration(config, kCGConfigureForSession);
        return err == kCGErrorSuccess ? 0 : 1;
    }
}
