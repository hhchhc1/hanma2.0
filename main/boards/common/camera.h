#ifndef CAMERA_H
#define CAMERA_H

#include <string>

class Camera {
public:
    virtual bool Capture() = 0;
    virtual bool SetHMirror(bool enabled) = 0;
    virtual bool SetVFlip(bool enabled) = 0;
};

#endif // CAMERA_H
