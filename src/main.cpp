#include <SDL2/SDL.h>
#include <whb/proc.h>
#include <nn/spm/storage.h>
#include <coreinit/filesystem.h>
#include <coreinit/ios.h>
extern "C" {
#include "libexfat/exfat.h"
}
#include "main.hpp"
#include "menu.hpp"

#include <fcntl.h>
#include <unistd.h> 

AppState main_app_state = STATE_MENU;
SDL_Window* main_window;
SDL_Renderer* main_renderer;
SDL_Texture* main_texture;

int init_sdl() {
    printf("Starting SDL...\n");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("Failed to init SDL: %s\n", SDL_GetError());
        return -1;
    }

    main_window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    main_renderer = SDL_CreateRenderer(main_window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(main_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);



    return 0;
}
/*
void get_device_node(char* vol_device_path) {
    FSInit();

    FSClient fs_client;
    FSAddClient(&fs_client, FS_ERROR_FLAG_ALL);

    FSCmdBlock fs_cmdblock;
    FSInitCmdBlock(&fs_cmdblock);

    FSMountSource fs_mount_source;
    FSStatus fs_status = FSGetMountSource(&fs_client, &fs_cmdblock, FS_MOUNT_SOURCE_UNK, &fs_mount_source, FS_ERROR_FLAG_ALL);

    if (fs_status < 0) {
		printf("FSGetMountSource failed: %i\n\n", fs_status);
		return;
	}

    char sdcard_path[17];

    fs_status = FSMount(&fs_client, &fs_cmdblock, &fs_mount_source, vol_device_path, sizeof(sdcard_path), FS_ERROR_FLAG_ALL);
    if (fs_status < 0) {
        printf("Failed to mount SD card: %i\n", fs_status);    
        return;
    }

    strncpy(vol_device_path, sdcard_path, 17);
    printf("Mounted device at: %s\n", vol_device_path);

    FSUnmount(&fs_client, &fs_cmdblock, sdcard_path, FS_ERROR_FLAG_ALL);
    FSDelClient(&fs_client, FS_ERROR_FLAG_ALL);
    FSShutdown();
}

void hex_dump(const void* data, size_t size) {
    const unsigned char* bytes = (const unsigned char*)data;
    for (size_t i = 0; i < size; i += 16) {
        printf("%08zx  ", i);
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size)
                printf("%02x ", bytes[i + j]);
            else
                printf("   ");
        }
        printf(" ");
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size)
                printf("%c", isprint(bytes[i + j]) ? bytes[i + j] : '.');
        }
        printf("\n");
    }
}

void mount_exfat(char* raw_device_path)  {
    exfat exfat_struct;

    if (!exfat_mount(&exfat_struct, raw_device_path, "")) {
        printf("Mounted exFAT partition!\n");
    } else {
        printf("Failed to mount exFAT volume!\n");

        goto error;
    }

    exfat_unmount(&exfat_struct);

    error: return;
}

void get_usb_list() {
    nn::spm::StorageListItem items[10];
    int32_t count = nn::spm::GetStorageList(items, 10);

    if (count <= 0) {
        printf("No storage devices found.\n");
        return;
    }

    for (int i = 0; i < count; ++i) {
        bool isPcFormatted = false;
        nn::spm::IsStorageMaybePcFormatted(&isPcFormatted, &items[i].index);

        if (isPcFormatted) {
            nn::spm::StorageInfo item;
            nn::spm::GetStorageInfo(&item, &items[i].index);

            printf("Found a USB!\n");
            printf("Path: %s\n", item.path);
            printf("Connection: %s\n", item.connection_type_string);
            printf("Format: %s\n", item.format_string);

            if (strcmp(item.format_string, "raw") == 0) {
                printf("Warning: Storage is not formatted in a recognized filesystem (wfs).\n");
            }

            get_device_node(item.path);
            //mount_exfat(item.path);

            char buffer[512];
            if (nn::spm::ReadRawStorageHead512(&items[i].index, buffer) == 0) {
                hex_dump(buffer, 512);
            } else {
                printf("Failed to read raw storage on device %i.\n", i);
            }
        } else {
            printf("USB is either WFS or corrupted!\n");
        }
    }
}
*/
int main(int argc, char **argv) {
    WHBProcInit();

    printf("=======================BEGIN=======================\n");

    // nn::spm::Initialize();
    // get_usb_list();

    if (init_sdl() != 0) return -1;

    ui_init(main_window, main_renderer, main_texture, &main_app_state);

    while (WHBProcIsRunning()) {
        ui_render();
        SDL_RenderPresent(main_renderer);
    }
    ui_shutodwn();

    SDL_DestroyTexture(main_texture);
    SDL_DestroyRenderer(main_renderer);
    SDL_DestroyWindow(main_window);
    SDL_Quit();

    // nn::spm::Finalize();

    printf("=======================END=======================\n");

    WHBProcShutdown();
    return 0;
}
