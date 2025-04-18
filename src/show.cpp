#include "show.hpp"

void
show(const SDL_AudioDeviceEvent& e)
{
    const char* type = e.type == SDL_AUDIODEVICEADDED ? "added" : "removed";
    printf("AudioDeviceEvent: type=%s, which=%u, iscapture=%u\n",
                 type, e.which, unsigned{e.iscapture});
}


void
show(const SDL_ControllerAxisEvent& e)
{
    printf("ControllerAxisEvent: which=%d axis=%u value=%d\n",
                 e.which, unsigned{e.axis}, int{e.value});
}


void
show(const SDL_ControllerButtonEvent& e)
{
    const char* state = e.state == SDL_PRESSED ? "pressed" : "released";

    auto b = static_cast<SDL_GameControllerButton>(e.button);
    const char* button = SDL_GameControllerGetStringForButton(b);

    printf("ControllerButtonEvent: which=%d, button=%s, state=%s\n",
                 int{e.which}, button, state);
}


void
show(const SDL_ControllerDeviceEvent& e)
{
    const char* type = "invalid";
    switch (e.type) {
    case SDL_CONTROLLERDEVICEADDED:
        type = "added";
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        type = "removed";
        break;
    case SDL_CONTROLLERDEVICEREMAPPED:
        type = "remapped";
        break;
    }
    printf("ControllerDeviceEvent: type=%s, which=%d\n",
                 type, int{e.which});
}


void
show(const SDL_ControllerSensorEvent& e)
{
    const char* sensor = "invalid";
    switch (e.sensor) {
    case SDL_SENSOR_UNKNOWN:
        sensor = "unknown";
        break;
    case SDL_SENSOR_ACCEL:
        sensor = "accel";
        break;
    case SDL_SENSOR_GYRO:
        sensor = "gyro";
        break;
    case SDL_SENSOR_ACCEL_L:
        sensor = "accel-l";
        break;
    case SDL_SENSOR_GYRO_L:
        sensor = "gyro-l";
        break;
    case SDL_SENSOR_ACCEL_R:
        sensor = "accel-r";
        break;
    case SDL_SENSOR_GYRO_R:
        sensor = "gyro-r";
        break;
    }
    printf("ControllerSensorEvent: which=%d, sensor=%s\n",
                 int{e.which}, sensor);
}


void
show(const SDL_ControllerTouchpadEvent& e)
{
    const char* type = "invalid";
    switch (e.type) {
    case SDL_CONTROLLERTOUCHPADDOWN:
        type = "down";
        break;
    case SDL_CONTROLLERTOUCHPADMOTION:
        type = "motion";
        break;
    case SDL_CONTROLLERTOUCHPADUP:
        type = "up";
        break;
    }
    printf("ControllerTouchpadEvent: type=%s, which=%d, touchpad=%d,"
                 " x=%f, y=%f, pressure=%f\n",
                 type, int{e.which}, e.touchpad, e.x, e.y, e.pressure);
}


void
show(const SDL_DisplayEvent& e)
{
    const char* event = "invalid";
    switch (e.event) {
    case SDL_DISPLAYEVENT_ORIENTATION:
        event = "orientation";
        break;
    case SDL_DISPLAYEVENT_CONNECTED:
        event = "connected";
        break;
    case SDL_DISPLAYEVENT_DISCONNECTED:
        event = "disconnected";
        break;
    }
    printf("DisplayEvent: display=%d, event=%s\n",
                 e.display, event);
}


void
show(const SDL_JoyAxisEvent& e)
{
    printf("JoyAxisEvent: which=%d, axis=%u, value=%d\n",
                 int{e.which}, unsigned{e.axis}, e.value);
}


void
show(const SDL_JoyBallEvent& e)
{
    printf("JoyBallEvent: which=%d, ball=%u, xrel=%d, yrel=%d\n",
                 int{e.which}, unsigned{e.ball}, int{e.xrel}, int{e.yrel});
}


void
show(const SDL_JoyBatteryEvent& e)
{
    const char* level;
    switch (e.level) {
    case SDL_JOYSTICK_POWER_EMPTY:
        level = "empty";
        break;
    case SDL_JOYSTICK_POWER_LOW:
        level = "low";
        break;
    case SDL_JOYSTICK_POWER_MEDIUM:
        level = "medium";
        break;
    case SDL_JOYSTICK_POWER_FULL:
        level = "full";
        break;
    case SDL_JOYSTICK_POWER_WIRED:
        level = "wired";
        break;
    default:
        level = "invalid";
    }
    printf("JoyBatteryEvent: which=%d, level=%s\n",
                 e.which, level);
}


