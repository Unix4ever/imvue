#include <gtest/gtest.h>

#include "imgui.h"

int main(int argc, char **argv)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // Build atlas
    unsigned char* tex_pixels = NULL;
    int tex_w, tex_h;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

    testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    ImGui::DestroyContext();
    return result;
}
