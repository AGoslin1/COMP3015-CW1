
#include "texture.h"
#include "stb/stb_image.h"
#include "glutils.h"
#include <iostream>

/*static*/
GLuint Texture::loadTexture(const std::string & fName) {
    int width = 0, height = 0;
    unsigned char* data = Texture::loadPixels(fName, width, height);
    GLuint tex = 0;

    if (data != nullptr && width > 0 && height > 0) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        Texture::deletePixels(data);
    }
    else {
        // Failed to load image file — create a safe 1x1 white fallback texture
        if (data) Texture::deletePixels(data);
        std::cerr << "Texture::loadTexture warning: failed to load '" << fName << "' - creating 1x1 white fallback\n";
        unsigned char pixel[4] = { 255, 255, 255, 255 };
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    return tex;
}

void Texture::deletePixels(unsigned char* data) {
    stbi_image_free(data);
}

unsigned char* Texture::loadPixels(const std::string& fName, int& width, int& height, bool flip) {
    int bytesPerPix;
    stbi_set_flip_vertically_on_load(flip);
    unsigned char* data = stbi_load(fName.c_str(), &width, &height, &bytesPerPix, 4);
    return data;
}

GLuint Texture::loadCubeMap(const std::string& baseName, const std::string& extension) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texID);

    const char* suffixes[] = { "posx", "negx", "posy", "negy", "posz", "negz" };
    int w = 0, h = 0;
    bool anyFail = false;
    unsigned char* facesData[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    // Load all faces first, detect failure
    for (int i = 0; i < 6; ++i) {
        std::string texName = baseName + "_" + suffixes[i] + extension;
        facesData[i] = Texture::loadPixels(texName, w, h, false);
        if (!facesData[i] || w <= 0 || h <= 0) {
            std::cerr << "Texture::loadCubeMap warning: failed to load '" << texName << "'\n";
            anyFail = true;
            break;
        }
    }

    if (!anyFail) {
        // Allocate immutable storage and upload faces
        glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA8, w, h);
        for (int i = 0; i < 6; ++i) {
            glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, facesData[i]);
            Texture::deletePixels(facesData[i]);
            facesData[i] = nullptr;
        }
    }
    else {
        // Free any partially loaded faces
        for (int i = 0; i < 6; ++i) {
            if (facesData[i]) Texture::deletePixels(facesData[i]);
            facesData[i] = nullptr;
        }
        // Create a 1x1 white cubemap fallback to avoid GL errors
        std::cerr << "Texture::loadCubeMap info: creating 1x1 white cubemap fallback for base '" << baseName << "'\n";
        unsigned char pixel[4] = { 255, 255, 255, 255 };
        for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return texID;
}

GLuint Texture::loadHdrCubeMap(const std::string& baseName) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texID);

    const char* suffixes[] = { "posx", "negx", "posy", "negy", "posz", "negz" };
    int w = 0, h = 0;
    bool anyFail = false;
    float* facesData[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    for (int i = 0; i < 6; ++i) {
        std::string texName = baseName + "_" + suffixes[i] + ".hdr";
        facesData[i] = stbi_loadf(texName.c_str(), &w, &h, NULL, 3);
        if (!facesData[i] || w <= 0 || h <= 0) {
            std::cerr << "Texture::loadHdrCubeMap warning: failed to load '" << texName << "'\n";
            anyFail = true;
            break;
        }
    }

    if (!anyFail) {
        glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGB32F, w, h);
        for (int i = 0; i < 6; ++i) {
            glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 0, 0, w, h, GL_RGB, GL_FLOAT, facesData[i]);
            stbi_image_free(facesData[i]);
            facesData[i] = nullptr;
        }
    }
    else {
        for (int i = 0; i < 6; ++i) {
            if (facesData[i]) stbi_image_free(facesData[i]);
            facesData[i] = nullptr;
        }
        // Create 1x1 white HDR fallback (RGB32F)
        std::cerr << "Texture::loadHdrCubeMap info: creating 1x1 white HDR cubemap fallback for base '" << baseName << "'\n";
        float pixel[3] = { 1.0f, 1.0f, 1.0f };
        for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB32F, 1, 1, 0, GL_RGB, GL_FLOAT, pixel);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return texID;
}
