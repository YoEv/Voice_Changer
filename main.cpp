#include <Bela.h>
#include <iostream>
#include <unistd.h>

// 声明在 `render.cpp` 中定义的函数
extern bool setup(BelaContext* context, void* userData);
extern void render(BelaContext* context, void* userData);
extern void cleanup(BelaContext* context, void* userData);

int main(int argc, char *argv[]) {
    BelaInitSettings* settings = Bela_InitSettings_alloc(); // 标准音频设置

    // 设置默认设置
    Bela_defaultSettings(settings);
    settings->setup = setup;
    settings->render = render;
    settings->cleanup = cleanup;

    // 初始化音频设备
    if(Bela_initAudio(settings, 0) != 0) {
        Bela_InitSettings_free(settings);
        std::cerr << "Error: unable to initialize audio" << std::endl;
        return 1;
    }
    Bela_InitSettings_free(settings);

    // 启动音频设备
    if(Bela_startAudio()) {
        std::cerr << "Error: unable to start real-time audio" << std::endl;
        return 1;
    }

    // 运行直到被停止
    while(!Bela_stopRequested()) {
        usleep(100000);
    }

    // 停止音频设备
    Bela_stopAudio();

    // 清理音频资源
    Bela_cleanupAudio();

    // 完成
    return 0;
}
