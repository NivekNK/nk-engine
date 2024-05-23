#include "nk/window.h"

namespace nk {
    class WindowWin32 : public Window {
    public:
        WindowWin32(const ApplicationConfig& config);
        virtual ~WindowWin32() override;

        virtual bool pump_messages() override;
        virtual f64 get_absolute_time() override;
        virtual void sleep(u64 ms) override;

    private:
        HINSTANCE m_hinstance;
        HWND m_hwnd;

        f64 m_clock_frequency;
        LARGE_INTEGER m_start_time;
    };
}
