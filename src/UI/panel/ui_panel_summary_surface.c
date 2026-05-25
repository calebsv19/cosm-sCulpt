#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include "UI/text_draw.h"

#include <string.h>

static SDL_Rect UIPanelSummary_IntersectRects(SDL_Rect a, SDL_Rect b) {
    SDL_Rect result = {0, 0, 0, 0};
    const int left = (a.x > b.x) ? a.x : b.x;
    const int top = (a.y > b.y) ? a.y : b.y;
    const int right = ((a.x + a.w) < (b.x + b.w)) ? (a.x + a.w) : (b.x + b.w);
    const int bottom = ((a.y + a.h) < (b.y + b.h)) ? (a.y + a.h) : (b.y + b.h);
    result.x = left;
    result.y = top;
    result.w = right - left;
    result.h = bottom - top;
    if (result.w < 0) result.w = 0;
    if (result.h < 0) result.h = 0;
    return result;
}

void UIPanelSummary_DrawText(SDL_Renderer* renderer,
                             TTF_Font* font,
                             const char* text,
                             int x,
                             int y,
                             SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

void UIPanelSummary_DrawTextClipped(SDL_Renderer* renderer,
                                    TTF_Font* font,
                                    const char* text,
                                    int x,
                                    int y,
                                    int max_width,
                                    int clip_height,
                                    SDL_Color color) {
    int width = 0;
    SDL_Rect clip = { x, y - 2, max_width, clip_height };
    SDL_Rect previous_clip = {0, 0, 0, 0};
    SDL_bool had_clip = SDL_FALSE;

    if (!renderer || !font || !text || !text[0] || max_width <= 0 || clip_height <= 0) return;
    if (line_drawing_text_measure_utf8(renderer, font, text, &width, NULL) &&
        width <= max_width) {
        UIPanelSummary_DrawText(renderer, font, text, x, y, color);
        return;
    }

    had_clip = SDL_RenderIsClipEnabled(renderer);
    if (had_clip) {
        (void)SDL_RenderGetClipRect(renderer, &previous_clip);
        clip = UIPanelSummary_IntersectRects(clip, previous_clip);
        if (clip.w <= 0 || clip.h <= 0) {
            return;
        }
    }
    SDL_RenderSetClipRect(renderer, &clip);
    UIPanelSummary_DrawText(renderer, font, text, x, y, color);
    SDL_RenderSetClipRect(renderer, had_clip ? &previous_clip : NULL);
}

static int UIPanelSummary_MeasureTextWidth(TTF_Font* font, const char* text, size_t length) {
    char buffer[512];
    int width = 0;
    size_t clamped_length = length;
    if (!text || length == 0) return 0;
    if (!font) return (int)length * 8;
    if (clamped_length >= sizeof(buffer)) clamped_length = sizeof(buffer) - 1;
    memcpy(buffer, text, clamped_length);
    buffer[clamped_length] = '\0';
    if (TTF_SizeUTF8(font, buffer, &width, NULL) != 0) {
        return 0;
    }
    return width;
}

static size_t UIPanelSummary_CountWordChars(const char* text) {
    size_t length = 0;
    if (!text) return 0;
    while (text[length] != '\0' &&
           text[length] != ' ' &&
           text[length] != '\t' &&
           text[length] != '\n' &&
           text[length] != '\r') {
        ++length;
    }
    return length;
}

static size_t UIPanelSummary_FitWordPrefix(TTF_Font* font,
                                           const char* text,
                                           size_t length,
                                           int max_width) {
    size_t fit = 1;
    if (!text || length == 0 || max_width <= 0) return 0;
    for (size_t i = 1; i <= length; ++i) {
        if (UIPanelSummary_MeasureTextWidth(font, text, i) > max_width) {
            return (i > 1) ? (i - 1) : 1;
        }
        fit = i;
    }
    return fit;
}

static size_t UIPanelSummary_BuildWrappedLine(TTF_Font* font,
                                              const char* text,
                                              int max_width,
                                              char* out_line,
                                              size_t out_line_size) {
    size_t cursor = 0;
    size_t line_length = 0;
    if (out_line && out_line_size > 0) out_line[0] = '\0';
    if (!text || !text[0] || max_width <= 0 || !out_line || out_line_size < 2) return 0;

    while (text[cursor] == ' ' || text[cursor] == '\t') {
        ++cursor;
    }
    if (text[cursor] == '\r') ++cursor;
    if (text[cursor] == '\n') {
        return cursor + 1;
    }

    while (text[cursor] != '\0' && text[cursor] != '\n' && text[cursor] != '\r') {
        size_t word_length = UIPanelSummary_CountWordChars(text + cursor);
        char candidate[512];
        size_t candidate_length = line_length;
        if (word_length == 0) {
            ++cursor;
            continue;
        }

        if (candidate_length > 0 && candidate_length + 1 < sizeof(candidate)) {
            candidate[candidate_length++] = ' ';
        }
        if (candidate_length + word_length >= sizeof(candidate)) {
            word_length = sizeof(candidate) - candidate_length - 1;
        }
        memcpy(candidate, out_line, line_length);
        memcpy(candidate + candidate_length, text + cursor, word_length);
        candidate_length += word_length;
        candidate[candidate_length] = '\0';

        if (line_length == 0 && UIPanelSummary_MeasureTextWidth(font, candidate, candidate_length) > max_width) {
            size_t fit = UIPanelSummary_FitWordPrefix(font, text + cursor, word_length, max_width);
            if (fit == 0) fit = 1;
            if (fit >= out_line_size) fit = out_line_size - 1;
            memcpy(out_line, text + cursor, fit);
            out_line[fit] = '\0';
            return cursor + fit;
        }

        if (UIPanelSummary_MeasureTextWidth(font, candidate, candidate_length) > max_width) {
            break;
        }

        if (candidate_length >= out_line_size) candidate_length = out_line_size - 1;
        memcpy(out_line, candidate, candidate_length);
        out_line[candidate_length] = '\0';
        line_length = candidate_length;
        cursor += word_length;
        while (text[cursor] == ' ' || text[cursor] == '\t') {
            ++cursor;
        }
    }

    return cursor;
}

int UIPanelSummary_CountWrappedLines(TTF_Font* font,
                                     const char* text,
                                     int max_width) {
    int lines = 0;
    size_t consumed = 0;
    char line[512];
    if (!text || !text[0] || max_width <= 0) return 0;
    while (text[consumed] != '\0') {
        size_t step = UIPanelSummary_BuildWrappedLine(font, text + consumed, max_width, line, sizeof(line));
        if (step == 0) break;
        consumed += step;
        ++lines;
    }
    return lines;
}

int UIPanelSummary_DrawWrappedText(SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   const char* text,
                                   int x,
                                   int y,
                                   int max_width,
                                   int line_height,
                                   int line_gap,
                                   int max_lines,
                                   SDL_Color color) {
    int lines_drawn = 0;
    int step_y = line_height + line_gap;
    size_t consumed = 0;
    char line[512];
    if (!renderer || !font || !text || !text[0] || max_width <= 0 || max_lines == 0) return 0;
    if (line_height <= 0) line_height = TTF_FontHeight(font);
    if (step_y <= 0) step_y = line_height;

    while (text[consumed] != '\0' && (max_lines < 0 || lines_drawn < max_lines)) {
        size_t step = UIPanelSummary_BuildWrappedLine(font, text + consumed, max_width, line, sizeof(line));
        if (step == 0) break;
        consumed += step;
        if (line[0] != '\0') {
            UIPanelSummary_DrawText(renderer, font, line, x, y + (lines_drawn * step_y), color);
        }
        ++lines_drawn;
    }
    return lines_drawn;
}

void UIPanelSummary_DrawCard(SDL_Renderer* renderer,
                             SDL_Rect rect,
                             SDL_Color fill_color,
                             SDL_Color border_color,
                             SDL_Color accent_color,
                             int accent_height) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    UIPanelVisual_DrawFrame(renderer, rect, fill_color, border_color, 70);
    if (accent_height > 0) {
        UIPanelVisual_DrawAccentBand(renderer, rect, accent_color, 1, accent_height, 220);
    }
}

void UIPanelSummary_DrawDivider(SDL_Renderer* renderer,
                                SDL_Rect rect,
                                int y,
                                int inset_x,
                                SDL_Color color,
                                Uint8 alpha) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    if (inset_x < 0) inset_x = 0;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, alpha);
    SDL_RenderDrawLine(renderer,
                       rect.x + inset_x,
                       y,
                       rect.x + rect.w - inset_x,
                       y);
}
