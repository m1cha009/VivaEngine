#include "ConsoleWindow.h"

#include <imgui.h>

#include <algorithm>
#include <format>
#include <iterator>
#include <string_view>
#include <utility>

using namespace Viva;

namespace {

// The oldest messages go once there are more than this.
constexpr size_t kMaxEntries = 2000;

constexpr const char* kLevelNames[] = { "Trace", "Info", "Warnings", "Errors" };

ImVec4 LevelColor(Log::Level level)
{
    switch (level) {
    case Log::Level::Trace: return { 0.55f, 0.55f, 0.55f, 1.0f }; // grey
    case Log::Level::Warn:  return { 1.0f, 0.8f, 0.3f, 1.0f };    // yellow
    case Log::Level::Error: return { 1.0f, 0.4f, 0.35f, 1.0f };   // red
    default:                return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }
}

} // namespace

ConsoleWindow::ConsoleWindow()
{
    // The lambda captures `this`: the listener calls back into this object, which is why the
    // destructor must remove it.
    Log::SetListener([this](Log::Level level, double seconds, std::string_view message) {
        const std::lock_guard lock(m_IncomingMutex);
        m_Incoming.push_back({ level, seconds, std::string(message) });
    });
}

ConsoleWindow::~ConsoleWindow()
{
    Log::SetListener(nullptr);
}

void ConsoleWindow::Draw(bool* open)
{
    // Take in what was logged since the last frame.
    {
        std::vector<Entry> incoming;
        {
            const std::lock_guard lock(m_IncomingMutex);
            incoming.swap(m_Incoming);
        }
        for (Entry& entry : incoming) {
            ++m_Counts[static_cast<size_t>(entry.Level)];
            m_Entries.push_back(std::move(entry));
        }
        while (m_Entries.size() > kMaxEntries)
            m_Entries.pop_front();
    }

    if (!ImGui::Begin("Console", open)) {
        ImGui::End();
        return;
    }

    // The toolbar: Clear, and a checkbox per level with its count, like Unity's Console.
    if (ImGui::Button("Clear")) {
        m_Entries.clear();
        std::fill(std::begin(m_Counts), std::end(m_Counts), size_t { 0 });
    }
    for (size_t level = 0; level < std::size(kLevelNames); ++level) {
        ImGui::SameLine();
        const std::string label = std::format("{} ({})", kLevelNames[level], m_Counts[level]);
        ImGui::Checkbox(label.c_str(), &m_Show[level]);
    }
    ImGui::Separator();

    // The messages scroll in a child region of their own, under the toolbar.
    if (ImGui::BeginChild("Messages", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
        m_Shown.clear();
        for (size_t i = 0; i < m_Entries.size(); ++i) {
            if (m_Show[static_cast<size_t>(m_Entries[i].Level)])
                m_Shown.push_back(i);
        }
        // ImGuiListClipper asks only for the lines that fit in the visible part of the list, so
        // thousands of messages cost no more to draw than a screenful. Lines must all be the same
        // height for that, which one line of text each is.
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_Shown.size()));
        while (clipper.Step()) {
            for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
                const Entry& entry = m_Entries[m_Shown[static_cast<size_t>(line)]];
                ImGui::TextColored(LevelColor(entry.Level), "[%8.3f] %s", entry.Seconds, entry.Message.c_str());
            }
        }
        // Stay at the bottom while new messages arrive, unless the user scrolled up to read.
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}
