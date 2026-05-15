/*
 *   简单的性能计时器
 */

#ifndef _MANYSTAR_PERF_TIMER_H_
#define _MANYSTAR_PERF_TIMER_H_

#include <chrono>
#include <print>

namespace manystar
{

class PerfTimer
{
public:

    enum class TimeType {
        nano,
        micro,
        milli,
        second,
    };

    explicit PerfTimer(const char* name, TimeType type = TimeType::micro) noexcept
        : _name(name), _type(type), _start(std::chrono::high_resolution_clock::now()) {
    }

    ~PerfTimer() noexcept {
        auto end = std::chrono::high_resolution_clock::now();
        switch (_type)
        {
        case TimeType::nano:
            std::println("[PERF] {} use {} ns.", _name, std::chrono::duration_cast<std::chrono::nanoseconds>(end - _start).count());
            break;
        case TimeType::micro:
            std::println("[PERF] {} use {} μs.", _name, std::chrono::duration_cast<std::chrono::microseconds>(end - _start).count());
            break;
        case TimeType::milli:
            std::println("[PERF] {} use {} ms.", _name, std::chrono::duration_cast<std::chrono::milliseconds>(end - _start).count());
            break;
        default:
            std::println("[PERF] {} use {} s.", _name, std::chrono::duration_cast<std::chrono::seconds>(end - _start).count());
            break;
        }
    }

    PerfTimer(const PerfTimer&) = delete;
    PerfTimer& operator=(const PerfTimer&) = delete;

private:

    TimeType    _type;
    const char* _name;
    std::chrono::time_point<std::chrono::high_resolution_clock> _start;
};

}	// namespace manystar

#endif // !_MANYSTAR_PERF_TIMER_H_
