#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>

int main(int, char**) {
    SDL_Log("genesisEmu (ImGui %s)", ImGui::GetVersion());
    return 0;
}