void
show(const SDL_JoyButtonEvent& e)
{
    const char* state = e.state == SDL_PRESSED ? "pressed" : "released";
    printf("JoyButtonEvent: which=%d, button=%u, state=%s\n",
                 int{e.which}, unsigned{e.button}, state);
}


void
show(const SDL_JoyDeviceEvent& e)
{
    const char* type = e.type == SDL_JOYDEVICEADDED ? "added" : "removed";
    printf("JoyDeviceEvent: type=%s, which=%d\n",
                 type, e.which);
}


void
show(const SDL_JoyHatEvent& e)
{
    const char* value = "invalid";
    switch (e.value) {
    case SDL_HAT_CENTERED:
        value = "centered";
        break;
    case SDL_HAT_UP:
        value = "up";
        break;
    case SDL_HAT_RIGHT:
        value = "right";
        break;
    case SDL_HAT_DOWN:
        value = "down";
        break;
    case SDL_HAT_LEFT:
        value = "left";
        break;
    case SDL_HAT_RIGHTUP:
        value = "rightup";
        break;
    case SDL_HAT_RIGHTDOWN:
        value = "rightdown";
        break;
    case SDL_HAT_LEFTUP:
        value = "leftup";
        break;
    case SDL_HAT_LEFTDOWN:
        value = "leftdown";
        break;
    }
    printf("JoyHatEvent: which=%d, hat=%u, value=%s\n",
                 int{e.which}, unsigned{e.hat}, value);
}


void
show(const SDL_MouseButtonEvent& e)
{
    const char* state = e.state == SDL_PRESSED ? "pressed" : "released";
    printf("MouseButtonEvent: windowID=%x, which=%u, button=%u,"
                 " state=%s, clicks=%u. x=%d, y=%d\n",
                 e.windowID, e.which, unsigned{e.button}, state,
                 unsigned{e.clicks}, e.x, e.y);
}

void
show(const SDL_MouseMotionEvent& e)
{
    printf("MouseMotionEvent: windowID=%x, which=%u, state=%x, x=%d, y=%d,"
                 " xrel=%d, yrel=%d\n",
                 e.windowID, e.which, e.state, e.x, e.y, e.xrel, e.yrel);
}


void
show(const SDL_MouseWheelEvent& e)
{
    const char* direction = e.direction == SDL_MOUSEWHEEL_NORMAL ? "normal" : "flipped";

    printf("MouseWheelEvent: windowID=%x, which=%u, direction=%s,"
                 " x=%d, y=%d, preciseX=%f, preciseY=%f, mouseX=%d, mouseY=%d\n",
                 e.windowID, e.which, direction,
                 e.x, e.y, e.preciseX, e.preciseY, e.mouseX, e.mouseY);
}


void
show(const SDL_QuitEvent&)
{
    printf("QuitEvent\n");
}


void
show(const SDL_TouchFingerEvent& e)
{
    const char* type;
    switch (e.type) {
    case SDL_FINGERMOTION:
        type = "motion";
        break;
    case SDL_FINGERDOWN:
        type = "down";
        break;
    case SDL_FINGERUP:
        type = "up";
        break;
    default:
        type = "invalid";
    }
    printf("TouchFingerEvent: type=%s, touchID=%lld, fingerID=%lld,"
                 " x=%f, y=%f, dx=%f, dy=%f, pressure=%f, windowID=%u\n",
                 type, e.touchId, e.fingerId,
                 e.x, e.y, e.dx, e.dy, e.pressure, e.windowID);
}


