#include <dirent.h>
#include <vector>
#include <string>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "config.hpp"
#include "menu.hpp"

void scan_directory(const char* path, std::vector<std::string>& video_files) {
    video_files.clear();
    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string name(ent->d_name);
        if (name.length() > 4 && name.substr(name.length() - 4) == ".mp4") {
            video_files.push_back(name);
        }
    }
    closedir(dir);
}

std::string format_time(int seconds) {
    int mins = seconds / 60;
    int secs = seconds % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    return std::string(buffer);
}

void render_file_browser(SDL_Renderer* renderer, TTF_Font* font, int selected_index, const std::vector<std::string>& video_files) {
    // Set background color and clear screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Color white = {255, 255, 255};
    SDL_Color yellow = {255, 255, 0};

    int y = 20;
    const int padding = 10;
    const int start_x = 40;

    for (int i = 0; i < static_cast<int>(video_files.size()); ++i) {
        std::string display_str = video_files[i];

        // Optional: truncate long filenames
        const size_t max_chars = 50;
        if (display_str.length() > max_chars) {
            display_str = display_str.substr(0, max_chars - 3) + "...";
        }

        // Render text surface
        SDL_Surface* text_surface = TTF_RenderText_Blended(
            font,
            display_str.c_str(),
            (i == selected_index) ? yellow : white
        );

        if (!text_surface) {
            continue; // Skip this item if rendering failed
        }

        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
        if (!text_texture) {
            SDL_FreeSurface(text_surface);
            continue;
        }

        // Stop rendering if items go off-screen
        if (y + text_surface->h > SCREEN_WIDTH) {
            SDL_FreeSurface(text_surface);
            SDL_DestroyTexture(text_texture);
            break;
        }

        SDL_Rect dst_rect = {start_x, y, text_surface->w, text_surface->h};
        SDL_RenderCopy(renderer, text_texture, nullptr, &dst_rect);

        y += text_surface->h + padding;

        SDL_FreeSurface(text_surface);
        SDL_DestroyTexture(text_texture);
    }

    SDL_RenderPresent(renderer);
}

void render_video_hud(SDL_Renderer* renderer, TTF_Font* font, SDL_Texture* texture, int current_pts_seconds, int duration_seconds) {
    SDL_RenderCopy(renderer, texture, NULL, NULL);

    std::string time_str = format_time(current_pts_seconds) + " / " + format_time(duration_seconds);
    SDL_Color white = {255, 255, 255};

    SDL_Surface* text_surface = TTF_RenderText_Blended(font, time_str.c_str(), white);
    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);

    int text_w, text_h;
    SDL_QueryTexture(text_texture, NULL, NULL, &text_w, &text_h);
    SDL_Rect dst_rect = {10, SCREEN_WIDTH - text_h - 10, text_w, text_h};

    SDL_RenderCopy(renderer, text_texture, NULL, &dst_rect);

    SDL_FreeSurface(text_surface);
    SDL_DestroyTexture(text_texture);
}