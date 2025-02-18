#include "./ui_console.h"

#include "imgui.h"
#include "ui/ui_util.h"
#include "util/ringbuffer.h"

#include <cstring>
#include <string>

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

struct ExampleAppConsole {
    char InputBuf[256];
    ImVector<char*> Items;
    ImVector<char*> History;
    int HistoryPos;  // -1: new line, 0..History.Size-1 browsing history.
    ImGuiTextFilter Filter;
    bool AutoScroll;
    bool ScrollToBottom;
    size_t CursorX;

    ui_console_t* win;

    ExampleAppConsole() {
        ClearLog();
        memset(InputBuf, 0, sizeof(InputBuf));
        HistoryPos = -1;
        CursorX = 0;

        AutoScroll = true;
        ScrollToBottom = false;
    }
    ~ExampleAppConsole() {
        ClearLog();
        for (int i = 0; i < History.Size; i++)
            ImGui::MemFree(History[i]);
    }

    // Portable helpers
    static int Stricmp(const char* s1, const char* s2) {
        int d;
        while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) {
            s1++;
            s2++;
        }
        return d;
    }
    static int Strnicmp(const char* s1, const char* s2, int n) {
        int d = 0;
        while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) {
            s1++;
            s2++;
            n--;
        }
        return d;
    }
    static char* Strdup(const char* s) {
        IM_ASSERT(s);
        size_t len = strlen(s) + 1;
        void* buf = ImGui::MemAlloc(len);
        IM_ASSERT(buf);
        return (char*)memcpy(buf, (const void*)s, len);
    }
    static void Strtrim(char* s) {
        char* str_end = s + strlen(s);
        while (str_end > s && str_end[-1] == ' ')
            str_end--;
        *str_end = 0;
    }

    void ClearLog() {
        for (int i = 0; i < Items.Size; i++)
            ImGui::MemFree(Items[i]);
        Items.clear();
    }

    void AddLog(const char* fmt, ...) IM_FMTARGS(2) {
        // FIXME-OPT
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf) - 1] = 0;
        va_end(args);
        Items.push_back(Strdup(buf));
    }

    void AddChar(const char c) {
        switch (c) {
            case '\n': {
                void* line = ImGui::MemAlloc(CursorX + 1);
                IM_ASSERT(line);
                memset(line, ' ', CursorX);
                ((char*)line)[CursorX] = '\0';
                Items.push_back((char*)line);
            } break;
            case '\r': {
                CursorX = 0;
            } break;
            default: {
                if ((uint8_t)c >= 32 && (uint8_t)c < 128) {
                    if (Items.empty()) {
                        Items.push_back(Strdup(""));
                    }
                    char* line = Items.back();

                    const size_t line_len = strlen(line);
                    const size_t min_len = CursorX + 1;  // position 0 requires at least 1 char
                    if (line_len < min_len) {
                        char* new_line = (char*)ImGui::MemAlloc(min_len + 1);
                        memset(new_line, ' ', min_len);
                        memcpy(new_line, line, line_len);
                        ((char*)new_line)[min_len] = '\0';
                        Items[Items.Size - 1] = new_line;
                        ImGui::MemFree(line);
                        line = new_line;
                    }

                    ((char*)line)[CursorX] = c;
                    ++CursorX;
                }
            }
        }
    }

    void Draw(const char* title, bool* p_open) {
        ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title, p_open)) {
            ImGui::End();
            return;
        }

        // As a specific feature guaranteed by the library, after calling Begin() the last Item represent the title bar.
        // So e.g. IsItemHovered() will return true when hovering the title bar.
        // Here we create a context menu only available from the title bar.
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Close Console")) *p_open = false;
            ImGui::EndPopup();
        }

        // TODO: display items starting from the bottom

        if (ImGui::SmallButton("Clear")) {
            ClearLog();
        }
        ImGui::SameLine();
        bool copy_to_clipboard = ImGui::SmallButton("Copy");
        // static float t = 0.0f; if (ImGui::GetTime() - t > 0.02f) { t = ImGui::GetTime(); AddLog("Spam %f", t); }

        ImGui::Separator();

        // Options menu
        if (ImGui::BeginPopup("Options")) {
            ImGui::Checkbox("Auto-scroll", &AutoScroll);
            ImGui::EndPopup();
        }

        // Options, Filter
        ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_Tooltip);
        if (ImGui::Button("Options")) ImGui::OpenPopup("Options");
        ImGui::SameLine();
        Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
        ImGui::Separator();

        // Reserve enough left-over height for 1 separator + 1 input text
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild(
                "ScrollingRegion",
                ImVec2(0, -footer_height_to_reserve),
                ImGuiChildFlags_NavFlattened,
                ImGuiWindowFlags_HorizontalScrollbar)) {
            if (ImGui::BeginPopupContextWindow()) {
                if (ImGui::Selectable("Clear")) ClearLog();
                ImGui::EndPopup();
            }

            // Display every line as a separate entry so we can change their color or add custom widgets.
            // If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
            // NB- if you have thousands of entries this approach may be too inefficient and may require user-side
            // clipping to only process visible items. The clipper will automatically measure the height of your first
            // item and then "seek" to display only items in the visible area. To use the clipper we can replace your
            // standard loop:
            //      for (int i = 0; i < Items.Size; i++)
            //   With:
            //      ImGuiListClipper clipper;
            //      clipper.Begin(Items.Size);
            //      while (clipper.Step())
            //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            // - That your items are evenly spaced (same height)
            // - That you have cheap random access to your elements (you can access them given their index,
            //   without processing all the ones before)
            // You cannot this code as-is if a filter is active because it breaks the 'cheap random-access' property.
            // We would need random-access on the post-filtered list.
            // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices
            // or offsets of items that passed the filtering test, recomputing this array when user changes the filter,
            // and appending newly elements as they are inserted. This is left as a task to the user until we can manage
            // to improve this example code!
            // If your items are of variable height:
            // - Split them into same height items would be simpler and facilitate random-seeking into your list.
            // - Consider using manual call to IsRectVisible() and skipping extraneous decoration from your items.
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));  // Tighten spacing
            if (copy_to_clipboard) ImGui::LogToClipboard();
            for (const char* item : Items) {
                if (!Filter.PassFilter(item)) continue;

                // Normally you would store more information in your item than just a string.
                // (e.g. make Items[] an array of structure, store color/type etc.)
                ImVec4 color;
                bool has_color = false;
                if (strstr(item, "[error]")) {
                    color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                    has_color = true;
                }
                else if (strncmp(item, "# ", 2) == 0) {
                    color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f);
                    has_color = true;
                }
                if (has_color) ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(item);
                if (has_color) ImGui::PopStyleColor();
            }
            if (copy_to_clipboard) ImGui::LogFinish();

            // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the
            // frame. Using a scrollbar or mouse-wheel will take away from the bottom edge.
            if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                ImGui::SetScrollHereY(1.0f);
            ScrollToBottom = false;

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::Separator();

        // Command-line
        bool reclaim_focus = false;
        ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue
            | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackHistory;
        if (ImGui::InputText(
                "Input",
                InputBuf,
                IM_ARRAYSIZE(InputBuf),
                input_text_flags,
                &TextEditCallbackStub,
                (void*)this)) {
            char* s = InputBuf;
            Strtrim(s);
            ExecCommand(s);
            strcpy(s, "");
            reclaim_focus = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            // The user pressed the Escape key
            ExecCommand("\x1B");
            reclaim_focus = true;
        }

        // Auto-focus on window apparition
        ImGui::SetItemDefaultFocus();
        if (reclaim_focus) ImGui::SetKeyboardFocusHere(-1);  // Auto focus previous widget

        ImGui::End();
    }

    void ExecCommand(const char* command_line) {
        if (command_line[0]) {
            // Insert into history. First find match and delete it so it can be pushed to the back.
            // This isn't trying to be smart or optimal.
            HistoryPos = -1;
            for (int i = History.Size - 1; i >= 0; i--)
                if (Stricmp(History[i], command_line) == 0) {
                    ImGui::MemFree(History[i]);
                    History.erase(History.begin() + i);
                    break;
                }
            History.push_back(Strdup(command_line));
        }

        // Process command
        if (Stricmp(command_line, "CLEAR") == 0) {
            ClearLog();
        }
        else if (Stricmp(command_line, "HISTORY") == 0) {
            AddLog("# %s\n", command_line);
            int first = History.Size - 10;
            for (int i = first > 0 ? first : 0; i < History.Size; i++)
                AddLog("%3d: %s\n", i, History[i]);
        }
        else {
            const char* s = command_line;
            while (*s) {
                if (!rb_put(win->rx, *s)) break;
                ++s;
            }
            rb_put(win->rx, '\r');  // CR
            rb_put(win->rx, '\n');  // LF
        }

        // On command input, we scroll to bottom even if AutoScroll==false
        ScrollToBottom = true;
    }

    // In C++11 you'd be better off using lambdas for this sort of forwarding callbacks
    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data) {
        ExampleAppConsole* console = (ExampleAppConsole*)data->UserData;
        return console->TextEditCallback(data);
    }

    int TextEditCallback(ImGuiInputTextCallbackData* data) {
        // AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
        switch (data->EventFlag) {
            case ImGuiInputTextFlags_CallbackHistory: {
                // Example of HISTORY
                const int prev_history_pos = HistoryPos;
                if (data->EventKey == ImGuiKey_UpArrow) {
                    if (HistoryPos == -1)
                        HistoryPos = History.Size - 1;
                    else if (HistoryPos > 0)
                        HistoryPos--;
                }
                else if (data->EventKey == ImGuiKey_DownArrow) {
                    if (HistoryPos != -1)
                        if (++HistoryPos >= History.Size) HistoryPos = -1;
                }

                // A better implementation would preserve the data on the current input line along with cursor position.
                if (prev_history_pos != HistoryPos) {
                    const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, history_str);
                }
            }
        }
        return 0;
    }
};