void
show(const SDL_WindowEvent& e)
{
    const char* event = "invalid";
    switch (e.event) {
    case SDL_WINDOWEVENT_SHOWN:
        event = "shown";
        break;
    case SDL_WINDOWEVENT_HIDDEN:
        event = "hidden";
        break;
    case SDL_WINDOWEVENT_EXPOSED:
        event = "exposed";
        break;
    case SDL_WINDOWEVENT_MOVED:
        event = "moved";
        break;
    case SDL_WINDOWEVENT_RESIZED:
        event = "resized";
        break;
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        event = "size-changed";
        break;
    case SDL_WINDOWEVENT_MINIMIZED:
        event = "minimized";
        break;
    case SDL_WINDOWEVENT_MAXIMIZED:
        event = "maximized";
        break;
    case SDL_WINDOWEVENT_RESTORED:
        event = "restored";
        break;
    case SDL_WINDOWEVENT_ENTER:
        event = "enter";
        break;
    case SDL_WINDOWEVENT_LEAVE:
        event = "leavel";
        break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
        event = "focus-gained";
        break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
        event = "focus-lost";
        break;
    case SDL_WINDOWEVENT_CLOSE:
        event = "close";
        break;
    case SDL_WINDOWEVENT_TAKE_FOCUS:
        event = "take-focus";
        break;
    case SDL_WINDOWEVENT_HIT_TEST:
        event = "hit-test";
        break;
    case SDL_WINDOWEVENT_ICCPROF_CHANGED:
        event = "iccprof-changed";
        break;
    case SDL_WINDOWEVENT_DISPLAY_CHANGED:
        event = "display-changed";
        break;
    }
    printf("WindowEvent: windowID=%x, event=%s\n",
                 e.windowID, event);
}



#define CASE(x)                                    \
    case x:                                        \
        name = #x;                                 \
        break

void
show(const SDL_Event& e)
{
    const char* name = nullptr;
    switch (e.type) {
    case SDL_QUIT:
        show(e.quit);
        break;

        CASE(SDL_APP_TERMINATING);
        CASE(SDL_APP_LOWMEMORY);
        CASE(SDL_APP_WILLENTERBACKGROUND);
        CASE(SDL_APP_DIDENTERBACKGROUND);
        CASE(SDL_APP_WILLENTERFOREGROUND);
        CASE(SDL_APP_DIDENTERFOREGROUND);

        CASE(SDL_LOCALECHANGED);

    case SDL_DISPLAYEVENT:
        show(e.display);
        break;
    case SDL_WINDOWEVENT:
        show(e.window);
        break;

        CASE(SDL_SYSWMEVENT);
        CASE(SDL_KEYDOWN);
        CASE(SDL_KEYUP);
        CASE(SDL_TEXTEDITING);
        CASE(SDL_TEXTINPUT);
        CASE(SDL_KEYMAPCHANGED);
        CASE(SDL_TEXTEDITING_EXT);

    case SDL_MOUSEMOTION:
        show(e.motion);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        show(e.button);
        break;
    case SDL_MOUSEWHEEL:
        show(e.wheel);
        break;

    case SDL_JOYAXISMOTION:
        show(e.jaxis);
        break;
    case SDL_JOYBALLMOTION:
        show(e.jball);
        break;
    case SDL_JOYHATMOTION:
        show(e.jhat);
        break;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        show(e.jbutton);
        break;
    case SDL_JOYDEVICEADDED:
    case SDL_JOYDEVICEREMOVED:
        show(e.jdevice);
        break;
    case SDL_JOYBATTERYUPDATED:
        show(e.jbattery);
        break;
    case SDL_CONTROLLERAXISMOTION:
        show(e.caxis);
        break;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        show(e.cbutton);
        break;
    case SDL_CONTROLLERDEVICEADDED:
    case SDL_CONTROLLERDEVICEREMOVED:
    case SDL_CONTROLLERDEVICEREMAPPED:
        show(e.cdevice);
        break;
    case SDL_CONTROLLERTOUCHPADDOWN:
    case SDL_CONTROLLERTOUCHPADMOTION:
    case SDL_CONTROLLERTOUCHPADUP:
        show(e.ctouchpad);
        break;
    case SDL_CONTROLLERSENSORUPDATE:
        show(e.csensor);
        break;
    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
    case SDL_FINGERMOTION:
        show(e.tfinger);
        break;

        CASE(SDL_DOLLARGESTURE);
        CASE(SDL_DOLLARRECORD);
        CASE(SDL_MULTIGESTURE);
        CASE(SDL_CLIPBOARDUPDATE);
        CASE(SDL_DROPFILE);
        CASE(SDL_DROPTEXT);
        CASE(SDL_DROPBEGIN);
        CASE(SDL_DROPCOMPLETE);

    case SDL_AUDIODEVICEADDED:
    case SDL_AUDIODEVICEREMOVED:
        show(e.adevice);
        break;

        CASE(SDL_SENSORUPDATE);
        CASE(SDL_RENDER_TARGETS_RESET);
        CASE(SDL_RENDER_DEVICE_RESET);
        CASE(SDL_POLLSENTINEL);

    default:
        name = "unknown";
    }
    if (name)
        printf("Event: %s\n", name);
}

#undef CASE