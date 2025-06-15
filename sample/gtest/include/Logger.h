#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "syncLogger.h"

namespace Logger {
class Logger {
public:
    static vc::SyncLogger &instance()
    {
        static vc::SyncLogger logger;
        return logger;
    }
};

template <typename... T>
inline void print(T &&...args)
{
    Logger::instance().print(std::forward<decltype(args)>(args)...);
}

template <typename... T>
inline void println(T &&...args)
{
    Logger::instance().println(std::forward<decltype(args)>(args)...);
}
}; // namespace Logger
#endif