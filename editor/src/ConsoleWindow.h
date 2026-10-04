#pragma once

#include "Viva/Log.h"

#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

// The Console window: the engine's log, like Unity's Console. It listens to Viva::Log from the
// moment it's created, so it should be created early, to catch the startup messages too.
class ConsoleWindow {
public:
    ConsoleWindow();
    ~ConsoleWindow(); // stops listening

    ConsoleWindow(const ConsoleWindow&) = delete;
    ConsoleWindow& operator=(const ConsoleWindow&) = delete;

    void Draw(bool* open);

private:
    struct Entry {
        Viva::Log::Level Level = Viva::Log::Level::Info;
        double Seconds = 0.0;
        std::string Message;
    };

    // The listener may be called on another thread (Log's lock keeps it from being called twice at
    // once), so new messages wait in m_Incoming, behind a mutex of their own, until Draw moves
    // them to m_Entries on the main thread. Draw then reads m_Entries without any lock, which also
    // means that logging from inside Draw (an ImGui assert, say) can't deadlock.
    std::mutex m_IncomingMutex;
    std::vector<Entry> m_Incoming;
    // A deque drops its oldest entries from the front without moving the rest, as a vector would.
    std::deque<Entry> m_Entries;
    std::vector<size_t> m_Shown; // the entries that pass the level checkboxes, rebuilt each frame
    size_t m_Counts[4] {}; // messages per level, indexed by Level
    bool m_Show[4] { false, true, true, true }; // Trace hidden at first
};