static ExampleAppConsole console;

void ui_console_init(ui_console_t* win, const ui_console_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    memset(win, 0, sizeof(ui_console_t));
    win->title = desc->title;
    win->rx = desc->rx;
    win->tx = desc->tx;
    win->init_x = (float)desc->x;
    win->init_y = (float)desc->y;
    win->init_w = (float)((desc->w == 0) ? 400 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 256 : desc->h);
    win->open = win->last_open = desc->open;
    win->valid = true;

    console.win = win;
}

void ui_console_discard(ui_console_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
    console.ClearLog();
}

void ui_console_process_tx(ui_console_t* win) {
    // Pull characters from tx buffer
    uint8_t data;
    while (rb_get(win->tx, &data)) {
        console.AddChar(data);
    }
}

void ui_console_draw(ui_console_t* win) {
    CHIPS_ASSERT(win && win->valid && win->title);
    ui_util_handle_window_open_dirty(&win->open, &win->last_open);

    ui_console_process_tx(win);

    if (win->open) {
        console.Draw(win->title, &win->open);
    }
}

void ui_console_save_settings(ui_console_t* win, ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    ui_settings_add(settings, win->title, win->open);
}

void ui_console_load_settings(ui_console_t* win, const ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    win->open = ui_settings_isopen(settings, win->title);
}
