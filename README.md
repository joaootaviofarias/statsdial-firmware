# Stats Dial Firmware

A real-time PC hardware monitor built with an ESP32, an LCD display, that gets PC info via serial from a dedicated [App](https://github.com/joaootaviofarias/statsdial-desktop).

![Monitor](images/monitor.jpg)
![Clock](images/clock.jpg)

## 📟 ESP32 Firmware

The microcontroller runs a graphical UI (powered by LVGL) to display system metrics.

**Features:**

- **Real-time Monitoring:** Displays CPU usage, GPU usage, RAM usage, and CPU temperature.
- **Smart Sleep Mode:** Automatically transitions to a Clock screen when the PC goes to sleep and serial data stops.
- **Auto-Reconnect:** Smoothly handles the PC waking up from suspend and instantly resumes displaying hardware metrics.
