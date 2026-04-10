#pragma once
#include <string>
// 引入 GLFW 来获取 OpenGL 的数据类型 (GLuint)
#include <GLFW/glfw3.h> 

class ImageLoader {
private:
    GLuint textureID = 0;
    int width = 0;
    int height = 0;
    int channels = 0;

public:
    ImageLoader() = default;
    ~ImageLoader();

    // 从硬盘加载图片并生成 OpenGL 纹理
    bool LoadFromFile(const std::string& filename);
    
    // 清理纹理内存
    void Free();

    // Getters，供 ImGui 渲染时使用
    GLuint getTextureID() const { return textureID; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    bool isLoaded() const { return textureID != 0; }
};