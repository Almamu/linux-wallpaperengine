"""Replay the production Wayland dispatcher with a virtual clock and event I/O.

Rendering and the compositor are test doubles. The scheduling, callback budget,
and generation logic are extracted unchanged, apart from the clock/I/O names.
Native presentation tests are still needed to check compositor/driver behavior.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest


STUB = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cerrno>
#include <climits>
#include <cmath>
#include <map>
#include <poll.h>
#include <ranges>
#include <string>
#include <tuple>
#include <vector>
using namespace std::chrono_literals;
struct FakeClock {
    using duration = std::chrono::steady_clock::duration;
    using time_point = std::chrono::steady_clock::time_point;
    static inline time_point current {1s};
    static time_point now() { return current; }
};
static bool prepared = false;
static FakeClock::duration wakeDelay {};
int wl_display_prepare_read(void*) { assert(!prepared); prepared=true; return 0; }
int wl_display_dispatch_pending(void*) { assert(!prepared); return 0; }
int wl_display_flush(void*) { return 0; }
int wl_display_get_fd(void*) { return 0; }
int wl_display_read_events(void*) { assert(prepared); prepared=false; return 0; }
void wl_display_cancel_read(void*) { assert(prepared); prepared=false; }
int fakePoll(pollfd*, int, const timespec* t, void*) {
    assert(prepared);
    auto wait = std::chrono::seconds(t->tv_sec) + std::chrono::nanoseconds(t->tv_nsec);
    assert(wait >= 0ns && wait <= 16ms);
    FakeClock::current += wait + wakeDelay;
    return 0;
}
struct wl_callback {};
struct wl_output {};
constexpr uint32_t WL_OUTPUT_MODE_CURRENT=1;
void wl_callback_destroy(wl_callback*) {}
float g_Time = 0;
struct Context { struct Settings { struct Render { int maximumFPS=145; } render; } settings; };
struct WaylandOpenGLDriver;
namespace Output {
struct WaylandOutputViewport {
    WaylandOpenGLDriver* m_driver;
    int frameCredits=1, refreshRate=0;
    std::string name;
    int scale=1;
    bool initialized=false;
    void* layerSurface=nullptr;
    struct { int x=0,y=0; } size;
    std::array<int,4> viewport {};
    bool rendering=false;
    wl_callback* frameCallback=nullptr;
    FakeClock::time_point nextFrameTime {};
    FakeClock::duration lastRenderDuration {}, work {};
    explicit WaylandOutputViewport(WaylandOpenGLDriver* driver) : m_driver(driver) {}
    int frameBudget() const;
    WaylandOpenGLDriver* getDriver() const { return m_driver; }
    void resize() {}
};
}
using Output::WaylandOutputViewport;
struct WaylandOpenGLDriver {
    Context m_context;
    struct { void* display=nullptr; } m_waylandContext;
    bool m_requestedExit=false;
    uint32_t m_frameCounter=0;
    FakeClock::time_point m_nextFrameToken {};
    FakeClock::duration m_lastFrameWait {};
    struct Outputs {
        std::map<std::string, WaylandOutputViewport*> viewports;
        const auto& getViewports() const { return viewports; }
        void reset() {}
    } outputs;
    struct App {
        WaylandOpenGLDriver* driver;
        std::vector<std::tuple<WaylandOutputViewport*, uint32_t, FakeClock::time_point>> draws;
        Context& getContext() { return driver->m_context; }
        int streamFPS=0;
        FakeClock::time_point streamExpires {};
        std::map<std::string, FakeClock::time_point> streamDeadlines;
        int presentations=0;
        std::vector<FakeClock::time_point> presentationTimes;
        int getLockscreenStreamFPS() const { return FakeClock::now()<streamExpires ? streamFPS : 0; }
        FakeClock::time_point getLockscreenFrameDeadline(const std::string& name) const {
            if (!getLockscreenStreamFPS()) return FakeClock::time_point::max();
            auto found=streamDeadlines.find(name);
            return found==streamDeadlines.end() ? FakeClock::time_point{} : found->second;
        }
        void update(WaylandOutputViewport* output, bool present=true) {
            assert(output->rendering);
            presentations += present;
            if (present) presentationTimes.push_back(FakeClock::now());
            if (getLockscreenStreamFPS() && FakeClock::now() >= getLockscreenFrameDeadline(output->name))
                streamDeadlines[output->name] = FakeClock::now()+std::chrono::duration_cast<FakeClock::duration>(std::chrono::duration<double>(1.0/streamFPS));
            draws.emplace_back(output,driver->m_frameCounter,FakeClock::now());
            FakeClock::current += output->work;
        }
    } app {this};
    App& getApp() { return app; }
    Outputs& getOutput() { return outputs; }
    float getRenderTime() { return std::chrono::duration<float>(FakeClock::now().time_since_epoch()).count(); }
    void dispatchEventQueue();
};
'''

CHECKS = r'''
int main(int argc, char** argv) {
    std::string test=argv[1];
    WaylandOpenGLDriver driver;
    WaylandOutputViewport fast(&driver), slow(&driver);
    // Deliberately put the slower connector first lexically.
    driver.outputs.viewports={{"A-slow", &slow}, {"Z-fast", &fast}};
    fast.refreshRate=165080; slow.refreshRate=60000;
    fast.name="fast"; slow.name="slow";
    if(test=="lockscreen") {
        fast.frameCredits=slow.frameCredits=0;
        driver.app.streamFPS=30;
        driver.app.streamExpires=FakeClock::now()+3s;
        for (int i=0; i<400; ++i) driver.dispatchEventQueue();
        assert(driver.app.draws.size() >= 176 && driver.app.draws.size() <= 182);
        assert(driver.app.presentations==0); // No hidden EGL swaps or fake callback credits.
        assert(fast.frameCredits==0 && slow.frameCredits==0);
        auto count=driver.app.draws.size();
        for (int i=0; i<100; ++i) driver.dispatchEventQueue();
        assert(driver.app.draws.size()==count); // Expired lease stops all extra work.
        driver.app.streamExpires=FakeClock::now()+3s;
        driver.dispatchEventQueue();
        assert(driver.app.draws.size()==count+2); // Renewal wakes both hidden outputs.
        driver.app.streamFPS=0;
        surfaceFrameCallback(&slow,nullptr,0);
        driver.dispatchEventQueue();
        assert(driver.app.presentations==1); // Normal callback presentation resumes.
    } else if(test=="lockscreen_presentation_cap") {
        driver.m_context.settings.render.maximumFPS=24;
        driver.app.streamFPS=60;
        driver.app.streamExpires=FakeClock::now()+2s;
        slow.frameCredits=0;
        const auto assertDesktopCap = [&] {
            const auto& times=driver.app.presentationTimes;
            for (size_t i=1; i<times.size(); ++i)
                assert(times[i]-times[i-1] >= 41666666ns);
        };
        while (FakeClock::now() < driver.app.streamExpires) {
            surfaceFrameCallback(&fast,nullptr,0);
            driver.dispatchEventQueue();
        }
        assertDesktopCap();
        assert(driver.app.presentations==48 || driver.app.presentations==49);
        int hiddenFrames=0;
        uint32_t previousHiddenGeneration=0;
        for (const auto& [output, token, time] : driver.app.draws) {
            if (output!=&slow) continue;
            assert(token > previousHiddenGeneration);
            previousHiddenGeneration=token;
            ++hiddenFrames;
        }
        // An immediate frame and integer clock ticks can include both endpoints.
        assert(hiddenFrames>=119 && hiddenFrames<=121);
        assert(slow.frameCredits==0);
        for (int i=0; i<10; ++i) {
            surfaceFrameCallback(&fast,nullptr,0);
            driver.dispatchEventQueue();
        }
        assertDesktopCap();
    } else if(test=="lockscreen_hidden_duration") {
        driver.app.streamFPS=30;
        driver.app.streamExpires=FakeClock::now()+1s;
        slow.frameCredits=0;
        slow.lastRenderDuration=1ms;
        slow.work=3ms;
        driver.dispatchEventQueue();
        assert(slow.frameCredits==0);
        assert(slow.lastRenderDuration==1ms);
    } else if(test=="budget") {
        assert(fast.frameBudget()==2 && slow.frameBudget()==1);
        fast.frameCredits=2;
        mode(&fast,nullptr,0,1920,1080,60000);
        assert(fast.refreshRate==165080 && fast.frameCredits==2);
        mode(&fast,nullptr,WL_OUTPUT_MODE_CURRENT,1920,1080,60000);
        assert(fast.refreshRate==60000 && fast.frameCredits==1);
        fast.frameCredits=0;
        mode(&fast,nullptr,WL_OUTPUT_MODE_CURRENT,2560,1440,165080);
        assert(fast.frameCredits==0); // Mode changes cannot grant rendering credits.
        surfaceFrameCallback(&fast,nullptr,0);
        assert(fast.frameCredits==2 && fast.frameCallback==nullptr);
        for(int refresh : {0, 1000, 60000, 145000}) {
            fast.refreshRate=refresh; assert(fast.frameBudget()==1);
        }
        fast.refreshRate=165080;
        driver.m_context.settings.render.maximumFPS=INT_MAX;
        assert(fast.frameBudget()==1);
        driver.dispatchEventQueue(); // A positive interval even above clock precision.
        assert(driver.m_nextFrameToken > FakeClock::now());
    } else if(test=="frame_wait") {
        fast.nextFrameTime=slow.nextFrameTime=FakeClock::now()+5ms;
        wakeDelay=100us;
        driver.dispatchEventQueue();
        assert(driver.m_lastFrameWait==5100us);
        fast.frameCredits=slow.frameCredits=1;
        fast.nextFrameTime=slow.nextFrameTime=FakeClock::now()-1ms;
        auto before=FakeClock::now();
        driver.dispatchEventQueue();
        assert(FakeClock::now()>before);
        assert(driver.m_lastFrameWait==0ns); // Poll overhead is not idle frame time.
    } else if(test=="drift") {
        driver.outputs.viewports.erase("A-slow");
        wakeDelay=100us;
        for(int i=0;i<1000;i++) {
            surfaceFrameCallback(&fast,nullptr,0);
            driver.dispatchEventQueue();
        }
        auto& draws=driver.app.draws;
        double seconds=std::chrono::duration<double>(std::get<2>(draws.back())-std::get<2>(draws.front())).count();
        assert(draws.size()==1000);
        assert(999/seconds > 144.9 && 999/seconds < 145.1);
    } else if(test=="span") {
        driver.m_context.settings.render.maximumFPS=30;
        fast.work=slow.work=1ms;
        // Stagger callbacks, so outputs do not always render in the same dispatch.
        for(int i=0;i<500;i++) {
            surfaceFrameCallback(&fast,nullptr,0); driver.dispatchEventQueue();
            FakeClock::current += 5ms;
            surfaceFrameCallback(&slow,nullptr,0); driver.dispatchEventQueue();
        }
        std::map<WaylandOutputViewport*,uint32_t> previous;
        std::map<WaylandOutputViewport*,int> counts;
        std::map<uint32_t,int> shared;
        for(auto [output,token,time] : driver.app.draws) {
            assert(token > previous[output]); previous[output]=token;
            counts[output]++; shared[token]++;
        }
        auto first=std::get<2>(driver.app.draws.front());
        auto elapsed=std::chrono::duration<double>(FakeClock::now()-first).count();
        assert(driver.m_frameCounter <= elapsed*30+1.01);
        assert(counts[&fast]/elapsed > 29 && counts[&slow]/elapsed > 29);
        int sharedTokens=0;
        for(auto [token,count] : shared) if(count==2) sharedTokens++;
        assert(sharedTokens>counts[&fast]/2);
    } else if(test=="hidden") {
        surfaceFrameCallback(&fast,nullptr,0);
        for(int i=0;i<100;i++) {
            surfaceFrameCallback(&slow,nullptr,0); driver.dispatchEventQueue();
        }
        int fastDraws=0,slowDraws=0;
        for(auto [output,token,time] : driver.app.draws) (output==&fast?fastDraws:slowDraws)++;
        assert(fastDraws==2 && slowDraws>50);
        FakeClock::current += 10s;
        auto count=driver.m_frameCounter;
        surfaceFrameCallback(&fast,nullptr,0); driver.dispatchEventQueue();
        assert(driver.m_frameCounter==count+1); // No catch-up generations.
    } else if(test=="long_draw") {
        fast.work=9ms; slow.work=1ms;
        driver.dispatchEventQueue();
        assert(driver.app.draws.size()==2);
        assert(std::get<1>(driver.app.draws[0])+1==std::get<1>(driver.app.draws[1]));
        auto lastTime=std::chrono::duration<float>(std::get<2>(driver.app.draws.back()).time_since_epoch()).count();
        assert(std::abs(g_Time-lastTime)<1e-5f);
        auto count=driver.m_frameCounter;
        driver.dispatchEventQueue(); // No credits: an empty wake consumes no generation.
        assert(driver.m_frameCounter==count);
    } else if(test=="admission") {
        driver.dispatchEventQueue();
        driver.app.draws.clear();
        fast.lastRenderDuration=fast.work=3ms;
        slow.lastRenderDuration=slow.work=2ms;
        FakeClock::current=fast.nextFrameTime-1ms;
        fast.frameCredits=slow.frameCredits=1;
        slow.nextFrameTime=FakeClock::now();
        wakeDelay=2ms;
        driver.dispatchEventQueue();
        assert(driver.app.draws.size()==2);
        assert(std::get<0>(driver.app.draws[0])==&fast);
        assert(std::get<0>(driver.app.draws[1])==&slow);
        // Heavy work must disable deferral rather than starving the slower output.
        driver.app.draws.clear();
        wakeDelay=0ns;
        fast.lastRenderDuration=10ms;
        FakeClock::current=fast.nextFrameTime-1ms;
        slow.nextFrameTime=FakeClock::now();
        fast.frameCredits=slow.frameCredits=1;
        driver.dispatchEventQueue();
        assert(driver.app.draws.size()==1 && std::get<0>(driver.app.draws[0])==&slow);
    } else { return 2; }
}
'''


def function(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class WaylandPacing(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='lwe-pacing-test-')
        cls.addClassCleanup(cls.directory.cleanup)
        root = Path(__file__).resolve().parents[2] / 'src/WallpaperEngine/Render/Drivers'
        dispatch = function((root / 'WaylandOpenGLDriver.cpp').read_text(),
                            'void WaylandOpenGLDriver::dispatchEventQueue ()')
        viewport = (root / 'Output/WaylandOutputViewport.cpp').read_text()
        budget = function(viewport, 'int WaylandOutputViewport::frameBudget () const')
        callback = function(viewport, 'static void surfaceFrameCallback (')
        mode = function(viewport, 'static void mode (')
        dispatch = dispatch.replace('using Clock = std::chrono::steady_clock;',
                                    'using Clock = FakeClock;').replace('ppoll (', 'fakePoll (')
        source = Path(cls.directory.name) / 'probe.cpp'
        source.write_text(STUB + budget + '\n' + callback + '\n' + mode + '\n' + dispatch + CHECKS)
        cls.binary = source.with_suffix('')
        subprocess.run(['c++', '-std=c++20', '-O2', str(source), '-o', str(cls.binary)], check=True)

    def test_hidden_lockscreen_stream_is_capped_expires_and_never_swaps(self):
        subprocess.run([self.binary, 'lockscreen'], check=True)

    def test_fast_lockscreen_stream_keeps_the_configured_desktop_fps(self):
        subprocess.run([self.binary, 'lockscreen_presentation_cap'], check=True)

    def test_offscreen_work_does_not_replace_desktop_draw_estimate(self):
        subprocess.run([self.binary, 'lockscreen_hidden_duration'], check=True)

    def test_refresh_and_extreme_fps_budget(self):
        subprocess.run([self.binary, 'budget'], check=True)

    def test_late_wakes_do_not_accumulate_fps_drift(self):
        subprocess.run([self.binary, 'drift'], check=True)

    def test_frame_wait_excludes_zero_timeout_poll_overhead(self):
        subprocess.run([self.binary, 'frame_wait'], check=True)

    def test_staggered_span_callbacks_share_capped_generations(self):
        subprocess.run([self.binary, 'span'], check=True)

    def test_hidden_output_exhausts_credits_and_resumes_without_catchup(self):
        subprocess.run([self.binary, 'hidden'], check=True)

    def test_secondary_admission_and_connector_order(self):
        subprocess.run([self.binary, 'admission'], check=True)

    def test_long_draw_refreshes_generation_and_empty_wakes_do_not(self):
        subprocess.run([self.binary, 'long_draw'], check=True)


if __name__ == '__main__':
    unittest.main()
