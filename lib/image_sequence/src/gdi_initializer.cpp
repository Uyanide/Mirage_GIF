#ifdef IMSQ_USE_GDIPLUS

#include "gdi_initializer.h"

#include <gdiplus.h>


bool
GdiPlusInitializer::initialize() {
    if (initialized) return true;
    if (token != 0) return false;
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::Status status = Gdiplus::GdiplusStartup(&token, &input, nullptr);
    initialized            = (status == Gdiplus::Ok);
    return initialized;
}

GdiPlusInitializer::~GdiPlusInitializer() {
    if (initialized) {
        Gdiplus::GdiplusShutdown(token);
    }
}

bool
GdiPlusInitializer::isInitialized() const {
    return initialized;
}

GdiPlusInitializer gdiPlusInitializer;

#endif  // IMSQ_USE_GDIPLUS