#include "ImageLoader.h"
#include <iostream>

// 这一行非常重要！它告诉编译器在这里实现 stb_image 的底层代码
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image/stb_image.h"

#include <GLFW/glfw3.h> 

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

ImageLoader::~ImageLoader() {
    Free();
}

void ImageLoader::Free() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }
}

bool ImageLoader::LoadFromFile(const std::string& filename) {
    Free(); // 加载新图前，释放旧图内存

    // 让 stb_image 知道我们要翻转 Y 轴 (OpenGL 的纹理坐标从左下角开始)
    stbi_set_flip_vertically_on_load(true);

    // 读取图片像素数据 (强制要求 RGBA 4个通道)
    unsigned char* image_data = stbi_load(filename.c_str(), &width, &height, &channels, 4);
    if (image_data == nullptr) {
        std::cerr << "[Error] 无法加载图片: " << filename << std::endl;
        return false;
    }

    // 生成并绑定 OpenGL 纹理
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // 设置纹理缩放过滤方式 (线性过滤，让图片缩放时更平滑)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // 设置边缘包裹方式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 将 CPU 里的像素数据上传给 GPU显存
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

    // 释放 CPU 内存 (数据已经进显卡了)
    stbi_image_free(image_data);

    return true;
}