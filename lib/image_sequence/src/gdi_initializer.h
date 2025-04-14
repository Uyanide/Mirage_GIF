#ifndef GIF_IMAGE_GDI_INITIALIZER_H
#define GIF_IMAGE_GDI_INITIALIZER_H

#ifdef IMSQ_USE_GDIPLUS

#include <windows.h>

class GdiPlusInitializer {
  private:
    ULONG_PTR token  = 0;
    bool initialized = false;

  public:
    GdiPlusInitializer() = default;
    ~GdiPlusInitializer();

    bool
    initialize();

    [[nodiscard]] bool
    isInitialized() const;
};

extern GdiPlusInitializer gdiPlusInitializer;

#endif  // IMSQ_USE_GDIPLUS

#endif  // GIF_IMAGE_GDI_INITIALIZER_H